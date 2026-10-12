#include "vehicle_traffic_spacing.h"

#define VEHICLE_TRAFFIC_SPACING_LIGHT 50
#define VEHICLE_TRAFFIC_SPACING_MEDIUM 45
#define VEHICLE_TRAFFIC_SPACING_HEAVY 40

#define COMPILE_TIME_SIZE_ASSERT(name_, expression_)                                               \
  typedef char compile_time_size_assert_##name_[(expression_) ? 1 : -1]

const int32 G_VEHICLE_TRAFFIC_SPACING_PRESETS[VEHICLE_TRAFFIC_SPACING_PRESET_COUNT] = {
  VEHICLE_TRAFFIC_SPACING_LIGHT,
  VEHICLE_TRAFFIC_SPACING_MEDIUM,
  VEHICLE_TRAFFIC_SPACING_HEAVY
};

COMPILE_TIME_SIZE_ASSERT(vehicle_traffic_spacing_presets_size,
                         sizeof(G_VEHICLE_TRAFFIC_SPACING_PRESETS) == 0x0C);
