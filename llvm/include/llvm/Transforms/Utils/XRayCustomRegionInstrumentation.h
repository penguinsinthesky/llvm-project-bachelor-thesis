#ifndef LLVM_XRAYCUSTOMREGIONINSERTER_H
#define LLVM_XRAYCUSTOMREGIONINSERTER_H

// Region Kinds defined in compiler-rt are the single source of truth
enum XRayCustomRegionKind : int;

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

  static XRayCustomRegionInserter forUserPlaced(StringRef RegionName,
                                                Module &Module);

  static XRayCustomRegionInserter forLoop(const Loop &Loop,
                                          StringRef RegionName);

  CallInst *insertEnter(IRBuilderBase &Builder);

  CallInst *insertExit(IRBuilderBase &Builder);

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
