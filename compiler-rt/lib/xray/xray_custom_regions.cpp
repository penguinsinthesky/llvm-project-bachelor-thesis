#include "xray/xray_custom_regions.h"

#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_internal_defs.h"
#include "xray/xray_defs.h"

using __sanitizer::Report;

extern "C" {

struct XRayCustomRegionEntry {

  // objects should only live in the .xray_custom_regions sections and thus
  // should not be copied/moved anywhere
  XRayCustomRegionEntry(const XRayCustomRegionEntry &other) = delete;
  XRayCustomRegionEntry(XRayCustomRegionEntry &&other) noexcept = delete;
  XRayCustomRegionEntry &operator=(const XRayCustomRegionEntry &other) = delete;
  XRayCustomRegionEntry &
  operator=(XRayCustomRegionEntry &&other) noexcept = delete;

#if SANITIZER_WORDSIZE == 64
  uint64_t NameOffset;
  uint64_t Kind;
#elif SANITIZER_WORDSIZE == 32
  uint32_t NameOffset;
  uint32_t Kind;
#else
#error "Unsupported word size."
#endif

  const char *getName() const XRAY_NEVER_INSTRUMENT {
    return reinterpret_cast<const char *>(&NameOffset) + NameOffset;
  }
};

extern const XRayCustomRegionEntry __start_xray_custom_regions[]
    __attribute__((weak));
extern const XRayCustomRegionEntry __stop_xray_custom_regions[]
    __attribute__((weak));
const static uint32_t NUM_ENTRIES =
    __stop_xray_custom_regions - __start_xray_custom_regions;

const char *__xray_custom_region_kind_string(const XRayCustomRegionKind kind)
    XRAY_NEVER_INSTRUMENT {
  switch (kind) {
  case INLINED_FUNCTION:
    return "INLINED_FUNCTION";
  case USER_PLACED:
    return "USER_PLACED";
  case INVALID:
    return "INVALID";
  }

  Report("Invalid XRayCustomRegionKind value: %u\n", kind);
  return nullptr;
}

XRayCustomRegionKind
__xray_custom_region_get_kind(const uint32_t region_id) XRAY_NEVER_INSTRUMENT {
  if (region_id >= NUM_ENTRIES) {
    Report("Invalid custom region ID: %u\n", region_id);
    return INVALID;
  }

  return static_cast<XRayCustomRegionKind>(
      __start_xray_custom_regions[region_id].Kind);
}

const char *
__xray_custom_region_get_name(const uint32_t region_id) XRAY_NEVER_INSTRUMENT {
  if (region_id >= NUM_ENTRIES) {
    Report("Invalid custom region ID: %u\n", region_id);
    return nullptr;
  }

  return __start_xray_custom_regions[region_id].getName();
}

uint32_t __xray_get_num_custom_regions() { return NUM_ENTRIES; }
}
