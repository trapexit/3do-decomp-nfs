#pragma once

#include "geometry.h"

#define VEHICLE_RENDER_VALUE_COUNT 9

typedef struct VehicleRenderSnapshotState
{
  uint8   reserved_000[0x0C];
  Vector3 current_position;
  Vector3 current_vector;
  int32   current_value_a;
  int32   current_value_b;
  int32   current_values[VEHICLE_RENDER_VALUE_COUNT];
  int32   current_mode;
  uint8   reserved_054[0x300];
  int32   render_mode;
  int32   render_value_c;
  Vector3 render_position;
  Vector3 render_vector;
  int32   render_value_a;
  int32   render_value_b;
  uint8   reserved_37_c[0x0C];
  int32   render_values[VEHICLE_RENDER_VALUE_COUNT];
  uint8   reserved_3_ac[0x38];
  int32   current_value_c;
} VehicleRenderSnapshotState;

void
capture_vehicle_render_snapshot(VehicleRenderSnapshotState *state_);
