#pragma once

#include "types.h"

typedef struct VehicleTrafficDirectionCounts
{
  int32 reverse_count;
  int32 forward_count;
} VehicleTrafficDirectionCounts;

extern VehicleTrafficDirectionCounts g_VEHICLE_TRAFFIC_DIRECTION_COUNTS;
extern int32                         g_VEHICLE_SPAWN_CONFIGURATION_MODE;

void
initialize_vehicle_drive_state(void);
