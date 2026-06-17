#ifndef LLVM_XRAYLOOPINSTRUMENT_H
#define LLVM_XRAYLOOPINSTRUMENT_H

#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"

namespace llvm {

class XRayLoopInstrumentPass
    : public RequiredPassInfoMixin<XRayLoopInstrumentPass> {
public:
  PreservedAnalyses run(Loop &L, LoopAnalysisManager &,
                        LoopStandardAnalysisResults &, LPMUpdater &);

private:
  static std::optional<StringRef> getRegionName(const Loop &L);

  static void instrumentLoop(const Loop &L, StringRef RegionName);
};

} // namespace llvm

#endif // LLVM_XRAYLOOPINSTRUMENT_H
