#include "llvm/XRay/CustomRegionMetadata.h"

namespace llvm::xray {

Metadata *InlinedFunctionMetadata::createMetadata() const {
  return ValueAsMetadata::getConstant(OriginalFunction);
}

InlinedFunctionMetadata
InlinedFunctionMetadata::fromMetadata(const Metadata *MD) {
  auto *const VAM = cast<ValueAsMetadata>(MD);
  auto *const OriginalFunction = cast<Function>(VAM->getValue());

  return {OriginalFunction};
}

XRayCustomRegionMetadata::XRayCustomRegionMetadata(
    const CustomRegionKind Kind,
    const std::variant<InlinedFunctionMetadata> Specific)
    : Kind(Kind), Specific(Specific) {}

XRayCustomRegionMetadata
XRayCustomRegionMetadata::inlinedFunction(InlinedFunctionMetadata IFM) {
  return XRayCustomRegionMetadata(CustomRegionKind::INLINED_FUNCTION, IFM);
}

static CustomRegionKind regionKindFromMd(const Metadata *MD) {
  // assume correct format (always compiler generated -> hard failure if wrong)
  const auto *KindVAM = cast<ValueAsMetadata>(MD);
  const auto *KindConstant = cast<ConstantInt>(KindVAM->getValue());
  assert(KindConstant->getType()->isIntegerTy(32) && "Kind must be an i32");

  switch (KindConstant->getZExtValue()) {
  case static_cast<uint64_t>(CustomRegionKind::INLINED_FUNCTION):
    return CustomRegionKind::INLINED_FUNCTION;
  default:
    llvm_unreachable(("Unknown Kind value"));
  }
}

XRayCustomRegionMetadata
XRayCustomRegionMetadata::fromMDNode(const MDTuple *Node) {
  assert(Node->getNumOperands() == 2 &&
         "Metadata must be a tuple of exactly two elements");

  const Metadata *KindMD = Node->getOperand(0);
  const Metadata *SpecificMD = Node->getOperand(1);

  switch (regionKindFromMd(KindMD)) {
  case CustomRegionKind::INLINED_FUNCTION:
    return inlinedFunction(InlinedFunctionMetadata::fromMetadata(SpecificMD));
  default:
    llvm_unreachable("Unknown CustomRegionKind");
  }
}
MDNode *XRayCustomRegionMetadata::createMDNode(LLVMContext &C) const {
  Metadata *KindMD = ValueAsMetadata::getConstant(
      ConstantInt::get(Type::getInt32Ty(C), static_cast<uint32_t>(Kind)));

  Metadata *MDs[2] = {KindMD, specificMetadata()};
  return MDTuple::get(C, MDs);
}
Metadata *XRayCustomRegionMetadata::specificMetadata() const {
  return std::visit([](const auto &S) { return S.createMetadata(); }, Specific);
}
} // namespace llvm::xray
