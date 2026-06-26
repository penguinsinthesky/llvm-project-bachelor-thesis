#include "llvm/CodeGen/XRayCustomRegionFiltering.h"

#include "../../../compiler-rt/include/xray/xray_custom_region_kind.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionAnalysis.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/InitializePasses.h"
#include "llvm/Transforms/Utils/XRayCustomRegionInstrumentation.h"

using namespace llvm;

namespace {
struct CustomRegionInstructions {
  SmallVector<MachineInstr *, 2> EnterProbes;
  SmallVector<MachineInstr *, 2> ExitProbes;
  SmallPtrSet<const MachineBasicBlock *, 8> ForwardReachable;
  SmallPtrSet<const MachineBasicBlock *, 8> BackwardReachable;

  CustomRegionInstructions() = default;

  /// invalidates reachability sets
  uint64_t countInstructions() {
    // now ForwardReachable only contains blocks that are contained in a path
    // from the entry to the exit probe
    set_intersect(ForwardReachable, BackwardReachable);

    SmallPtrSet<const MachineBasicBlock *, 8> &BlocksInRegion =
        ForwardReachable;

    uint64_t Count = 0;

    // for each block containing an entry probe, only count those instructions
    // after it.
    // if the matching exit probe is in the same block, do not count it and any
    // instructions after it.
    for (const auto *EnterProbe : EnterProbes) {
      const MachineBasicBlock *ParentBlock = EnterProbe->getParent();
      bool AfterEnter = false;
      for (const auto &EnterSibling : *ParentBlock) {
        if (AfterEnter &&
            EnterSibling.getOpcode() ==
                TargetOpcode::PATCHABLE_CUSTOM_REGION_EXIT &&
            EnterSibling.getOperand(0).getGlobal() ==
                EnterProbe->getOperand(0).getGlobal()) {
          // if we encounter this region's exit in this block already, stop
          // counting in this block
          break;
        }

        if (AfterEnter) {
          Count++;
        }

        if (&EnterSibling == EnterProbe) {
          AfterEnter = true;
        }
      }

      // do not count twice
      BlocksInRegion.erase(ParentBlock);
    }

    // for each block containing an exit probe, only count those instructions
    // before it.
    // if the matching enter probe is in the same block, do not count it and
    // anything before.
    for (const auto *ExitProbe : ExitProbes) {
      const MachineBasicBlock *ParentBlock = ExitProbe->getParent();
      bool BeforeExit = false;

      // iterate in reverse to handle this region's enter probe correctly
      for (const auto &ExitSibling : reverse(ParentBlock->instrs())) {
        if (BeforeExit &&
            ExitSibling.getOpcode() ==
                TargetOpcode::PATCHABLE_CUSTOM_REGION_ENTER &&
            ExitSibling.getOperand(0).getGlobal() ==
                ExitProbe->getOperand(0).getGlobal()) {
          // if we encounter this region's enter probe, stop counting in this
          // block
          break;
        }

        if (BeforeExit) {
          Count++;
        }

        if (&ExitSibling == ExitProbe) {
          BeforeExit = true;
        }
      }

      // do not count twice
      BlocksInRegion.erase(ParentBlock);
    }

    // TODO also filter out other XRay pseudo instructions? (also in existing
    // XRayInstrumentation.cpp)

    // count instructions in all other blocks
    for (const auto *Block : BlocksInRegion) {
      Count += Block->size();
    }

    return Count;
  }
};

class XRayInlinedRegionFilter {
public:
  bool run(MachineFunction &MF) const {
    DenseMap<XRayCustomRegionInfo, CustomRegionInstructions> RegionInstructions;

    for (auto &Block : MF) {
      for (auto &Inst : Block) {
        if (Inst.getOpcode() == TargetOpcode::PATCHABLE_CUSTOM_REGION_ENTER) {
          auto *RegionInfoGlobal =
              cast<GlobalVariable>(Inst.getOperand(0).getGlobal());
          auto RegionInfo =
              XRayCustomRegionInfo::fromRegionInfoGlobal(RegionInfoGlobal);
          if (RegionInfo.getRegionKind() !=
              XRayCustomRegionKind::INLINED_FUNCTION) {
            // not an inlined function
            continue;
          }

          auto &Instructions = RegionInstructions[RegionInfo];

          Instructions.EnterProbes.push_back(&Inst);
          // do a forward search from here
          for (const auto *Succ : depth_first(&Block)) {
            Instructions.ForwardReachable.insert(Succ);
          }
        } else if (Inst.getOpcode() ==
                   TargetOpcode::PATCHABLE_CUSTOM_REGION_EXIT) {
          auto *RegionInfoGlobal =
              cast<GlobalVariable>(Inst.getOperand(0).getGlobal());
          auto RegionInfo =
              XRayCustomRegionInfo::fromRegionInfoGlobal(RegionInfoGlobal);
          if (RegionInfo.getRegionKind() !=
              XRayCustomRegionKind::INLINED_FUNCTION) {
            // not an inlined function
            continue;
          }

          auto &Instructions = RegionInstructions[RegionInfo];

          Instructions.ExitProbes.push_back(&Inst);
          // do a backwards search from here
          for (const auto *Pred : inverse_depth_first(&Block)) {
            Instructions.BackwardReachable.insert(Pred);
          }
        }
      }
    }

    bool Modified = false;

    // TODO respect loops

    for (auto &[RegionInfo, Instructions] : RegionInstructions) {
      const auto InlinedFunctionInfo = RegionInfo.getInlinedFunctionInfo();
      const uint64_t Threshold = InlinedFunctionInfo.getInstructionThreshold();
      const uint64_t InstCount = Instructions.countInstructions();

      if (InstCount < Threshold) {
        // region does not contain enough instructions -> remove again
        for (auto *Probe : Instructions.EnterProbes) {
          Probe->eraseFromParent();
        }

        for (auto *Probe : Instructions.ExitProbes) {
          Probe->eraseFromParent();
        }

        Modified = true;
      }
    }

    return Modified;
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
