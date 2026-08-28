#ifndef LLVM_XRAYCUSTOMREGIONFILTERING_H
#define LLVM_XRAYCUSTOMREGIONFILTERING_H

#include "llvm/CodeGen/MachineFunctionAnalysisManager.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class XRayInlinedRegionFilterPass
    : public RequiredPassInfoMixin<XRayInlinedRegionFilterPass> {
public:
  XRayInlinedRegionFilterPass() = default;

  PreservedAnalyses run(MachineFunction &MF,
                        MachineFunctionAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_XRAYCUSTOMREGIONFILTERING_H
