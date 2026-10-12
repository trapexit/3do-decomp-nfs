/* Frontovl kernel memory-allocation entry point. */

#include "kernel_memory.h"

void *
allocate_memory_from_lists(List *lists_, int32 byte_count_, uint32 type_bits_)
{
  return AllocMemFromMemLists(lists_, byte_count_, type_bits_);
}
