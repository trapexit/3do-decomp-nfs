#include "file_load_options.h"

#include "file_information.h"
#include "memory.h"

static void *
_load_file_with_options_reported(const char *path_, int32 options_, int32 report_error_)
{
  MemoryBlockDescriptor *record;

  record = load_file_record_with_options(path_, options_, report_error_);
  if(record == NULL)
    return NULL;

  return record->payload;
}

void *
load_file_with_options(const char *path_, int32 options_)
{
  return _load_file_with_options_reported(path_, options_, 0);
}

void *
load_file_with_options_with_error(const char *path_, int32 options_)
{
  return _load_file_with_options_reported(path_, options_, 1);
}

MemoryBlockDescriptor *
load_file_record_with_options(const char *path_, int32 options_, int32 report_error_)
{
  int32                  byte_count;
  int32                  file_handle;
  int32                  file_offset;
  MemoryBlockDescriptor *record;

  record = acquire_named_memory_record(path_);
  if(record != NULL)
    return record;

  open_file_information(path_, &file_handle, &file_offset, &byte_count, report_error_);
  if(byte_count == 0)
    return NULL;

  record = allocate_memory_record_locked(path_, byte_count, options_, report_error_);
  if(record == NULL)
    {
      close_file_information(file_handle);
      return NULL;
    }

  read_file_information(file_handle, record->payload, byte_count);
  close_file_information(file_handle);
  return record;
}
