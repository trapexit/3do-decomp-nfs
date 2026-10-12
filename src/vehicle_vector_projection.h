#pragma once

#include "types.h"

typedef struct VehicleVectorProjectionState
{
  uint8 reserved_00[0x48];
  int32 first_x;
  int32 first_y;
  int32 first_z;
  uint8 reserved_54[0x44];
  int32 second_x;
  int32 second_y;
  int32 second_z;
} VehicleVectorProjectionState;

int32
calculate_negative_x_vector_dot_product_fixed8(const VehicleVectorProjectionState *state_);
