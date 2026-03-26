#ifndef LLVM_CUSTOMREGIONMETADATA_H
#define LLVM_CUSTOMREGIONMETADATA_H

#include "llvm/IR/Constants.h"
#include "llvm/IR/Metadata.h"
#include <variant>

namespace llvm::xray {

enum class CustomRegionKind { INLINED_FUNCTION = 0 };

struct InlinedFunctionMetadata {
  Function *OriginalFunction;

  Metadata *createMetadata() const;

  static InlinedFunctionMetadata fromMetadata(const Metadata *MD);
};

class XRayCustomRegionMetadata {
  CustomRegionKind Kind;
  std::variant<InlinedFunctionMetadata> Specific;

  XRayCustomRegionMetadata(CustomRegionKind Kind,
                           std::variant<InlinedFunctionMetadata> Specific);

public:
  static XRayCustomRegionMetadata inlinedFunction(InlinedFunctionMetadata IFM);

  static XRayCustomRegionMetadata fromMDNode(const MDTuple *Node);

  [[nodiscard]] MDNode *createMDNode(LLVMContext &C) const;

private:
  Metadata *specificMetadata() const;
};

} // namespace llvm::xray

#endif // LLVM_CUSTOMREGIONMETADATA_H
