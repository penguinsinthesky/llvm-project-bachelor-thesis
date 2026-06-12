#include "llvm/Transforms/Instrumentation/XRayLoopInstrument.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Transforms/Utils/XRayCustomRegionInstrumentation.h"

namespace llvm {
static void
findBodyStartBlocks(const Loop &L, const PostDominatorTree &PD,
                    SmallVectorImpl<BasicBlock *> &BodyStartBlocks) {
  const auto *LatchNode = PD.getNode(L.getLoopLatch());

  // these are the leaf blocks in the post-dominator tree (which are still in
  // the loop)
  SmallVector<const DomTreeNode *, 8> Stack;
  Stack.push_back(LatchNode);
  while (!Stack.empty()) {
    auto *Node = Stack.pop_back_val();
    auto *Block = Node->getBlock();

    if (Node->isLeaf() && L.contains(Block)) {
      BodyStartBlocks.push_back(Block);
      continue;
    }

    // push all children
    Stack.append(LatchNode->begin(), LatchNode->end());
  }
}
} // namespace llvm

llvm::PreservedAnalyses
llvm::XRayLoopInstrumentPass::run(Function &F, FunctionAnalysisManager &FAM) {
  bool Modified = false;

  const PostDominatorTree &PD = FAM.getResult<PostDominatorTreeAnalysis>(F);
  const LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);

  for (const auto *L : LI) {
    const std::optional<StringRef> RegionName = getRegionName(*L);
    if (!RegionName.has_value()) {
      // not annotated
      continue;
    }

    instrumentLoop(*L, F, PD, RegionName.value());
    Modified = true;
  }

  return Modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
std::optional<llvm::StringRef>
llvm::XRayLoopInstrumentPass::getRegionName(const Loop &L) {
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
void llvm::XRayLoopInstrumentPass::instrumentLoop(const Loop &L,
                                                  const Function &F,
                                                  const PostDominatorTree &PD,
                                                  const StringRef RegionName) {
  SmallVector<BasicBlock *, 1> BodyStartBlocks;
  SmallVector<BasicBlock *, 2> BodyEndBlocks;

  findBodyStartBlocks(L, PD, BodyStartBlocks);
  L.getLoopLatches(BodyEndBlocks);

  auto Inserter = XRayCustomRegionInserter::forLoop(L, RegionName);

  IRBuilder Builder(F.getContext());

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