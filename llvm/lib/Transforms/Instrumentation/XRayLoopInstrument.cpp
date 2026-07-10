#include "llvm/Transforms/Instrumentation/XRayLoopInstrument.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils/XRayCustomRegionInstrumentation.h"

namespace llvm {

static std::optional<StringRef> getRegionName(const Loop &L) {
  if (const MDNode *ID = L.getLoopID()) {
    for (const Metadata *Op : ID->operands()) {
      if (const auto *OpNode = dyn_cast<MDNode>(Op)) {
        if (OpNode->getNumOperands() == 2 &&
            OpNode->getOperand(0).equalsStr("llvm.loop.xray.instrument")) {
          if (const MDString *RegionName =
                  dyn_cast<MDString>(OpNode->getOperand(1))) {
            return std::make_optional(RegionName->getString());
          }

          reportFatalInternalError(
              "\"llvm.loop.xray.instrument\" loop metadata node must contain a "
              "string as second operand");
        }
      }
    }
  }

  return std::nullopt;
}

static void
findBodyStartBlocks(const Loop &L,
                    SmallVectorImpl<BasicBlock *> &BodyStartBlocks) {
  BasicBlock *Header = L.getHeader();

  if (L.isLoopExiting(Header)) {
    // The header exits the loop and is therefore not part of the body.
    // This is found with for and while style loops where the header evaluates
    // the condition. This also applies to one-block loops where the header does
    // exit the loop but since it is a successor of itself (and contained in the
    // loop), it will be added here.

    for (auto *Succ : successors(Header)) {
      if (L.contains(Succ)) {
        BodyStartBlocks.push_back(Succ);
      }
    }
  } else {
    // The header is part of the body (e.g. for do-while style loops).
    BodyStartBlocks.push_back(Header);
  }
}

static void findEndBlocks(const Loop &L,
                          SmallVectorImpl<BasicBlock *> &EndBlocks) {
  const BasicBlock *Header = L.getHeader();
  BasicBlock *Latch = L.getLoopLatch();

  // the latch is a loop exit
  EndBlocks.push_back(Latch);

  // all exiting blocks are end blocks (can be caused by a break)
  SmallVector<BasicBlock *, 2> ExitingBlocks;
  L.getExitingBlocks(ExitingBlocks);

  // only add latch if not in the list already
  for (auto *ExitingBlock : ExitingBlocks) {
    // latch block already recorded
    // no unconditional loop exit expected in header
    if (ExitingBlock == Latch || ExitingBlock == Header)
      continue;

    EndBlocks.push_back(ExitingBlock);
  }
}

static void instrumentLoopBody(const Loop &L, const Twine &RegionName) {
  SmallVector<BasicBlock *, 1> BodyStartBlocks;
  SmallVector<BasicBlock *, 2> BodyEndBlocks;

  findBodyStartBlocks(L, BodyStartBlocks);
  findEndBlocks(L, BodyEndBlocks);

  auto Inserter = XRayCustomRegionInserter::forLoop(L, RegionName);

  // create builder; initial insert position doesn't matter, will be set
  // properly
  IRBuilder Builder(L.getHeader());

  for (auto *StartBlock : BodyStartBlocks) {
    // insert enter probe at beginning of the block
    Builder.SetInsertPoint(StartBlock->getFirstInsertionPt());
    Inserter.insertEnter(Builder);
  }

  for (auto *EndBlock : BodyEndBlocks) {
    // insert exit probe before terminator
    Builder.SetInsertPoint(EndBlock->getTerminator());
    Inserter.insertExit(Builder);
  }
}

PreservedAnalyses XRayLoopInstrumentPass::run(Loop &L, LoopAnalysisManager &,
                                              LoopStandardAnalysisResults &,
                                              LPMUpdater &) {
  const std::optional<StringRef> RegionName = getRegionName(L);
  if (!RegionName.has_value()) {
    // not annotated
    return PreservedAnalyses::all();
  }

  instrumentLoopBody(L, RegionName.value());

  return PreservedAnalyses::none();
}

PreservedAnalyses
XRayOuterLoopInstrumentPass::run(Function &F, FunctionAnalysisManager &FAM) {
  const auto &LI = FAM.getResult<LoopAnalysis>(F);

  bool Modified = false;
  int I = 0;

  // iterating over LoopInfo yields outer loops only
  for (const auto *OuterLoop : LI) {
    if (getRegionName(*OuterLoop).has_value()) {
      // loop is explicitly instrumented, don't touch here
      continue;
    }

    instrumentLoopBody(*OuterLoop,
                       "<outer-loop-" + Twine(I++) + ">" + F.getName());

    Modified = true;
  }

  return Modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

} // namespace llvm
