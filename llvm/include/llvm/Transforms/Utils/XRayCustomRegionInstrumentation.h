#ifndef LLVM_XRAYCUSTOMREGIONINSERTER_H
#define LLVM_XRAYCUSTOMREGIONINSERTER_H

#include "llvm/XRay/CustomRegionInfo.h"

namespace llvm {

class XRayCustomRegionInfo {
public:
  static XRayCustomRegionInfo fromIntrinsicCall(const CallInst &Call);

  xray::CustomRegionKind getRegionKind() const;

  StringRef getRegionName() const;

  const Function *getOriginalFunction() const;

private:
  explicit XRayCustomRegionInfo(const GlobalVariable *RegionInfoGlobal);

  const GlobalVariable *RegionInfoGlobal;

  const ConstantStruct *regionInfo() const;
};

class XRayCustomRegionInserter {

public:
  static XRayCustomRegionInserter
  forInlinedFunction(Function &OriginalFunction);

  CallInst *insertEnter(IRBuilder<> &Builder);

  CallInst *insertExit(IRBuilder<> &Builder);

  constexpr static StringRef SectionName = "xray_custom_regions";

private:
  static GlobalVariable *createRegionInfoGlobal(xray::CustomRegionKind Kind,
                                                const Twine &RegionName,
                                                Module &M);

  explicit XRayCustomRegionInserter(GlobalVariable *RegionInfoGlobal);

  GlobalVariable *RegionInfoGlobal;

  static StructType *getRegionInfoType(const Module &M);
};

} // namespace llvm

#endif // LLVM_XRAYCUSTOMREGIONINSERTER_H
