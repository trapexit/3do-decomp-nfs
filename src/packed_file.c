#include "packed_file.h"

#include "file_information.h"
#include "packed_format.h"

#define PACKED_FILE_HEADER_MIN_BYTES 8
#define PACKED_FILE_HEADER_MAX_BYTES 256

int32
get_packed_file_data_size(int32 handle_, int32 offset_, int32 byte_count_)
{
  uint8 header[PACKED_FILE_HEADER_MAX_BYTES];
  int32 header_byte_count;
  int32 result;

  result = 0;
  seek_file_information(handle_, offset_);
  header_byte_count = byte_count_ - PACKED_FILE_HEADER_MIN_BYTES;
  if(header_byte_count > PACKED_FILE_HEADER_MAX_BYTES)
    header_byte_count = PACKED_FILE_HEADER_MAX_BYTES;
  if(header_byte_count <= PACKED_FILE_HEADER_MIN_BYTES)
    header_byte_count = PACKED_FILE_HEADER_MIN_BYTES;

  if(read_file_information(handle_, header, header_byte_count) != 0)
    result = get_packed_data_size(header, byte_count_);

  seek_file_information(handle_, offset_);
  return result;
}
