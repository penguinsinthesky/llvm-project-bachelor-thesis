#ifndef LLVM_XRAY_CUSTOM_REGIONS_H
#define LLVM_XRAY_CUSTOM_REGIONS_H

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// synchronize with llvm::xray::CustomRegionKind !
enum XRayCustomRegionKind {
  INLINED_FUNCTION = 0,
  USER_PLACED = 1,

  /// May represent invalid region IDs
  INVALID = -1
};

/**
 * Returns a string representation of XRayCustomRegionKind as a string
 * with static lifetime.
 */
const char *__xray_custom_region_kind_string(enum XRayCustomRegionKind kind);

/**
 * Returns a custom region's kind or INVALID if the provided ID is out of
 * bounds.
 */
enum XRayCustomRegionKind __xray_custom_region_get_kind(int32_t region_id);

/**
 * Returns a custom region's name or null if the provided ID is out of bounds.
 */
const char *__xray_custom_region_get_name(int32_t region_id);

/// Returns the number of custom regions present in the binary
size_t __xray_get_num_custom_regions();

#ifdef __cplusplus
}
#endif

#endif // LLVM_XRAY_CUSTOM_REGIONS_H
