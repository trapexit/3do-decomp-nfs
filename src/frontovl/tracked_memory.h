#pragma once

#include "types.h"

#define PRIMARY_ALLOCATION_CAPACITY 40
#define SECONDARY_ALLOCATION_CAPACITY 60

/* Historical stream libraries link these tracked allocators by SDK names. */
#define ALLOCATE_NAMED_SECONDARY_MEMORY NewPtr
#define FREE_SECONDARY_MEMORY FreePtr

typedef struct MemoryTracker
{
  void **primary_allocations;
  void **secondary_allocations;
  int32  primary_allocation_count;
  int32  secondary_allocation_count;
} MemoryTracker;

extern MemoryTracker g_MEMORY_TRACKER;

void
initialize_memory_tracker(void);
void
track_primary_allocation(void *memory_);
void
untrack_primary_allocation(void *memory_);
void
release_all_primary_allocations(void);
void
free_primary_memory(void *memory_);
void *
allocate_primary_memory(uint32 byte_count_);
void *
allocate_named_primary_memory(uint32 byte_count_, const char *name_);
void
track_secondary_allocation(void *memory_);
void
untrack_secondary_allocation(void *memory_);
void
release_all_secondary_allocations(void);
void
FreePtr(void *memory_);
void *
allocate_secondary_memory(uint32 byte_count_);
void *
NewPtr(uint32 byte_count_, const char *name_);
void *
malloc(int32 byte_count_);
void
free(void *memory_);
