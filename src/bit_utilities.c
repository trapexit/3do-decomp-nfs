#include "bit_utilities.h"

#define BIT_INDEX_HALF_WORD_BITS 16
#define BIT_INDEX_BYTE_BITS 8
#define BIT_INDEX_NIBBLE_BITS 4
#define BIT_INDEX_PAIR_BITS 2

int32
get_highest_set_bit_index(uint32 value_)
{
  int32  bit_index;
  uint32 upper_bits;

  bit_index  = 0;
  upper_bits = (value_ >> BIT_INDEX_HALF_WORD_BITS);
  if(upper_bits != 0)
    {
      bit_index += BIT_INDEX_HALF_WORD_BITS;
      value_ = upper_bits;
    }
  upper_bits = (value_ >> BIT_INDEX_BYTE_BITS);
  if(upper_bits != 0)
    {
      bit_index += BIT_INDEX_BYTE_BITS;
      value_ = upper_bits;
    }
  upper_bits = (value_ >> BIT_INDEX_NIBBLE_BITS);
  if(upper_bits != 0)
    {
      bit_index += BIT_INDEX_NIBBLE_BITS;
      value_ = upper_bits;
    }
  upper_bits = (value_ >> BIT_INDEX_PAIR_BITS);
  if(upper_bits != 0)
    {
      bit_index += BIT_INDEX_PAIR_BITS;
      value_ = upper_bits;
    }
  upper_bits = (value_ >> 1);
  if(upper_bits != 0)
    bit_index++;

  return bit_index;
}
