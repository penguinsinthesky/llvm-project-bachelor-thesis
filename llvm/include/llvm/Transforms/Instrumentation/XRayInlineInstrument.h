#ifndef LLVM_XRAY_PRE_INLINE_INSTRUMENT_H
#define LLVM_XRAY_PRE_INLINE_INSTRUMENT_H

#include "llvm/IR/PassManager.h"

namespace llvm {

/**
 * Pass that automatically instruments all functions that do not have
 * the `xray_never_instrument` attribute with custom regions of kind INLINED.
 */
class XRayPreInlineAutoInstrumentPass
    : public RequiredPassInfoMixin<XRayPreInlineAutoInstrumentPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

/**
 * Pass that instruments functions with the `xray_always_instrument` attribute
 * with custom regions of type INLINED.
 * This is used to also instrument these functions even if they are inlined.
 */
class XRayPreInlineInstrumentIfAlwaysPass
    : public RequiredPassInfoMixin<XRayPreInlineInstrumentIfAlwaysPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

/**
 * Pass to remove any INLINED custom regions from a function whose original
 * function is the parent function itself.
 */
class XRayPostInlinePurgePass
    : public RequiredPassInfoMixin<XRayPostInlinePurgePass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_XRAY_PRE_INLINE_INSTRUMENT_H
