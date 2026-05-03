#include "llvm/Transforms/Instrumentation/XRayInlineInstrument.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/EscapeEnumerator.h"
#include "llvm/XRay/CustomRegionInfo.h"

namespace llvm {

PreservedAnalyses
XRayPreInlineInstrumentPass::run(Module &M,
                                 [[maybe_unused]] ModuleAnalysisManager &AM) {

  bool Modified = false;

  for (auto &F : M.functions()) {
    if (!shouldInstrument(F)) {
      continue;
    }

    insertInstructions(F);
    Modified = true;
  }

  return Modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
bool XRayPreInlineInstrumentPass::shouldInstrument(const Function &F) {
  if (F.isDeclaration()) {
    // never instrument declarations
    return false;
  }

  const auto InstrAttr = F.getFnAttribute("function-instrument");

  const bool AlwaysInstrument = InstrAttr.isStringAttribute() &&
                                InstrAttr.getValueAsString() == "xray-always";
  const bool NeverInstrument = InstrAttr.isStringAttribute() &&
                               InstrAttr.getValueAsString() == "xray-never";

  if (NeverInstrument && !AlwaysInstrument) {
    // always "beats" never if they are both present
    return false;
  }

  // TODO check skip-enter/exit attributes, bundles

  // TODO apply some heuristic now

  return true;
}

void XRayPreInlineInstrumentPass::insertInstructions(Function &F) const {
  LLVMContext &Ctx = F.getContext();

  const xray::XRayCustomRegionInfo RegionInfo =
      xray::XRayCustomRegionInfo::inlinedFunction(
          xray::InlinedFunctionRegionInfo{F.getName(), &F});

  MDNode *RegionMD = RegionInfo.createMetadata(Ctx);
  Value *MDValue = MetadataAsValue::get(Ctx, RegionMD);

  // prepend region enter intrinsic to entry block
  IRBuilder Builder(&*F.getEntryBlock().begin());
  Builder.CreateIntrinsic(Intrinsic::xray_customregionenter, {MDValue});

  // append region exit intrinsic to every exit block's end
  // (but before terminator instruction)
  EscapeEnumerator Exits(F);

  while (IRBuilder<> *ExitBuilder = Exits.Next()) {
    ExitBuilder->CreateIntrinsic(Intrinsic::xray_customregionexit, {MDValue});
  }
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

    const Function *ParentFunction = I.getFunction();

    assert(I.arg_size() == 1 &&
           "XRay custom region intrinsics must have exactly one argument");
    const Metadata *MD =
        cast<MetadataAsValue>(I.getArgOperand(0))->getMetadata();

    const xray::XRayCustomRegionInfo RegionMD =
        xray::XRayCustomRegionInfo::fromMetadata(MD);

    if (RegionMD.getKind() != xray::CustomRegionKind::INLINED_FUNCTION) {
      // do not touch probes that are not meant for inlining
      return;
    }

    const std::optional<Function *> OriginalFunction =
        RegionMD.getInlinedFunctionInfo().OriginalFunction;

    if (OriginalFunction.has_value() &&
        OriginalFunction.value() == ParentFunction) {
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

  return Visitor.RemovalCandidates.empty() ? PreservedAnalyses::all()
                                           : PreservedAnalyses::none();
}
} // namespace llvm
