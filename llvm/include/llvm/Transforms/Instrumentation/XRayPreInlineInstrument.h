#ifndef LLVM_XRAY_PRE_INLINE_INSTRUMENT_H
#define LLVM_XRAY_PRE_INLINE_INSTRUMENT_H

#include "llvm/IR/PassManager.h"

namespace llvm {

// TODO register pass in clang
class XRayPreInlineInstrumentPass
    : public PassInfoMixin<XRayPreInlineInstrumentPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

  static bool isRequired() { return true; }

private:
  static bool shouldInstrument(const Function &F);

  void insertInstructions(Function &F) const;
};

} // namespace llvm

#endif // LLVM_XRAY_PRE_INLINE_INSTRUMENT_H
