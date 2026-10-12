#include "fixed_math.h"

#include "item_lookup.h"
#include "string_utilities.h"

#include "folio.h"
#include "item.h"
#include "operamath.h"

#include "stdlib.h"

#define FIXED_FRACTION_BITS 16
#define FIXED16_ONE (1 << FIXED_FRACTION_BITS)
#define FIXED16_HALF ((uint32) 1 << (FIXED_FRACTION_BITS - 1))
#define FIXED_RAMP_START 1000
#define FIXED_ROUND_SHIFT (FIXED_FRACTION_BITS - 1)
#define FIXED16_DIVIDE_BY_ZERO_NEGATIVE_RESULT ((int32) 0x80000001)
#define FIXED16_DIVIDE_BY_ZERO_POSITIVE_RESULT ((int32) 0x7fffffff)
#define DECIMAL_POINT '.'
#define DECIMAL_DIGIT_ZERO '0'
#define DECIMAL_DIGIT_NINE '9'
#define DECIMAL_SIGN_MINUS '-'
#define DECIMAL_SPACE ' '
#define FRACTION_DIGIT_LIMIT 4
#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

COMPILE_TIME_SIZE_ASSERT(int32_size, sizeof(int32) == 4);

const int32 G_DECIMAL_POWERS_OF_TEN[10] = { 1,      10,      100,      1000,      10000,
                                            100000, 1000000, 10000000, 100000000, 1000000000 };

Err
open_math_folio(void)
{
  Item math_folio_item;

  math_folio_item = OpenItem(find_named_item(MKNODEID(KERNELNODE, FOLIONODE), "Operamath"), NULL);
  if(math_folio_item < 0)
    return math_folio_item;

  _MathBase = (MathFolio *) LookupItem(math_folio_item);
  return 0;
}

int32
integer_to_fixed16(int32 value_)
{
  return value_ << FIXED_FRACTION_BITS;
}

int32
fixed16_to_integer(int32 value_)
{
  if(value_ < 0)
    {
      value_ -= 1;
      value_ += (1 << FIXED_FRACTION_BITS);
    }
  return value_ >> FIXED_FRACTION_BITS;
}

int32
multiply_three_fixed16(int32 first_, int32 second_, int32 third_)
{
  return MulSF16(MulSF16(first_, second_), third_);
}

int32
multiply_four_fixed16(int32 first_, int32 second_, int32 third_, int32 fourth_)
{
  int32 first_product;
  int32 second_product;

  first_product  = MulSF16(first_, second_);
  second_product = MulSF16(third_, fourth_);
  return MulSF16(first_product, second_product);
}

int32
multiply_five_fixed16(int32 first_, int32 second_, int32 third_, int32 fourth_, int32 fifth_)
{
  int32 first_product;
  int32 second_product;

  first_product  = MulSF16(first_, second_);
  second_product = MulSF16(third_, fourth_);
  first_product  = MulSF16(first_product, second_product);
  return MulSF16(first_product, fifth_);
}

int32
multiply_six_fixed16(int32 first_,
                     int32 second_,
                     int32 third_,
                     int32 fourth_,
                     int32 fifth_,
                     int32 sixth_)
{
  int32 first_product;
  int32 second_product;
  int32 third_product;

  first_product  = MulSF16(first_, second_);
  second_product = MulSF16(third_, fourth_);
  third_product  = MulSF16(fifth_, sixth_);
  first_product  = MulSF16(first_product, second_product);
  return MulSF16(first_product, third_product);
}

int32
divide_fixed16_saturating(int32 dividend_, int32 divisor_)
{
  if(divisor_ == 0)
    {
      if(dividend_ <= 0)
        return FIXED16_DIVIDE_BY_ZERO_NEGATIVE_RESULT;
      return FIXED16_DIVIDE_BY_ZERO_POSITIVE_RESULT;
    }
  return DivSF16(dividend_, divisor_);
}

int32
divide_fixed16_or_one(int32 dividend_, int32 divisor_)
{
  if(divisor_ == 0)
    {
      return FIXED16_ONE;
    }

  return DivSF16(dividend_, divisor_);
}

int32
interpolate_fixed16(int32 start_, int32 end_, int32 factor_)
{
  return start_ + multiply_fixed16(end_ - start_, factor_);
}

int32
divide_fixed16_twice(int32 dividend_, int32 first_divisor_, int32 second_divisor_)
{
  return DivSF16(DivSF16(dividend_, first_divisor_), second_divisor_);
}

int32
parse_fixed16_decimal(char *text_)
{
  char *decimal_point;
  char *fraction;
  int32 decimal_started;
  int32 digit_count;
  int32 fixed_fraction;
  int32 fixed_integer;
  int32 leading_zero_count;
  int32 sign;

  while(*text_ == DECIMAL_SPACE)
    text_++;

  if(*text_ == DECIMAL_SIGN_MINUS)
    {
      sign = -1;
      text_++;
    }
  else
    {
      sign = 1;
    }

  decimal_started = (*text_ == DECIMAL_POINT);
  if(decimal_started)
    decimal_point = text_;
  else
    decimal_point = find_character(text_, DECIMAL_POINT);

  if(decimal_point == NULL)
    {
      if(decimal_started)
        return 0;
      return sign * (atoi(text_) << FIXED_FRACTION_BITS);
    }

  fraction       = decimal_point + 1;
  *decimal_point = '\0';
  if(decimal_started)
    fixed_integer = 0;
  else
    fixed_integer = atoi(text_) << FIXED_FRACTION_BITS;

  digit_count = 1;
  while((fraction[digit_count] >= DECIMAL_DIGIT_ZERO) &&
        (fraction[digit_count] <= DECIMAL_DIGIT_NINE))
    {
      digit_count++;
    }

  leading_zero_count = 0;
  while(*fraction == DECIMAL_DIGIT_ZERO)
    {
      leading_zero_count++;
      fraction++;
    }

  if((leading_zero_count + FRACTION_DIGIT_LIMIT) < digit_count)
    {
      fraction[leading_zero_count + FRACTION_DIGIT_LIMIT] = '\0';
      digit_count = leading_zero_count + FRACTION_DIGIT_LIMIT;
    }

  fixed_fraction = (atoi(fraction) << FIXED_FRACTION_BITS) / G_DECIMAL_POWERS_OF_TEN[digit_count];
  return sign * (fixed_integer + fixed_fraction);
}

int32
multiply_fixed16_rounded(int32 left_, int32 right_)
{
  uint32 product;

  product = (uint32) left_ * (uint32) right_;
  return (int32) (product + FIXED16_HALF) >> FIXED_FRACTION_BITS;
}

int32
multiply_fixed16(int32 left_, int32 right_)
{
  int32  left_integer;
  int32  right_integer;
  uint32 left_fraction;
  uint32 right_fraction;
  uint32 fraction_product;

  left_integer     = left_ >> FIXED_FRACTION_BITS;
  right_integer    = right_ >> FIXED_FRACTION_BITS;
  left_fraction    = (uint32) left_ & ~((uint32) left_integer << FIXED_FRACTION_BITS);
  right_fraction   = (uint32) right_ & ~((uint32) right_integer << FIXED_FRACTION_BITS);
  fraction_product = left_fraction * right_fraction;

  return (int32) (left_fraction * (uint32) right_integer) +
         (int32) ((uint32) left_integer * right_fraction) +
         (int32) (fraction_product >> FIXED_FRACTION_BITS) +
         (int32) ((uint32) (left_integer * right_integer) << FIXED_FRACTION_BITS) +
         (int32) ((uint8) (fraction_product >> FIXED_ROUND_SHIFT) & 1);
}

void
fill_fixed16_ramp(int32 value_count_, int32 *values_)
{
  int32 index;

  for(index = 0; index < value_count_; index++)
    {
      values_[index] = (FIXED_RAMP_START + (index << FIXED_FRACTION_BITS));
    }
}
