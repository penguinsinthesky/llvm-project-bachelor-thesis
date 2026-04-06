#include "llvm/XRay/CustomRegionInfo.h"

namespace llvm::xray {

[[noreturn]] static void reportFatalFromMetadataError(std::string Msg,
                                                      const Metadata *MD) {
  raw_string_ostream SO(Msg);

  const std::string Between = " Metadata: ";
  SO.write(Between.c_str(), Between.size());

  MD->print(SO);

  reportFatalInternalError(StringRef(Msg));
}

Metadata *InlinedFunctionRegionInfo::createMetadata(LLVMContext &Ctx) const {

  Metadata *OriginalFunctionMD =
      OriginalFunction.has_value()
          ? ValueAsMetadata::getConstant(*OriginalFunction)
          : nullptr; // TODO check if allowed

  return MDTuple::get(
      Ctx, {MDString::get(Ctx, OriginalFunctionName), OriginalFunctionMD});
}

InlinedFunctionRegionInfo
InlinedFunctionRegionInfo::fromMetadata(const Metadata *MD) {
  const MDTuple *Tuple = dyn_cast<MDTuple>(MD);

  if (Tuple == nullptr || Tuple->getNumOperands() != 2) {
    reportFatalFromMetadataError(
        "Metadata for inlined function XRay region must be a 2-tuple", MD);
  }

  const MDString *OriginalFunctionNameMD =
      dyn_cast<MDString>(Tuple->getOperand(0));

  if (OriginalFunctionNameMD == nullptr) {
    reportFatalFromMetadataError(
        "First tuple element of inlined function XRay region must be a string",
        MD);
  }

  StringRef OriginalFunctionName = OriginalFunctionNameMD->getString();

  const Metadata *OriginalFunctionMD = Tuple->getOperand(1);
  if (OriginalFunctionMD == nullptr) {
    // TODO check if this a valid approach
    return InlinedFunctionRegionInfo{OriginalFunctionName, std::nullopt};
  }

  // if metadata value is not null, it is expected to still exist and be a
  // function
  if (auto *const VAM = dyn_cast<ValueAsMetadata>(OriginalFunctionMD)) {
    if (auto *const OriginalFunction = dyn_cast<Function>(VAM->getValue())) {
      return {OriginalFunctionName, OriginalFunction};
    }
  }

  reportFatalFromMetadataError(
      "Metadata does not represent an inlined function.", MD);
}

Metadata *UserPlacedRegionInfo::createMetadata(LLVMContext &Ctx) const {
  return MDTuple::get(Ctx, {});
}

UserPlacedRegionInfo UserPlacedRegionInfo::fromMetadata(const Metadata *MD) {
  return UserPlacedRegionInfo{};
}

XRayCustomRegionInfo::XRayCustomRegionInfo(
    const CustomRegionKind Kind,
    const std::variant<InlinedFunctionRegionInfo, UserPlacedRegionInfo>
        &Specific)
    : Kind(Kind), Specific(Specific) {}

XRayCustomRegionInfo
XRayCustomRegionInfo::inlinedFunction(InlinedFunctionRegionInfo IFM) {
  return XRayCustomRegionInfo(INLINED_FUNCTION, IFM);
}

XRayCustomRegionInfo
XRayCustomRegionInfo::userPlaced(UserPlacedRegionInfo UPM) {
  return XRayCustomRegionInfo(USER_PLACED, UPM);
}

static CustomRegionKind regionKindFromMd(const Metadata *MD) {
  if (const auto *KindVAM = dyn_cast<ValueAsMetadata>(MD)) {
    if (const auto *KindConstant = dyn_cast<ConstantInt>(KindVAM->getValue())) {
      assert(KindConstant->getType()->isIntegerTy(32) && "Kind must be an i32");

      const auto KindID = static_cast<uint32_t>(KindConstant->getZExtValue());

      switch (KindID) {
      case INLINED_FUNCTION: {
        return INLINED_FUNCTION;
      }
      case USER_PLACED: {
        return USER_PLACED;
      }
      default:
        break; // will report an error
      }
    }
  }

  reportFatalFromMetadataError(
      "Metadata does not represent a kind for custom XRay regions", MD);
}

XRayCustomRegionInfo XRayCustomRegionInfo::fromMetadata(const Metadata *MD) {
  const MDTuple *Tuple = dyn_cast<MDTuple>(MD);

  if (Tuple == nullptr || Tuple->getNumOperands() != 2) {
    reportFatalFromMetadataError(
        "Metadata for custom XRay region must be a 2-tuple", MD);
  }

  const Metadata *KindMD = Tuple->getOperand(0);
  const Metadata *SpecificMD = Tuple->getOperand(1);

  switch (regionKindFromMd(KindMD)) {
  case INLINED_FUNCTION:
    return inlinedFunction(InlinedFunctionRegionInfo::fromMetadata(SpecificMD));
  case USER_PLACED:
    return userPlaced(UserPlacedRegionInfo::fromMetadata(SpecificMD));
  }

  llvm_unreachable("Unknown CustomRegionKind");
}

MDNode *XRayCustomRegionInfo::createMetadata(LLVMContext &Ctx) const {
  Metadata *KindMD = ValueAsMetadata::getConstant(
      ConstantInt::get(Type::getInt32Ty(Ctx), Kind));

  Metadata *SpecificMD = std::visit(
      [&Ctx](const auto &S) { return S.createMetadata(Ctx); }, Specific);

  Metadata *MDs[2] = {KindMD, SpecificMD};
  return MDTuple::get(Ctx, MDs);
}
} // namespace llvm::xray
