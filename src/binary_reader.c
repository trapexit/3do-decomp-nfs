#include "binary_reader.h"

#define BINARY_BYTE_BITS 8

uint32
read_big_endian_unsigned(const uint8 *source_, int32 byte_count_)
{
  uint32 value;

  value = 0;
  while(byte_count_ != 0)
    {
      value = ((value << BINARY_BYTE_BITS) | (uint32) *source_++);
      byte_count_--;
    }

  return value;
}
