/* Frontovl wrappers for kernel memory-list management. */

#include "kernel_memory_list.h"

void *
_AllocMemFromMemList(MemList *memory_list_, int32 byte_count_, uint32 memory_flags_);
void
_FreeMemToMemList(MemList *memory_list_, void *memory_, int32 byte_count_);
MemList *
_AllocMemList(const void *memory_, const char *name_);
void
_FreeMemList(MemList *memory_list_);

void *
allocate_memory_from_list(MemList *memory_list_, int32 byte_count_, uint32 memory_flags_)
{
  return _AllocMemFromMemList(memory_list_, byte_count_, memory_flags_);
}

void
free_memory_to_list(MemList *memory_list_, void *memory_, int32 byte_count_)
{
  _FreeMemToMemList(memory_list_, memory_, byte_count_);
}

MemList *
allocate_memory_list(const void *memory_, const char *name_)
{
  return _AllocMemList(memory_, name_);
}

void
free_memory_list(MemList *memory_list_)
{
  _FreeMemList(memory_list_);
}

void
free_memory_to_lists(List *memory_lists_, void *memory_, int32 byte_count_)
{
  FreeMemToMemLists(memory_lists_, memory_, byte_count_);
}
