/* Frontovl runtime byte-fill operation. */

#include "runtime_memory_ops.h"

#include "string.h"

void *
fill_runtime_memory(void *destination_, int32 value_, uint32 byte_count_)
{
  return memset(destination_, value_, byte_count_);
}

void *
copy_runtime_memory(void *destination_, const void *source_, uint32 byte_count_)
{
  return memcpy(destination_, source_, byte_count_);
}
