#include "dsp_memory.h"

#include "tracked_memory.h"

#define DSP_ALLOCATION_NAME "DSP"

void *
allocate_dsp_memory(uint32 byte_count_)
{
  return allocate_named_primary_memory(byte_count_, DSP_ALLOCATION_NAME);
}

void
free_dsp_memory(void *memory_)
{
  free_primary_memory(memory_);
}
