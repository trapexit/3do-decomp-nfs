#include "geometry.h"

#include "operamath.h"

#define ANGLE24_FULL_CIRCLE 0x01000000
#define ANGLE24_MASK 0x00FFFFFF
#define ANGLE24_QUARTER_CIRCLE 0x00400000
#define ANGLE24_HALF_CIRCLE 0x00800000
#define ANGLE24_THREE_QUARTER_CIRCLE 0x00C00000

int32
normalize_angle24(int32 angle_)
{
  if(angle_ < 0)
    {
      angle_ += ANGLE24_FULL_CIRCLE;
    }

  return angle_ & ANGLE24_MASK;
}

int32
fold_angle24_to_quadrant(int32 angle_)
{
  if(angle_ < ANGLE24_QUARTER_CIRCLE)
    return angle_;
  if(angle_ < ANGLE24_HALF_CIRCLE)
    return ANGLE24_HALF_CIRCLE - angle_;
  if(angle_ < ANGLE24_THREE_QUARTER_CIRCLE)
    return angle_ - ANGLE24_HALF_CIRCLE;
  return ANGLE24_FULL_CIRCLE - angle_;
}

int32
calculate_angle_from_y_and_negative_x_f16(int32 x_, int32 y_)
{
  return Atan2F16(y_, -x_);
}
