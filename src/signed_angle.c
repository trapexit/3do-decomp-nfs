#include "signed_angle.h"

#include "operamath.h"

#define ANGLE_HALF_TURN 0x00800000
#define ANGLE_FULL_TURN 0x01000000

int32
calculate_signed_angle_f16(int32 x_, int32 y_)
{
  int32 angle;

  angle = Atan2F16(x_, y_);
  if(angle >= ANGLE_HALF_TURN)
    {
      angle -= ANGLE_FULL_TURN;
    }
  return angle;
}
