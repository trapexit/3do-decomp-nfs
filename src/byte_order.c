#include "byte_order.h"

#define BYTE_INDEX_THIRD 2
#define BYTE_INDEX_FOURTH 3

typedef union ByteOrderWord
{
  uint32 value;
  uint8  bytes[4];
} ByteOrderWord;

void
swap_byte_pair(uint8 *bytes_)
{
  uint8 first_byte;

  first_byte = bytes_[0];
  bytes_[0]  = bytes_[1];
  bytes_[1]  = first_byte;
}

uint32
read_little_endian_uint32(const uint8 *bytes_)
{
  ByteOrderWord word;

  word.bytes[0]                 = bytes_[BYTE_INDEX_FOURTH];
  word.bytes[1]                 = bytes_[BYTE_INDEX_THIRD];
  word.bytes[BYTE_INDEX_THIRD]  = bytes_[1];
  word.bytes[BYTE_INDEX_FOURTH] = bytes_[0];

  return word.value;
}
