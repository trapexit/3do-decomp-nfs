#pragma once

#include "base_types.h"

extern const int32 G_DECIMAL_POWERS_OF_TEN[10];

Err
open_math_folio(void);
int32
multiply_fixed16_rounded(int32 left_, int32 right_);
int32
multiply_fixed16(int32 left_, int32 right_);
int32
integer_to_fixed16(int32 value_);
int32
fixed16_to_integer(int32 value_);
int32
multiply_three_fixed16(int32 first_, int32 second_, int32 third_);
int32
multiply_four_fixed16(int32 first_, int32 second_, int32 third_, int32 fourth_);
int32
multiply_five_fixed16(int32 first_, int32 second_, int32 third_, int32 fourth_, int32 fifth_);
int32
multiply_six_fixed16(int32 first_,
                     int32 second_,
                     int32 third_,
                     int32 fourth_,
                     int32 fifth_,
                     int32 sixth_);
int32
divide_fixed16_saturating(int32 dividend_, int32 divisor_);
int32
divide_fixed16_or_one(int32 dividend_, int32 divisor_);
int32
interpolate_fixed16(int32 start_, int32 end_, int32 factor_);
int32
divide_fixed16_twice(int32 dividend_, int32 first_divisor_, int32 second_divisor_);
int32
parse_fixed16_decimal(char *text_);
void
fill_fixed16_ramp(int32 value_count_, int32 *values_);
