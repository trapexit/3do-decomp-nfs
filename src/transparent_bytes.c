#include "transparent_bytes.h"

#define TRANSPARENT_BYTE 0xFF

void
copy_non_transparent_bytes(const uint8 *source_, uint8 *destination_, uint32 byte_count_)
{
  uint8  source_byte;
  uint8 *destination_end;

  if(byte_count_ == 0)
    return;

  destination_end = destination_ + byte_count_;
  do
    {
      source_byte = *source_++;
      if(source_byte != TRANSPARENT_BYTE)
        *destination_ = source_byte;
      destination_++;
    }
  while(destination_ < destination_end);
}
