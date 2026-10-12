/* SDK-origin fixed-point sine and cosine table interpolation. */

#include "operamath_tables.h"

#include "operamath.h"

#define OPERAMATH_ANGLE_QUARTER_TURN 0x00400000U
#define OPERAMATH_ANGLE_HALF_TURN 0x00800000U
#define OPERAMATH_ANGLE_PERIOD_MASK 0x00FFFFFFU
#define OPERAMATH_SINE_INDEX_SHIFT 10
#define OPERAMATH_SINE_FRACTION_MASK 0x03FFU
#define OPERAMATH_SINE_FRACTION_ONE 0x0400U
#define OPERAMATH_SINE_TABLE_SCALE_SHIFT 10
#define OPERAMATH_SINE_RESULT_SHIFT 15

frac16
SinF16(frac16 angle_)
{
  uint32 phase;
  uint32 fraction;
  uint32 index;
  uint32 current_value;
  uint32 next_value;
  uint32 result;
  int32  negative;

  phase = (uint32) angle_;
  if((phase & OPERAMATH_ANGLE_QUARTER_TURN) != 0)
    phase = OPERAMATH_ANGLE_HALF_TURN - phase;
  phase &= OPERAMATH_ANGLE_PERIOD_MASK;

  negative = phase >= OPERAMATH_ANGLE_HALF_TURN;
  if(negative)
    phase -= OPERAMATH_ANGLE_HALF_TURN;

  index         = phase >> OPERAMATH_SINE_INDEX_SHIFT;
  fraction      = phase & OPERAMATH_SINE_FRACTION_MASK;
  current_value = ((uint32) sintable[index]) >> OPERAMATH_SINE_TABLE_SCALE_SHIFT;
  next_value    = ((uint32) sintable[index + 1]) >> OPERAMATH_SINE_TABLE_SCALE_SHIFT;
  result = (current_value * (OPERAMATH_SINE_FRACTION_ONE - fraction)) + (next_value * fraction);
  result >>= OPERAMATH_SINE_RESULT_SHIFT;

  if(negative)
    return -(frac16) result;
  return (frac16) result;
}

frac16
CosF16(frac16 angle_)
{
  return SinF16((frac16) ((uint32) angle_ + OPERAMATH_ANGLE_QUARTER_TURN));
}
