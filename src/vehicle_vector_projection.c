#include "vehicle_vector_projection.h"

#define FIXED8_FRACTION_BITS 8

int32
calculate_negative_x_vector_dot_product_fixed8(const VehicleVectorProjectionState *state_)
{
  int32  first_x;
  int32  first_y;
  int32  first_z;
  int32  second_x;
  int32  second_y;
  int32  second_z;
  uint32 result;

  first_x  = (int32) (0 - (uint32) (state_->first_x >> FIXED8_FRACTION_BITS));
  first_y  = state_->first_y >> FIXED8_FRACTION_BITS;
  first_z  = state_->first_z >> FIXED8_FRACTION_BITS;
  second_x = state_->second_x >> FIXED8_FRACTION_BITS;
  second_y = state_->second_y >> FIXED8_FRACTION_BITS;
  second_z = state_->second_z >> FIXED8_FRACTION_BITS;
  result   = (uint32) first_x * (uint32) second_x;
  result += (uint32) first_y * (uint32) second_y;
  result += (uint32) first_z * (uint32) second_z;
  return (int32) result;
}
