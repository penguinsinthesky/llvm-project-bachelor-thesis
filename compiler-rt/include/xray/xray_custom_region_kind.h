#ifndef LLVM_XRAY_CUSTOM_REGION_KIND_H
#define LLVM_XRAY_CUSTOM_REGION_KIND_H

#ifdef __cplusplus
extern "C" {
#endif

enum XRayCustomRegionKind : int {
  /// This region encloses an inlined function.
  INLINED_FUNCTION = 0,
  /// This region originates from user placed probes.
  USER_PLACED = 1,
  /// This region encloses a loop body.
  LOOP = 2,

  /// May represent invalid region IDs
  INVALID = -1
};

#ifdef __cplusplus
}
#endif

#endif // LLVM_XRAY_CUSTOM_REGION_KIND_H
