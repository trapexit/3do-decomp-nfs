#include "post_race_speed.h"

#include "runtime_memory.h"

#define FIXED16_FRACTION_MASK 0xFFFFU
#define FIXED16_SHIFT 16
#define SPEED_DECIMAL_SCALE 100
#define SPEED_DECIMAL_ROUNDING 0x8000
#define SPEED_DECIMAL_DIGIT_DIVISOR 10
#define PER_FRAME_TO_MPH_FIXED16 0x00866666
#define PER_SECOND_TO_MPH_FIXED16 0x00023D70
#define SPEED_FORMAT "%3d.%d"

void
format_per_frame_speed_mph(int32 speed_, char *destination_)
{
  int32  fractional_digit;
  int32  rounded_hundredths;
  int32  scaled_speed;
  uint32 fraction;

  scaled_speed = runtime_multiply_fixed16(speed_, PER_FRAME_TO_MPH_FIXED16);
  fraction     = ((uint32) scaled_speed & FIXED16_FRACTION_MASK);
  rounded_hundredths =
    (int32) ((fraction * SPEED_DECIMAL_SCALE + SPEED_DECIMAL_ROUNDING) >> FIXED16_SHIFT);
  fractional_digit =
    ((rounded_hundredths / SPEED_DECIMAL_DIGIT_DIVISOR) % SPEED_DECIMAL_DIGIT_DIVISOR);
  runtime_format_string(destination_,
                        SPEED_FORMAT,
                        (scaled_speed >> FIXED16_SHIFT),
                        fractional_digit);
}

void
format_per_second_speed_mph(int32 speed_, char *destination_)
{
  int32  fractional_digit;
  int32  rounded_hundredths;
  int32  scaled_speed;
  uint32 fraction;

  scaled_speed = runtime_multiply_fixed16(speed_, PER_SECOND_TO_MPH_FIXED16);
  fraction     = ((uint32) scaled_speed & FIXED16_FRACTION_MASK);
  rounded_hundredths =
    (int32) ((fraction * SPEED_DECIMAL_SCALE + SPEED_DECIMAL_ROUNDING) >> FIXED16_SHIFT);
  fractional_digit =
    ((rounded_hundredths / SPEED_DECIMAL_DIGIT_DIVISOR) % SPEED_DECIMAL_DIGIT_DIVISOR);
  runtime_format_string(destination_,
                        SPEED_FORMAT,
                        (scaled_speed >> FIXED16_SHIFT),
                        fractional_digit);
}
