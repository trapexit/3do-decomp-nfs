#include "vehicle_motion_projection.h"

#define FIXED8_FRACTION_BITS 8

int32
calculate_vehicle_motion_projection(const VehicleMotionProjectionState *state_)
{
  int32 projection;

  projection =
    (state_->vector_x >> FIXED8_FRACTION_BITS) * (state_->direction->x >> FIXED8_FRACTION_BITS);
  projection +=
    (state_->vector_y >> FIXED8_FRACTION_BITS) * (state_->direction->y >> FIXED8_FRACTION_BITS);
  projection +=
    (state_->vector_z >> FIXED8_FRACTION_BITS) * -(state_->direction->z >> FIXED8_FRACTION_BITS);
  return projection;
}
