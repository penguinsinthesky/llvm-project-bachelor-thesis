#include "llvm/CodeGen/XRayCustomRegionFiltering.h"
#include "llvm/ADT/STLExtras.h"

#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/EquivalenceClasses.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/InitializePasses.h"
#include "llvm/Transforms/Utils/XRayCustomRegionInstrumentation.h"

using namespace llvm;

namespace {

typedef unsigned RegionInstanceId;

struct RegionInstance {
  XRayCustomRegionInfo RegionInfo;
  SmallPtrSet<MachineInstr *, 2> EnterProbes;
  SmallPtrSet<MachineInstr *, 2> ExitProbes;
  SmallPtrSet<const MachineInstr *, 16> Instructions;

  explicit RegionInstance(const XRayCustomRegionInfo RegionInfo,
                          MachineInstr *FirstEnter)
      : RegionInfo(RegionInfo), Invalidated(false) {
    EnterProbes.insert(FirstEnter);
  }

  bool join(const RegionInstance &Other) {
    assert((RegionInfo == Other.RegionInfo) &&
           "Cannot join two region instance of different regions");

    assert((!Invalidated && !Other.Invalidated) &&
           "Cannot join invalidated region instances");

    return set_union(EnterProbes, Other.EnterProbes) ||
           set_union(ExitProbes, Other.ExitProbes) ||
           set_union(Instructions, Other.Instructions);
  }

  void invalidate() {
    assert(
        (!Invalidated) &&
        "Cannot invalidate region instance which has already been invalidated");
    Invalidated = true;
  }

  // prevent accidental copying
  RegionInstance(const RegionInstance &) = delete;
  RegionInstance &operator=(const RegionInstance &) = delete;

  RegionInstance(RegionInstance &&) noexcept = default;
  RegionInstance &operator=(RegionInstance &&) noexcept = default;

private:
  bool Invalidated;

  friend class RegionInstanceStorage;
};

class RegionInstanceStorage {
public:
  RegionInstanceId newInstance(const XRayCustomRegionInfo RegionInfo,
                               MachineInstr *FirstEnter) {
    const RegionInstanceId Id = Storage.size();
    Storage.emplace_back(RegionInfo, FirstEnter);
    Eq.insert(Id);

    return Id;
  }

  RegionInstance &operator[](const RegionInstanceId Id) {
    const auto Leader = Eq.findLeader(Id);
    const size_t Index = *Leader;
    return Storage[Index];
  }

  const RegionInstance &operator[](const RegionInstanceId Id) const {
    return const_cast<RegionInstanceStorage *>(this)->operator[](Id);
  }

  void join(const RegionInstanceId A, const RegionInstanceId B) {
    if (A == B) {
      // this can happen in diamond shaped CFGs where one entry probe is
      // followed by one exit probe with branching in between.
      return;
    }

    const RegionInstanceId ALeader = Eq.getLeaderValue(A);
    const RegionInstanceId BLeader = Eq.getLeaderValue(B);

    Storage[ALeader].join(Storage[BLeader]); // join both instances into A

    const auto UnionLeader = Eq.unionSets(A, B);

    Storage[BLeader].invalidate(); // B is no longer valid

    assert(*UnionLeader == ALeader);
  }

  template <typename FuncT> void forEachInstance(FuncT Consumer) const {
    for (const auto *I : Eq) {
      if (!I->isLeader()) {
        continue;
      }

      const RegionInstanceId Index = I->getData();
      const RegionInstance &Instance = Storage[Index];
      assert(!Instance.Invalidated && "Invalidated region cannot be leader");
      Consumer(Instance);
    }
  }

private:
  EquivalenceClasses<RegionInstanceId> Eq;
  SmallVector<RegionInstance, 4> Storage;
};

// TODO extend to stack to support nesting
using DataflowFact = SmallDenseMap<XRayCustomRegionInfo, RegionInstanceId>;

class XRayInlinedRegionFilter {
public:
  bool run(MachineFunction &MF) const {
    RegionInstanceStorage InstanceStorage;

    SmallDenseMap<const MachineBasicBlock *, DataflowFact> ROuts;

    // initialize R_out with empty maps for all blocks
    for (const auto &Block : MF) {
      ROuts.insert(std::make_pair(&Block, DataflowFact()));
    }

    ReversePostOrderTraversal<MachineFunction *> RPO(&MF);

    for (auto *Block : RPO) {
      DataflowFact RIn;

      if (Block->pred_size() > 0) {
        // merge incoming regions

        // initialize with incoming regions from first pred
        RIn = ROuts[*Block->pred_begin()];

        // skip first pred here
        for (const auto *Pred : drop_begin(Block->predecessors(), 1)) {
          const auto &PredRegions = ROuts[Pred];
          for (const auto &[RegionInfo, InstanceId] : PredRegions) {
            // since regions are well-formed, we expect all incoming edges to
            // have the same open regions
            auto SameRegion = RIn.find(RegionInfo);

            assert((SameRegion != RIn.end()) &&
                   "Incoming edge has an open region which others don't have");

            // join incoming instances
            InstanceStorage.join(SameRegion->second, InstanceId);
          }
        }
      }

      for (auto &I : *Block) {
        // handle entry/exit of region
        if (I.getOpcode() == TargetOpcode::PATCHABLE_CUSTOM_REGION_ENTER) {
          // new instance of region starts here
          const auto RegionInfo = XRayCustomRegionInfo::fromRegionInfoGlobal(
              cast<GlobalVariable>(I.getOperand(0).getGlobal()));

          RegionInstanceId NewInstanceId =
              InstanceStorage.newInstance(RegionInfo, &I);

          const auto [_, Inserted] =
              RIn.insert(std::make_pair(RegionInfo, NewInstanceId));

          assert(Inserted && "Encountered entry probe for already open region");
        } else if (I.getOpcode() ==
                   TargetOpcode::PATCHABLE_CUSTOM_REGION_EXIT) {
          // instance of region is closed here
          const auto RegionInfo = XRayCustomRegionInfo::fromRegionInfoGlobal(
              cast<GlobalVariable>(I.getOperand(0).getGlobal()));

          auto InstanceIt = RIn.find(RegionInfo);
          assert((InstanceIt != RIn.end()) &&
                 "Exited region that was currently not open");

          RegionInstance &Instance = InstanceStorage[InstanceIt->second];

          Instance.ExitProbes.insert(&I); // record exit probe
          RIn.erase(InstanceIt);          // remove from currently open regions
        } else {
          // add instructions to sets of all currently open regions
          // (does not include probe instructions themselves)
          for (const auto [_, InstanceId] : RIn) {
            RegionInstance &Instance = InstanceStorage[InstanceId];
            Instance.Instructions.insert(&I);
          }
        }
      }

      const auto PrevROut = ROuts.find(Block);
      assert((PrevROut != ROuts.end()) &&
             "Updating R_out for block with no previous R_out");

      // update ROut for this block
      PrevROut->second = std::move(RIn);
    }

    InstanceStorage.forEachInstance([](const RegionInstance &Instance) {
      const size_t InstructionCount = Instance.Instructions.size();

      errs() << "Instance of region \"" << Instance.RegionInfo.getRegionName()
             << "\": " << InstructionCount << " instructions\n";
    });

    return false; // TODO actually remove regions
  }
};

class XRayInlinedRegionFilterLegacy : public MachineFunctionPass {
public:
  static char ID;

  XRayInlinedRegionFilterLegacy() : MachineFunctionPass(ID) {}

protected:
  bool runOnMachineFunction(MachineFunction &MF) override {
    return XRayInlinedRegionFilter().run(MF);
  }
};

} // namespace

namespace llvm {
PreservedAnalyses
XRayInlinedRegionFilterPass::run(MachineFunction &MF,
                                 MachineFunctionAnalysisManager &AM) {
  return XRayInlinedRegionFilter().run(MF) ? PreservedAnalyses::none()
                                           : PreservedAnalyses::all();
}
} // namespace llvm

char XRayInlinedRegionFilterLegacy::ID = 0;
char &llvm::XRayInlinedRegionsFilterID = XRayInlinedRegionFilterLegacy::ID;
INITIALIZE_PASS_BEGIN(
    XRayInlinedRegionFilterLegacy, "xray-inlined-region-filter",
    "Remove XRay custom regions originating from too short functions", false,
    false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_END(
    XRayInlinedRegionFilterLegacy, "xray-inlined-region-filter",
    "Remove XRay custom regions originating from too short functions", false,
    false)
