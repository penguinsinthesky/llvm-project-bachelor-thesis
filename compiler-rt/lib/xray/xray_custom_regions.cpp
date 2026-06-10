#include "xray/xray_custom_regions.h"
#include "xray/xray_custom_regions_internal.h"

#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_dense_map.h"
#include "xray/xray_defs.h"
#include <cstdio>
// TODO not really sure why using DenseMap needs this
#include <new>

using __sanitizer::DenseMap;
using __sanitizer::SpinMutex;
using __sanitizer::SpinMutexLock;

using namespace __xray;

extern "C" {

extern const XRayCustomRegionEntry __start_xray_custom_regions[]
    __attribute__((weak));
extern const XRayCustomRegionEntry __stop_xray_custom_regions[]
    __attribute__((weak));
const static uint32_t NUM_CUSTOM_REGIONS =
    __stop_xray_custom_regions - __start_xray_custom_regions;

extern const XRaySledToCustomRegionMapping __start_xray_sled_to_custom_region[]
    __attribute__((weak));
extern const XRaySledToCustomRegionMapping __stop_xray_sled_to_custom_region[]
    __attribute__((weak));

SpinMutex XRayFuncIdToRegionMutex;
DenseMap<uint32_t, const XRayCustomRegionEntry *> XRayFuncIdToRegion;

bool __xray_register_custom_regions(const XRaySledMap &InstrMap)
    XRAY_NEVER_INSTRUMENT {
  DenseMap<size_t, const XRayCustomRegionEntry *> SledToRegion;

  const XRaySledToCustomRegionMapping *Mapping =
      __start_xray_sled_to_custom_region;
  while (Mapping != __stop_xray_sled_to_custom_region) {
    SledToRegion.insert(
        __sanitizer::detail::DenseMapPair<size_t,
                                          const XRayCustomRegionEntry *>(
            Mapping->getSledAddress(), Mapping->getRegionEntry()));
    ++Mapping;
  }

  DenseMap<uint32_t, const XRayCustomRegionEntry *> FuncIdToRegion;

  if (InstrMap.SledsIndex != nullptr) {
    for (size_t I = 0; I < InstrMap.Functions; ++I) {
      const auto *const FSI = &InstrMap.SledsIndex[I];
      const auto *const FirstSled = FSI->fromPCRelative();

      if (FirstSled->Kind != CUSTOM_REGION_ENTRY) {
        continue; // custom region "functions" start with an entry sled
      }

      size_t FuncId = I + 1; // FuncIds start at 1
      if (const XRayCustomRegionEntry *RegionEntry =
              SledToRegion.lookup(FirstSled->address());
          RegionEntry) {
        auto pair = __sanitizer::detail::DenseMapPair<
            uint32_t, const XRayCustomRegionEntry *>(FuncId, RegionEntry);
        FuncIdToRegion.insert(pair);
      } else {
        Report("No custom region entry for sled at address %p\n",
               reinterpret_cast<void *>(FirstSled->address()));
        return false;
      }
    }
  } else {
    enumerate_functions(InstrMap, [&](const uint32_t FuncId,
                                      const XRayFunctionSledIndex
                                          FunctionSleds) {
      const auto &FirstSled = FunctionSleds.Begin[0];
      FuncIdToRegion[FuncId] =
          SledToRegion[FirstSled.address()]; // take region info of first sled
    });
  }
  if (Verbosity()) {
    Report("Found %u sleds for %u custom regions\n", SledToRegion.size(),
           FuncIdToRegion.size());
  }

  {
    SpinMutexLock Guard(&XRayFuncIdToRegionMutex);
    XRayFuncIdToRegion =
        FuncIdToRegion; // FIXME move-constructor broken, use when fixed
    return true;
  }
}

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
  if (const auto *pair = XRayFuncIdToRegion.find(region_id); pair) {
    return static_cast<XRayCustomRegionKind>(pair->second->Kind);
  }

  printf("Invalid custom region ID: %u\n", region_id);
  return INVALID;
}

const char *
__xray_custom_region_get_name(const uint32_t region_id) XRAY_NEVER_INSTRUMENT {
  if (const auto *pair = XRayFuncIdToRegion.find(region_id); pair) {
    return pair->second->getName();
  }

  printf("Invalid custom region ID: %u\n", region_id);
  return nullptr;
}

uint32_t __xray_get_num_custom_regions() XRAY_NEVER_INSTRUMENT {
  return NUM_CUSTOM_REGIONS;
}
}
