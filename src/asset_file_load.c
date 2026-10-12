#include "asset_file_load.h"

#include "asset_decompression.h"
#include "file_information.h"
#include "memory.h"
#include "packed_file.h"

#define ASSET_FILE_ALIGNMENT_PADDING 3U
#define ASSET_FILE_ALIGNMENT_MASK 0xFFFFFFFCU
#define ASSET_FILE_ALLOCATION_OVERHEAD 1003

static int32
_align_asset_file_byte_count(int32 byte_count_)
{
  return (int32) (((uint32) byte_count_ + ASSET_FILE_ALIGNMENT_PADDING) &
                  ASSET_FILE_ALIGNMENT_MASK);
}

MemoryBlockDescriptor *
load_asset_file_record(const char *path_, uint32 memory_flags_, int32 report_error_)
{
  int32                  allocation_byte_count;
  int32                  file_byte_count;
  int32                  file_handle;
  int32                  file_offset;
  MemoryBlockDescriptor *record;
  uint8                 *staging_buffer;
  int32                  unpacked_byte_count;

  record = acquire_named_memory_record(path_);
  if(record != NULL)
    return record;

  open_file_information(path_, &file_handle, &file_offset, &file_byte_count, report_error_);
  if(file_byte_count == 0)
    return NULL;

  unpacked_byte_count = get_packed_file_data_size(file_handle, file_offset, file_byte_count);
  if(unpacked_byte_count == 0)
    {
      close_file_information(file_handle);
      return NULL;
    }

  if(unpacked_byte_count > file_byte_count)
    allocation_byte_count = unpacked_byte_count;
  else
    allocation_byte_count = file_byte_count;
  allocation_byte_count =
    (int32) (((uint32) allocation_byte_count + ASSET_FILE_ALLOCATION_OVERHEAD) &
             ASSET_FILE_ALIGNMENT_MASK);
  record =
    allocate_memory_record_locked(path_, allocation_byte_count, memory_flags_, report_error_);
  if(record == NULL)
    {
      close_file_information(file_handle);
      return NULL;
    }

  staging_buffer = (uint8 *) record->payload + allocation_byte_count -
                   _align_asset_file_byte_count(file_byte_count);
  if(read_file_information(file_handle,
                           staging_buffer,
                           _align_asset_file_byte_count(file_byte_count)) == 0)
    {
      close_file_information(file_handle);
      return NULL;
    }

  decode_packed_data(staging_buffer, (uint8 *) record->payload, file_byte_count, report_error_);
  resize_memory_record(record, unpacked_byte_count, report_error_);
  close_file_information(file_handle);
  return record;
}

void *
load_asset_file_with_error_reporting(const char *path_, uint32 memory_flags_)
{
  return load_asset_file(path_, memory_flags_, 1);
}

void *
load_asset_file_without_error_reporting(const char *path_, uint32 memory_flags_)
{
  return load_asset_file(path_, memory_flags_, 0);
}

void *
load_asset_file(const char *path_, uint32 memory_flags_, int32 report_error_)
{
  MemoryBlockDescriptor *record;

  record = load_asset_file_record(path_, memory_flags_, report_error_);
  if(record == NULL)
    return NULL;

  return record->payload;
}
