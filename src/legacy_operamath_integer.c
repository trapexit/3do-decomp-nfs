/* SDK-origin 64-bit word-pair arithmetic and matrix helpers. */

#include "operamath.h"

#define OPERAMATH_WORD_HALF_BITS 16
#define OPERAMATH_WORD_HALF_MASK 0xFFFFU
#define OPERAMATH_WORD_BITS 32
#define OPERAMATH_U64_BIT_COUNT 64
#define OPERAMATH_SQRT_BIT_COUNT 32
#define OPERAMATH_SQRT_PAIR_BITS 2
#define OPERAMATH_SQRT_PAIR_SHIFT 30
#define OPERAMATH_HIGH_BIT_SHIFT 31
#define OPERAMATH_MATRIX_3_ROWS 3
#define OPERAMATH_MATRIX_4_ROWS 4

static uint64
_shift_u64_left(uint64 value_, uint32 bit_count_)
{
  uint64 shifted;

  if(bit_count_ == 0)
    return value_;
  shifted.hi = (value_.hi << bit_count_) | (value_.lo >> (OPERAMATH_WORD_BITS - bit_count_));
  shifted.lo = value_.lo << bit_count_;
  return shifted;
}

static int32
_compare_u64(const uint64 *first_, const uint64 *second_)
{
  if(first_->hi < second_->hi)
    return -1;
  if(first_->hi > second_->hi)
    return 1;
  if(first_->lo < second_->lo)
    return -1;
  if(first_->lo > second_->lo)
    return 1;
  return 0;
}

static uint64
_subtract_u64(uint64 first_, const uint64 *second_)
{
  uint64 result;
  uint32 borrow;

  borrow    = first_.lo < second_->lo;
  result.lo = first_.lo - second_->lo;
  result.hi = first_.hi - second_->hi - borrow;
  return result;
}

void
MulS32_64(int64 *product_, int32 first_, int32 second_)
{
  uint32 first_magnitude;
  uint32 second_magnitude;
  uint32 first_low;
  uint32 first_high;
  uint32 second_low;
  uint32 second_high;
  uint32 low_product;
  uint32 middle_product_0;
  uint32 middle_product_1;
  uint32 high_product;
  uint32 carry;
  uint64 result;

  first_magnitude  = first_ < 0 ? 0U - (uint32) first_ : (uint32) first_;
  second_magnitude = second_ < 0 ? 0U - (uint32) second_ : (uint32) second_;
  first_low        = first_magnitude & OPERAMATH_WORD_HALF_MASK;
  first_high       = first_magnitude >> OPERAMATH_WORD_HALF_BITS;
  second_low       = second_magnitude & OPERAMATH_WORD_HALF_MASK;
  second_high      = second_magnitude >> OPERAMATH_WORD_HALF_BITS;

  low_product      = first_low * second_low;
  middle_product_0 = first_high * second_low;
  middle_product_1 = first_low * second_high;
  high_product     = first_high * second_high;

  carry     = (low_product >> OPERAMATH_WORD_HALF_BITS) +
              (middle_product_0 & OPERAMATH_WORD_HALF_MASK) +
              (middle_product_1 & OPERAMATH_WORD_HALF_MASK);
  result.lo = (low_product & OPERAMATH_WORD_HALF_MASK) | (carry << OPERAMATH_WORD_HALF_BITS);
  result.hi = high_product + (middle_product_0 >> OPERAMATH_WORD_HALF_BITS) +
              (middle_product_1 >> OPERAMATH_WORD_HALF_BITS) + (carry >> OPERAMATH_WORD_HALF_BITS);

  if((first_ < 0) != (second_ < 0))
    {
      result.lo = 0U - result.lo;
      result.hi = ~result.hi + (result.lo == 0);
    }

  product_->hi = result.hi;
  product_->lo = result.lo;
}

void
Add64(int64 *result_, int64 *first_, int64 *second_)
{
  uint32 first_low;
  uint32 second_low;
  uint32 low;

  first_low   = first_->lo;
  second_low  = second_->lo;
  low         = first_low + second_low;
  result_->hi = first_->hi + second_->hi + (low < first_low);
  result_->lo = low;
}

uint32
Sqrt64_32(uint64 *value_)
{
  uint64 radicand;
  uint64 remainder;
  uint64 trial;
  uint32 root;
  uint32 pair;
  int32  index;

  radicand     = *value_;
  remainder.hi = 0;
  remainder.lo = 0;
  root         = 0;

  for(index = 0; index < OPERAMATH_SQRT_BIT_COUNT; index += 1)
    {
      pair      = radicand.hi >> OPERAMATH_SQRT_PAIR_SHIFT;
      radicand  = _shift_u64_left(radicand, OPERAMATH_SQRT_PAIR_BITS);
      remainder = _shift_u64_left(remainder, OPERAMATH_SQRT_PAIR_BITS);
      remainder.lo |= pair;

      trial.hi = root >> OPERAMATH_SQRT_PAIR_SHIFT;
      trial.lo = (root << OPERAMATH_SQRT_PAIR_BITS) | 1U;
      root <<= 1;
      if(_compare_u64(&remainder, &trial) >= 0)
        {
          remainder = _subtract_u64(remainder, &trial);
          root |= 1U;
        }
    }

  return root;
}

uint64 *
DivU64(uint64 *quotient_, uint64 *remainder_result_, uint64 *dividend_, uint64 *divisor_)
{
  uint64 remaining_dividend;
  uint64 divisor_value;
  uint64 remainder;
  uint64 result;
  uint32 next_bit;
  int32  index;

  remaining_dividend = *dividend_;
  divisor_value      = *divisor_;
  remainder.hi       = 0;
  remainder.lo       = 0;
  result.hi          = 0;
  result.lo          = 0;

  for(index = 0; index < OPERAMATH_U64_BIT_COUNT; index += 1)
    {
      next_bit           = remaining_dividend.hi >> OPERAMATH_HIGH_BIT_SHIFT;
      remaining_dividend = _shift_u64_left(remaining_dividend, 1);
      remainder          = _shift_u64_left(remainder, 1);
      remainder.lo |= next_bit;
      result = _shift_u64_left(result, 1);
      if(_compare_u64(&remainder, &divisor_value) >= 0)
        {
          remainder = _subtract_u64(remainder, &divisor_value);
          result.lo |= 1U;
        }
    }

  *quotient_         = result;
  *remainder_result_ = remainder;
  return quotient_;
}

void
Transpose33_F16(mat33f16 destination_, mat33f16 source_)
{
  mat33f16 transposed;
  int32    row;
  int32    column;

  for(row = 0; row < OPERAMATH_MATRIX_3_ROWS; row += 1)
    for(column = 0; column < OPERAMATH_MATRIX_3_ROWS; column += 1)
      transposed[row][column] = source_[column][row];
  for(row = 0; row < OPERAMATH_MATRIX_3_ROWS; row += 1)
    for(column = 0; column < OPERAMATH_MATRIX_3_ROWS; column += 1)
      destination_[row][column] = transposed[row][column];
}

void
Transpose44_F16(mat44f16 destination_, mat44f16 source_)
{
  mat44f16 transposed;
  int32    row;
  int32    column;

  for(row = 0; row < OPERAMATH_MATRIX_4_ROWS; row += 1)
    for(column = 0; column < OPERAMATH_MATRIX_4_ROWS; column += 1)
      transposed[row][column] = source_[column][row];
  for(row = 0; row < OPERAMATH_MATRIX_4_ROWS; row += 1)
    for(column = 0; column < OPERAMATH_MATRIX_4_ROWS; column += 1)
      destination_[row][column] = transposed[row][column];
}
