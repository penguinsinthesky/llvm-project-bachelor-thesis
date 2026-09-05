#include "llvm/Transforms/Instrumentation/XRayInlineInstrument.h"

#include "../../../../compiler-rt/include/xray/xray_custom_region_kind.h" // FIXME
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/EscapeEnumerator.h"
#include "llvm/Transforms/Utils/XRayCustomRegionInstrumentation.h"

namespace llvm {

static void encloseInCustomRegion(Function &F) {
  auto Inserter = XRayCustomRegionInserter::forInlinedFunction(F);

  // prepend region enter intrinsic to entry block
  IRBuilder Builder(&*F.getEntryBlock().begin());
  Inserter.insertEnter(Builder);

  // append region exit intrinsic to every exit block's end
  // (but before terminator instruction)
  EscapeEnumerator Exits(F);

  while (IRBuilder<> *ExitBuilder = Exits.Next()) {
    Inserter.insertExit(*ExitBuilder);
  }
}

PreservedAnalyses XRayPreInlineAutoInstrumentPass::run(
    Module &M, [[maybe_unused]] ModuleAnalysisManager &AM) {

  bool Modified = false;

  for (auto &F : M.functions()) {
    if (F.isDeclaration()) {
      // never instrument declarations
      continue;
    }

    const auto InstrAttr = F.getFnAttribute("function-instrument");

    const bool AlwaysInstrument = InstrAttr.isStringAttribute() &&
                                  InstrAttr.getValueAsString() == "xray-always";
    const bool NeverInstrument = InstrAttr.isStringAttribute() &&
                                 InstrAttr.getValueAsString() == "xray-never";

    if (NeverInstrument && !AlwaysInstrument) {
      // always "beats" never if they are both present
      continue;
    }

    // TODO check bundle

    encloseInCustomRegion(F);
    Modified = true;
  }

  return Modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

PreservedAnalyses
XRayPreInlineInstrumentIfAlwaysPass::run(Module &M, ModuleAnalysisManager &AM) {
  bool Modified = false;

  for (auto &F : M.functions()) {
    const auto InstrAttr = F.getFnAttribute("function-instrument");

    const bool AlwaysInstrument = InstrAttr.isStringAttribute() &&
                                  InstrAttr.getValueAsString() == "xray-always";

    if (F.isDeclaration() || !AlwaysInstrument) {
      // if function has no body or is not annotated with
      // xray_always_instrument, don't touch it
      continue;
    }

    encloseInCustomRegion(F);
    Modified = true;
  }

  return Modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

namespace {

struct RemoveOwnIntrinsicVisitor : InstVisitor<RemoveOwnIntrinsicVisitor> {

  std::vector<CallInst *> RemovalCandidates;

  void visitIntrinsicInst(IntrinsicInst &I) {
    if (I.getIntrinsicID() != Intrinsic::xray_customregionenter &&
        I.getIntrinsicID() != Intrinsic::xray_customregionexit) {
      // instruction does not call our intrinsics
      return;
    }

    const auto RegionInfo = XRayCustomRegionInfo::fromIntrinsicCall(I);

    if (RegionInfo.getRegionKind() != XRayCustomRegionKind::INLINED_FUNCTION) {
      // do not touch probes that are not meant for inlining
      return;
    }

    const Function *ParentFunction = I.getFunction();
    const Function *OriginalFunction =
        RegionInfo.getInlinedFunctionInfo().getOriginalFunction();

    if (OriginalFunction != nullptr && OriginalFunction == ParentFunction) {
      // this call belongs to the function we are already in
      // if the original function does not exist, this instruction cannot be in
      // it
      RemovalCandidates.push_back(&I);
    }
  }
};
} // namespace

PreservedAnalyses XRayPostInlinePurgePass::run(Module &M,
                                               ModuleAnalysisManager &AM) {
  // visitor collects all removal candidates
  RemoveOwnIntrinsicVisitor Visitor;
  Visitor.visit(M);

  // actually remove calls
  for (auto *I : Visitor.RemovalCandidates) {
    I->eraseFromParent();
  }

  // left over globals will be dealt with by gl

  return Visitor.RemovalCandidates.empty() ? PreservedAnalyses::all()
                                           : PreservedAnalyses::none();
}
} // namespace llvm
