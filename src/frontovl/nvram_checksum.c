#include "nvram_checksum.h"

int32
calculate_nvram_checksum(const void *data_, int32 byte_count_)
{
  int32 byte_index;

  (void) data_;
  for(byte_index = 0; byte_index < byte_count_; byte_index += 1)
    {
    }
  return 0;
}
