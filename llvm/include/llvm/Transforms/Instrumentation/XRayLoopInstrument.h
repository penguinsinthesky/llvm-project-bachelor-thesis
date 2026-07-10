#ifndef LLVM_XRAYLOOPINSTRUMENT_H
#define LLVM_XRAYLOOPINSTRUMENT_H

#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"

namespace llvm {

/**
 * Pass that instruments loop bodies of loops with the llvm.loop.xray.instrument
 * metadata.
 */
class XRayLoopInstrumentPass
    : public RequiredPassInfoMixin<XRayLoopInstrumentPass> {
public:
  PreservedAnalyses run(Loop &L, LoopAnalysisManager &,
                        LoopStandardAnalysisResults &, LPMUpdater &);
};

/**
 * Pass that automatically instruments loop bodies for all outer loops
 * in a function. If an outer loop already has the llvm.loop.xray.instrument
 * metadata, it is left untouched.
 */
class XRayOuterLoopInstrumentPass
    : public RequiredPassInfoMixin<XRayOuterLoopInstrumentPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
};

} // namespace llvm

#endif // LLVM_XRAYLOOPINSTRUMENT_H
