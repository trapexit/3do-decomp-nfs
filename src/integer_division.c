#include "integer_division.h"

/* The original alternate entry multiplies before entering signed division. */
int32
multiply_divide_signed_integer(int32 multiplicand_, int32 multiplier_, int32 divisor_)
{
  uint32 product;

  product = (uint32) multiplicand_ * (uint32) multiplier_;

  return divide_signed_integer((int32) product, divisor_);
}
