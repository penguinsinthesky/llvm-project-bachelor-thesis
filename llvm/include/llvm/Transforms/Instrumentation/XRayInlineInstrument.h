#ifndef LLVM_XRAY_PRE_INLINE_INSTRUMENT_H
#define LLVM_XRAY_PRE_INLINE_INSTRUMENT_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class XRayPreInlineInstrumentPass
    : public RequiredPassInfoMixin<XRayPreInlineInstrumentPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

private:
  static bool shouldInstrument(const Function &F);

  void encloseInCustomRegion(Function &F);
};

class XRayPostInlinePurgePass
    : public RequiredPassInfoMixin<XRayPostInlinePurgePass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_XRAY_PRE_INLINE_INSTRUMENT_H
