#ifndef LLVM_XRAY_CUSTOM_REGIONS_INTERNAL_H
#define LLVM_XRAY_CUSTOM_REGIONS_INTERNAL_H

#include "xray/xray_defs.h"
#include "xray/xray_interface_internal.h"

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

struct XRaySledToCustomRegionMapping {
  // objects should only live in the .xray_sled_to_custom_region sections and
  // thus should not be copied/moved anywhere
  XRaySledToCustomRegionMapping(const XRaySledToCustomRegionMapping &other) =
      delete;
  XRaySledToCustomRegionMapping(
      XRaySledToCustomRegionMapping &&other) noexcept = delete;
  XRaySledToCustomRegionMapping &
  operator=(const XRaySledToCustomRegionMapping &other) = delete;
  XRaySledToCustomRegionMapping &
  operator=(XRaySledToCustomRegionMapping &&other) noexcept = delete;

#if SANITIZER_WORDSIZE == 64
  uint64_t SledOffset;
  uint64_t RegionEntryOffset;
#elif SANITIZER_WORDSIZE == 32
  uint32_t SledOffset;
  uint32_t RegionInfoOffset;
#else
#error "Unsupported word size."
#endif

  size_t getSledAddress() const XRAY_NEVER_INSTRUMENT {
    return reinterpret_cast<size_t>(&SledOffset) + SledOffset;
  }

  const XRayCustomRegionEntry *getRegionEntry() const XRAY_NEVER_INSTRUMENT {
    return reinterpret_cast<const XRayCustomRegionEntry *>(
        reinterpret_cast<size_t>(&RegionEntryOffset) + RegionEntryOffset);
  }
};

void __xray_allocate_custom_region_buffer();

bool __xray_register_custom_regions(const __xray::XRaySledMap &InstrMap, int32_t ObjId);
}

#endif // LLVM_XRAY_CUSTOM_REGIONS_INTERNAL_H
