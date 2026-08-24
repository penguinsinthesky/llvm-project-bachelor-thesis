#include "llvm/Transforms/Utils/XRayCustomRegionInstrumentation.h"

// Region Kinds defined in compiler-rt are the single source of truth
// TODO check if there is a better way of including this
#include "../../../../compiler-rt/include/xray/xray_custom_region_kind.h"

#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/Module.h"

namespace llvm {

constexpr static StringRef InlinedFunctionInfoMDKey =
    "llvm.xray.inlined_function_info";

const Function *XRayInlinedFunctionInfo::getOriginalFunction() const {
  const Metadata *OriginalFunctionMD = MD->getOperand(0);
  if (OriginalFunctionMD == nullptr) {
    // if the original function was DCE'ed, the whole metadata is null
    return nullptr;
  }

  return cast<Function>(
      cast<ConstantAsMetadata>(OriginalFunctionMD)->getValue());
}

uint64_t XRayInlinedFunctionInfo::getInstructionThreshold() const {
  const Metadata *InstructionThresholdID = MD->getOperand(1);
  return cast<ConstantInt>(
             cast<ConstantAsMetadata>(InstructionThresholdID)->getValue())
      ->getZExtValue();
}

XRayInlinedFunctionInfo::XRayInlinedFunctionInfo(const MDNode *MD) : MD(MD) {}

XRayCustomRegionInfo
XRayCustomRegionInfo::fromIntrinsicCall(const CallInst &Call) {
  assert((Call.getIntrinsicID() == Intrinsic::xray_customregionenter ||
          Call.getIntrinsicID() == Intrinsic::xray_customregionexit) &&
         "CallInst must call an XRay custom region intrinsic");
  assert(Call.arg_size() == 1 &&
         "Call to XRay custom region intrinsic must have exactly one argument");

  auto *const Global = cast<GlobalVariable>(Call.getArgOperand(0));
  return XRayCustomRegionInfo(Global);
}
XRayCustomRegionInfo XRayCustomRegionInfo::fromRegionInfoGlobal(
    const GlobalVariable *RegionInfoGlobal) {
  return XRayCustomRegionInfo(RegionInfoGlobal);
}

const GlobalVariable *XRayCustomRegionInfo::getRegionInfoGlobal() const {
  return RegionInfoGlobal;
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

XRayInlinedFunctionInfo XRayCustomRegionInfo::getInlinedFunctionInfo() const {
  assert(
      getRegionKind() == XRayCustomRegionKind::INLINED_FUNCTION &&
      "Only custom regions from inlined functions have an original function");

  SmallVector<MDNode *, 1> MDs;
  RegionInfoGlobal->getMetadata(InlinedFunctionInfoMDKey, MDs);
  assert(MDs.size() == 1 && "Expected exactly one metadata enty");

  return XRayInlinedFunctionInfo(MDs[0]);
}

bool XRayCustomRegionInfo::operator==(const XRayCustomRegionInfo &Other) const {
  return this->RegionInfoGlobal == Other.RegionInfoGlobal;
}

XRayCustomRegionInfo::XRayCustomRegionInfo(
    const GlobalVariable *RegionInfoGlobal)
    : RegionInfoGlobal(RegionInfoGlobal) {}

const ConstantStruct *XRayCustomRegionInfo::regionInfo() const {
  return cast<ConstantStruct>(RegionInfoGlobal->getInitializer());
}

XRayCustomRegionInserter
XRayCustomRegionInserter::forInlinedFunction(Function &OriginalFunction) {
  const std::string FnName = demangle(OriginalFunction.getName());
  LLVMContext &Ctx = OriginalFunction.getContext();
  auto *RegionInfoGlobal = createRegionInfoGlobal(
      XRayCustomRegionKind::INLINED_FUNCTION, Twine("<inlined>") + FnName,
      *OriginalFunction.getParent());

  const uint64_t XRayThreshold = OriginalFunction.getFnAttributeAsParsedInteger(
      "xray-instruction-threshold", std::numeric_limits<uint64_t>::max());

  auto *OriginalFunctionMD = ConstantAsMetadata::get(&OriginalFunction);
  auto *InstructionThresholdMD = ConstantAsMetadata::get(
      ConstantInt::get(Type::getInt64Ty(Ctx), XRayThreshold));

  auto *AdditionalMD =
      MDNode::get(Ctx, {OriginalFunctionMD, InstructionThresholdMD});
  RegionInfoGlobal->addMetadata(InlinedFunctionInfoMDKey, *AdditionalMD);

  return XRayCustomRegionInserter(RegionInfoGlobal);
}

XRayCustomRegionInserter
XRayCustomRegionInserter::forUserPlaced(const StringRef RegionName,
                                        Module &Module) {
  auto *RegionInfoGlobal = createRegionInfoGlobal(
      XRayCustomRegionKind::USER_PLACED, RegionName, Module);

  return XRayCustomRegionInserter(RegionInfoGlobal);
}

XRayCustomRegionInserter
XRayCustomRegionInserter::forLoop(const Loop &Loop, const Twine &RegionName) {
  Function *ParentFunction = Loop.getHeader()->getParent();
  auto *RegionInfoGlobal = createRegionInfoGlobal(
      XRayCustomRegionKind::LOOP, RegionName, *ParentFunction->getParent());

  return XRayCustomRegionInserter(RegionInfoGlobal);
}

Value *XRayCustomRegionInserter::insertEnter(IRBuilderBase &Builder) {
  return insertProbe(Builder, Intrinsic::xray_customregionenter);
}

Value *XRayCustomRegionInserter::insertExit(IRBuilderBase &Builder) {
  return insertProbe(Builder, Intrinsic::xray_customregionexit);
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

  auto *RegionInfoGlobal =
      new GlobalVariable(M, StructTy, true, GlobalValue::InternalLinkage,
                         StructLiteral, "xray_custom_region");
  // store this in our own section
  RegionInfoGlobal->setSection(SectionName);

  return RegionInfoGlobal;
}

XRayCustomRegionInserter::XRayCustomRegionInserter(
    GlobalVariable *RegionInfoGlobal)
    : RegionInfoGlobal(RegionInfoGlobal) {}

Value *XRayCustomRegionInserter::insertProbe(IRBuilderBase &Builder,
                                             const Intrinsic::ID Intrinsic) {
  // mark function so codegen knows this function may contain sleds
  Builder.GetInsertBlock()->getParent()->addFnAttr(
      "xray-contains-custom-region");
  return Builder.CreateIntrinsic(Intrinsic, {RegionInfoGlobal});
}

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
