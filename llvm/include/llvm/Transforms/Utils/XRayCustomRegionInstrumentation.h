#ifndef LLVM_XRAYCUSTOMREGIONINSERTER_H
#define LLVM_XRAYCUSTOMREGIONINSERTER_H

// Region Kinds defined in compiler-rt are the single source of truth
// TODO check if there is a better way of including this
#include "../../../../compiler-rt/include/xray/xray_custom_region_kind.h"

#include "llvm/Analysis/LoopInfo.h"

namespace llvm {

class XRayCustomRegionInfo {
public:
  static XRayCustomRegionInfo fromIntrinsicCall(const CallInst &Call);

  XRayCustomRegionKind getRegionKind() const;

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

  static XRayCustomRegionInserter forLoop(const Loop &Loop,
                                          StringRef RegionName);

  CallInst *insertEnter(IRBuilder<> &Builder);

  CallInst *insertExit(IRBuilder<> &Builder);

  constexpr static StringRef SectionName = "xray_custom_regions";

private:
  static GlobalVariable *createRegionInfoGlobal(XRayCustomRegionKind,
                                                const Twine &RegionName,
                                                Module &M);

  explicit XRayCustomRegionInserter(GlobalVariable *RegionInfoGlobal);

  GlobalVariable *RegionInfoGlobal;

  static StructType *getRegionInfoType(const Module &M);
};

} // namespace llvm

#endif // LLVM_XRAYCUSTOMREGIONINSERTER_H
