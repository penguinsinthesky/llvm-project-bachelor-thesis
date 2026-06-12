#ifndef LLVM_XRAYLOOPINSTRUMENT_H
#define LLVM_XRAYLOOPINSTRUMENT_H

#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"

namespace llvm {

class XRayLoopInstrumentPass
    : public RequiredPassInfoMixin<XRayLoopInstrumentPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);

private:
  static std::optional<StringRef> getRegionName(const Loop &L);

  static void instrumentLoop(const Loop &L, const Function &F,
                             const PostDominatorTree &PD, StringRef RegionName);
};

} // namespace llvm

#endif // LLVM_XRAYLOOPINSTRUMENT_H
