#include "disk_stream_elements.h"

#include "integer_division.h"

int32
read_disk_stream_elements(void   *destination_,
                          int32   element_size_,
                          int32   element_count_,
                          Stream *stream_)
{
  int32 byte_count;

  byte_count = ReadDiskStream(stream_, (char *) destination_, element_size_ * element_count_);
  return divide_signed_integer(byte_count, element_size_);
}
