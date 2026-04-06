#ifndef LLVM_CUSTOMREGIONINFO_H
#define LLVM_CUSTOMREGIONINFO_H

#include "llvm/IR/Constants.h"
#include "llvm/IR/Metadata.h"

#include <variant>

namespace llvm::xray {

enum CustomRegionKind : uint32_t { INLINED_FUNCTION = 1, USER_PLACED = 2 };

struct UserPlacedRegionInfo {
  Metadata *createMetadata(LLVMContext &Ctx) const;

  static UserPlacedRegionInfo fromMetadata(const Metadata *MD);
};

struct InlinedFunctionRegionInfo {
  StringRef OriginalFunctionName;
  std::optional<Function *> OriginalFunction;

  Metadata *createMetadata(LLVMContext &Ctx) const;

  static InlinedFunctionRegionInfo fromMetadata(const Metadata *MD);
};

class XRayCustomRegionInfo {
  CustomRegionKind Kind;
  std::variant<InlinedFunctionRegionInfo, UserPlacedRegionInfo> Specific;

  XRayCustomRegionInfo(CustomRegionKind Kind,
                       const std::variant<InlinedFunctionRegionInfo,
                                          UserPlacedRegionInfo> &Specific);

public:
  static XRayCustomRegionInfo inlinedFunction(InlinedFunctionRegionInfo IFM);

  static XRayCustomRegionInfo userPlaced(UserPlacedRegionInfo UPM);

  static XRayCustomRegionInfo fromMetadata(const Metadata *MD);

  [[nodiscard]] MDNode *createMetadata(LLVMContext &Ctx) const;

  [[nodiscard]] CustomRegionKind getKind() const { return Kind; }

  [[nodiscard]] InlinedFunctionRegionInfo getInlinedFunctionMD() const {
    return std::get<InlinedFunctionRegionInfo>(Specific);
  }
};

} // namespace llvm::xray

#endif // LLVM_CUSTOMREGIONINFO_H
