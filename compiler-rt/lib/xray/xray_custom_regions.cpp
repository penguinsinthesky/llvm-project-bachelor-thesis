#include "xray/xray_custom_regions.h"
#include "xray/xray_custom_regions_internal.h"

#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_dense_map.h"
#include "xray/xray_defs.h"
#include "xray_allocator.h"

// TODO not really sure why using DenseMap needs this
#include <new>

using __sanitizer::SpinMutex;
using __sanitizer::SpinMutexLock;

using namespace __xray;

using IdRegionMap = DenseMap<int32_t, const XRayCustomRegionEntry *>;

/// Maps custom regions' function IDs to custom region information.
/// Note this uses packed IDs as keys so one map can serve all DSOs.
static IdRegionMap XRayFuncIdToRegion;

/// Guards writes to the map above
static SpinMutex XRayFuncIdToRegionMutex;

template <typename R, class InfoFn>
static R get_custom_region_data(const int32_t region_id, R invalid, InfoFn Fn) {
  if (const auto *pair = XRayFuncIdToRegion.find(region_id)) {
    return Fn(pair->second);
  }

  Report("Invalid custom region ID: %d\n", region_id);
  return invalid;
}

extern "C" {

extern const XRayCustomRegionEntry __start_xray_custom_regions[]
    __attribute__((weak));
extern const XRayCustomRegionEntry __stop_xray_custom_regions[]
    __attribute__((weak));
const static size_t NUM_CUSTOM_REGIONS =
    __stop_xray_custom_regions - __start_xray_custom_regions;

extern const XRaySledToCustomRegionMapping __start_xray_sled_to_custom_region[]
    __attribute__((weak));
extern const XRaySledToCustomRegionMapping __stop_xray_sled_to_custom_region[]
    __attribute__((weak));

bool __xray_register_custom_regions(const XRaySledMap &InstrMap,
                                    const int32_t ObjId) XRAY_NEVER_INSTRUMENT {
  if (ObjId >= static_cast<int32_t>(XRayMaxObjects)) {
    Report("Too many objects registered! Maximum is %ld\n", XRayMaxObjects);
    return false;
  }

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

  IdRegionMap FuncIdToRegion;

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
        const auto PackedId = MakePackedId(FuncId, ObjId);

        auto pair = __sanitizer::detail::DenseMapPair<
            int32_t, const XRayCustomRegionEntry *>(PackedId, RegionEntry);
        FuncIdToRegion.insert(pair);
      } else {
        Report("No custom region entry for sled at address %p\n",
               reinterpret_cast<void *>(FirstSled->address()));
        return false;
      }
    }
  } else {
    enumerate_functions(InstrMap, [&](const int32_t FuncId,
                                      const XRayFunctionSledIndex
                                          FunctionSleds) {
      const auto &FirstSled = FunctionSleds.Begin[0];
      if (FirstSled.Kind == CUSTOM_REGION_ENTRY) {
        const auto PackedId = MakePackedId(FuncId, ObjId);
        FuncIdToRegion[PackedId] =
            SledToRegion[FirstSled.address()]; // take region info of first sled
      }
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
  case LOOP:
    return "LOOP";
  case INVALID:
    return "INVALID";
  }

  Report("Invalid XRayCustomRegionKind value: %u\n", kind);
  return nullptr;
}

XRayCustomRegionKind
__xray_custom_region_get_kind(const int32_t region_id) XRAY_NEVER_INSTRUMENT {
  return get_custom_region_data<XRayCustomRegionKind>(
      region_id, INVALID, [](const XRayCustomRegionEntry *Entry) {
        return static_cast<XRayCustomRegionKind>(Entry->Kind);
      });
}

const char *
__xray_custom_region_get_name(const int32_t region_id) XRAY_NEVER_INSTRUMENT {
  return get_custom_region_data<const char *>(
      region_id, nullptr,
      [](const XRayCustomRegionEntry *Entry) { return Entry->Name; });
}

size_t __xray_get_num_custom_regions() XRAY_NEVER_INSTRUMENT {
  return NUM_CUSTOM_REGIONS;
}
}
