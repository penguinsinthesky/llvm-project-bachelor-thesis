#include "llvm/Transforms/Utils/XRayCustomRegionInstrumentation.h"

namespace llvm {

constexpr static StringRef OriginalFunctionMdKey =
    "llvm.xray.original_function";

XRayCustomRegionInfo
XRayCustomRegionInfo::fromIntrinsicCall(const CallInst &Call) {
  assert(Call.getIntrinsicID() == Intrinsic::xray_customregionenter ||
         Call.getIntrinsicID() == Intrinsic::xray_customregionexit &&
             "CallInst must call an XRay custom region intrinsic");
  assert(Call.arg_size() == 1 &&
         "Call to XRay custom region intrinsic must have exactly one argument");

  auto *const Global = cast<GlobalVariable>(Call.getArgOperand(0));
  return XRayCustomRegionInfo(Global);
}

XRayCustomRegionKind XRayCustomRegionInfo::getRegionKind() const {
  const auto *Op = cast<ConstantInt>(regionInfo()->getOperand(0));
  return static_cast<XRayCustomRegionKind>(Op->getZExtValue());
}

StringRef XRayCustomRegionInfo::getRegionName() const {
  const auto *NameGlobal = cast<GlobalVariable>(regionInfo()->getOperand(1));
  const auto *const RegionNameConstant =
      cast<ConstantDataArray>(NameGlobal->getInitializer());

  return RegionNameConstant->getAsCString();
}

const Function *XRayCustomRegionInfo::getOriginalFunction() const {
  assert(
      getRegionKind() == XRayCustomRegionKind::INLINED_FUNCTION &&
      "Only custom regions from inlined functions have an original function");

  SmallVector<MDNode *, 1> MDs;
  RegionInfoGlobal->getMetadata(OriginalFunctionMdKey, MDs);
  assert(MDs.size() == 1 && "Expected exactly one metadata enty");

  const Metadata *OriginalFunctionMD = MDs[0]->getOperand(0);
  if (OriginalFunctionMD == nullptr) {
    // if the original function was DCE'ed, the whole metadata is null
    return nullptr;
  }

  return cast<Function>(
      cast<ConstantAsMetadata>(OriginalFunctionMD)->getValue());
}

XRayCustomRegionInfo::XRayCustomRegionInfo(
    const GlobalVariable *RegionInfoGlobal)
    : RegionInfoGlobal(RegionInfoGlobal) {}

const ConstantStruct *XRayCustomRegionInfo::regionInfo() const {
  return cast<ConstantStruct>(RegionInfoGlobal->getInitializer());
}

XRayCustomRegionInserter
XRayCustomRegionInserter::forInlinedFunction(Function &OriginalFunction) {
  auto *RegionInfoGlobal = createRegionInfoGlobal(
      XRayCustomRegionKind::INLINED_FUNCTION,
      "<inlined>" + OriginalFunction.getName(), *OriginalFunction.getParent());

  auto *OriginalFunctionMD =
      MDNode::get(OriginalFunction.getContext(),
                  ConstantAsMetadata::get(&OriginalFunction));
  RegionInfoGlobal->addMetadata(OriginalFunctionMdKey, *OriginalFunctionMD);

  return XRayCustomRegionInserter(RegionInfoGlobal);
}

XRayCustomRegionInserter
XRayCustomRegionInserter::forLoop(const Loop &Loop, StringRef RegionName) {
  Function *ParentFunction = Loop.getHeader()->getParent();
  auto *RegionInfoGlobal = createRegionInfoGlobal(
      XRayCustomRegionKind::LOOP, RegionName, *ParentFunction->getParent());

  return XRayCustomRegionInserter(RegionInfoGlobal);
}

CallInst *XRayCustomRegionInserter::insertEnter(IRBuilder<> &Builder) {
  return Builder.CreateIntrinsic(Intrinsic::xray_customregionenter,
                                 {RegionInfoGlobal});
}

CallInst *XRayCustomRegionInserter::insertExit(IRBuilder<> &Builder) {
  return Builder.CreateIntrinsic(Intrinsic::xray_customregionexit,
                                 {RegionInfoGlobal});
}
GlobalVariable *XRayCustomRegionInserter::createRegionInfoGlobal(
    const XRayCustomRegionKind Kind, const Twine &RegionName, Module &M) {
  LLVMContext &Ctx = M.getContext();

  StructType *StructTy = getRegionInfoType(M);

  Constant *KindConstant = ConstantInt::get(StructTy->getElementType(0), Kind);
  Constant *RegionNameConstant =
      ConstantDataArray::getString(Ctx, RegionName.str(), true);

  // global will add itself to the module and the module will take ownership
  auto *RegionNameGlobal = new GlobalVariable(
      M, RegionNameConstant->getType(), true, GlobalValue::PrivateLinkage,
      RegionNameConstant, "xray_custom_region_name");

  Constant *StructLiteral =
      ConstantStruct::get(StructTy, {KindConstant, RegionNameGlobal});

  // use internal leakage so this global appears in the symbol table
  auto *RegionInfoGlobal =
      new GlobalVariable(M, StructTy, true, GlobalValue::InternalLinkage,
                         StructLiteral, RegionName);
  // store this in our own section
  RegionInfoGlobal->setSection(SectionName);

  return RegionInfoGlobal;
}

XRayCustomRegionInserter::XRayCustomRegionInserter(
    GlobalVariable *RegionInfoGlobal)
    : RegionInfoGlobal(RegionInfoGlobal) {}

StructType *XRayCustomRegionInserter::getRegionInfoType(const Module &M) {
  LLVMContext &Ctx = M.getContext();

  // both values should have same size for alignment
  Type *I = M.getDataLayout().getIntPtrType(Ctx);
  Type *Ptr = PointerType::getUnqual(Ctx);

  // {<kind>, <name>}
  StructType *Ty = StructType::get(Ctx, {I, Ptr});
  return Ty;
}
} // namespace llvm
