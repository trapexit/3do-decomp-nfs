#include "byte_range.h"

void
clear_byte_range(uint8 *destination_, int32 byte_count_)
{
  int32 byte_index;

  for(byte_index = 0; byte_index < byte_count_; byte_index += 1)
    {
      destination_[byte_index] = 0;
    }
}

void
copy_byte_range_forward(const uint8 *source_, uint8 *destination_, int32 byte_count_)
{
  while(byte_count_ > 0)
    {
      *destination_++ = *source_++;
      byte_count_ -= 1;
    }
}
