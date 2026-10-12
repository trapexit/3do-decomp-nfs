#pragma once

#include "mem.h"

void *
allocate_memory_from_list(MemList *memory_list_, int32 byte_count_, uint32 memory_flags_);
void
free_memory_to_list(MemList *memory_list_, void *memory_, int32 byte_count_);
MemList *
allocate_memory_list(const void *memory_, const char *name_);
void
free_memory_list(MemList *memory_list_);
void
free_memory_to_lists(List *memory_lists_, void *memory_, int32 byte_count_);
