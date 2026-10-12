#pragma once

#include "types.h"

typedef struct VehicleMotionDirection
{
  uint8 reserved_00[0x30];
  int32 x;
  int32 y;
  int32 z;
} VehicleMotionDirection;

typedef struct VehicleMotionProjectionState
{
  uint8                   reserved_00[0x98];
  int32                   vector_x;
  int32                   vector_y;
  int32                   vector_z;
  uint8                   reserved_a4[0x10];
  VehicleMotionDirection *direction;
} VehicleMotionProjectionState;

int32
calculate_vehicle_motion_projection(const VehicleMotionProjectionState *state_);
