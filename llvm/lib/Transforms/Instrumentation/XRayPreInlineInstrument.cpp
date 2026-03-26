#include "llvm/Transforms/Instrumentation/XRayPreInlineInstrument.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/EscapeEnumerator.h"

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
  // prepend region enter intrinsic to entry block
  IRBuilder Builder(&*F.getEntryBlock().begin());
  Builder.CreateIntrinsic(Intrinsic::xray_customregionenter, {&F});

  // append region exit intrinsic to every exit block's end
  // (but before terminator instruction)
  EscapeEnumerator Exits(F);

  while (IRBuilder<> *ExitBuilder = Exits.Next()) {
    ExitBuilder->CreateIntrinsic(Intrinsic::xray_customregionexit, {&F});
  }
}
} // namespace llvm
