#pragma once

#include "types.h"

#define VEHICLE_INDEX_SLOT_COUNT 100

typedef struct VehicleIndexSlots
{
  uint8 reserved_000[0x4B0];
  int32 indices[VEHICLE_INDEX_SLOT_COUNT];
} VehicleIndexSlots;

void
clear_vehicle_index_slots(VehicleIndexSlots *vehicle_state_);
