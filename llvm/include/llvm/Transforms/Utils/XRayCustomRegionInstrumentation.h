#ifndef LLVM_XRAYCUSTOMREGIONINSERTER_H
#define LLVM_XRAYCUSTOMREGIONINSERTER_H

// Region Kinds defined in compiler-rt are the single source of truth
enum XRayCustomRegionKind : int;

#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"

namespace llvm {

class XRayInlinedFunctionInfo {
public:
  const Function *getOriginalFunction() const;

  uint64_t getInstructionThreshold() const;

private:
  const MDNode *MD;

  explicit XRayInlinedFunctionInfo(const MDNode *MD);

  friend class XRayCustomRegionInfo;
};

class XRayCustomRegionInfo {
public:
  static XRayCustomRegionInfo fromIntrinsicCall(const CallInst &Call);

  static XRayCustomRegionInfo
  fromRegionInfoGlobal(const GlobalVariable *RegionInfoGlobal);

  const GlobalVariable *getRegionInfoGlobal() const;

  XRayCustomRegionKind getRegionKind() const;

  StringRef getRegionName() const;

  XRayInlinedFunctionInfo getInlinedFunctionInfo() const;

  bool operator==(const XRayCustomRegionInfo &Other) const;

private:
  explicit XRayCustomRegionInfo(const GlobalVariable *RegionInfoGlobal);

  const GlobalVariable *RegionInfoGlobal;

  const ConstantStruct *regionInfo() const;

  friend struct DenseMapInfo<XRayCustomRegionInfo>;
};

template <> struct DenseMapInfo<XRayCustomRegionInfo> {
  static XRayCustomRegionInfo getEmptyKey() {
    return XRayCustomRegionInfo(nullptr);
  }

  static XRayCustomRegionInfo getTombstoneKey() {
    return XRayCustomRegionInfo(nullptr);
  }

  static unsigned getHashValue(const XRayCustomRegionInfo &Obj) {
    return DenseMapInfo<const GlobalVariable *>::getHashValue(
        Obj.RegionInfoGlobal);
  }

  static bool isEqual(const XRayCustomRegionInfo &LHS,
                      const XRayCustomRegionInfo &RHS) {
    return LHS == RHS;
  }
};

class XRayCustomRegionInserter {

public:
  static XRayCustomRegionInserter
  forInlinedFunction(Function &OriginalFunction);

  static XRayCustomRegionInserter forUserPlaced(StringRef RegionName,
                                                Module &Module);

  static XRayCustomRegionInserter forLoop(const Loop &Loop,
                                          const Twine &RegionName);

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
