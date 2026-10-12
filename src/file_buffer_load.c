#include "file_buffer_load.h"

#include "file_information.h"
#include "memory.h"

void *
load_file_into_buffer_reported(const char *path_, void *buffer_, int32 report_error_)
{
  int32 file_offset;
  int32 file_handle;
  int32 file_size;
  void *cached_memory;

  cached_memory = acquire_named_memory(path_);
  if(cached_memory != NULL)
    {
      move_bytes((const uint8 *) cached_memory,
                 (uint8 *) buffer_,
                 get_memory_payload_byte_count(cached_memory));
      free_memory(cached_memory);
      return cached_memory;
    }

  open_file_information(path_, &file_handle, &file_offset, &file_size, report_error_);
  if(file_size == 0)
    return NULL;

  read_file_information(file_handle, buffer_, file_size);
  close_file_information(file_handle);
  return buffer_;
}

void *
load_file_into_buffer_with_error(const char *path_, void *buffer_)
{
  return load_file_into_buffer_reported(path_, buffer_, 1);
}

void *
load_file_into_buffer_quiet(const char *path_, void *buffer_)
{
  return load_file_into_buffer_reported(path_, buffer_, 0);
}
