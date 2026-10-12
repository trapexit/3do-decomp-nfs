#include "tracked_memory.h"

#include "allocation.h"
#include "runtime_memory.h"

#define MEMORY_ALLOCATION_FLAGS 0
#define NO_ALLOCATION_LIST ((void **) 0)
#define NO_ALLOCATION_COUNT ((int32) 0)

MemoryTracker g_MEMORY_TRACKER = { NO_ALLOCATION_LIST,
                                   NO_ALLOCATION_LIST,
                                   NO_ALLOCATION_COUNT,
                                   NO_ALLOCATION_COUNT };

void
track_primary_allocation(void *memory_)
{
  int32 index;

  for(index = 0; index < PRIMARY_ALLOCATION_CAPACITY; index++)
    {
      if(g_MEMORY_TRACKER.primary_allocations[index] == NULL)
        {
          g_MEMORY_TRACKER.primary_allocations[index] = memory_;
          g_MEMORY_TRACKER.primary_allocation_count++;
          return;
        }
    }
}

void
untrack_primary_allocation(void *memory_)
{
  int32 index;

  for(index = 0; index < PRIMARY_ALLOCATION_CAPACITY; index++)
    {
      if(g_MEMORY_TRACKER.primary_allocations[index] == memory_)
        {
          g_MEMORY_TRACKER.primary_allocations[index] = NULL;
          g_MEMORY_TRACKER.primary_allocation_count--;
          return;
        }
    }
}

void
release_all_primary_allocations(void)
{
  int32 index;

  for(index = 0; index < PRIMARY_ALLOCATION_CAPACITY; index++)
    {
      if(g_MEMORY_TRACKER.primary_allocations[index] != NULL)
        {
          free_runtime_memory(g_MEMORY_TRACKER.primary_allocations[index]);
          g_MEMORY_TRACKER.primary_allocation_count--;
          g_MEMORY_TRACKER.primary_allocations[index] = NULL;
        }
    }
}

void
free_primary_memory(void *memory_)
{
  if(memory_ == NULL)
    {
      return;
    }

  untrack_primary_allocation(memory_);
  free_runtime_memory(memory_);
}

void *
allocate_primary_memory(uint32 byte_count_)
{
  void *memory;

  memory = allocate_runtime_memory("AllocMem1", byte_count_, MEMORY_ALLOCATION_FLAGS);
  track_primary_allocation(memory);
  return memory;
}

void *
allocate_named_primary_memory(uint32 byte_count_, const char *name_)
{
  void *memory;

  memory = allocate_runtime_memory(name_, byte_count_, MEMORY_ALLOCATION_FLAGS);
  track_primary_allocation(memory);
  return memory;
}

void
track_secondary_allocation(void *memory_)
{
  int32 index;

  for(index = 0; index < SECONDARY_ALLOCATION_CAPACITY; index++)
    {
      if(g_MEMORY_TRACKER.secondary_allocations[index] == NULL)
        {
          g_MEMORY_TRACKER.secondary_allocations[index] = memory_;
          g_MEMORY_TRACKER.secondary_allocation_count++;
          return;
        }
    }
}

void
untrack_secondary_allocation(void *memory_)
{
  int32 index;

  for(index = 0; index < SECONDARY_ALLOCATION_CAPACITY; index++)
    {
      if(g_MEMORY_TRACKER.secondary_allocations[index] == memory_)
        {
          g_MEMORY_TRACKER.secondary_allocations[index] = NULL;
          g_MEMORY_TRACKER.secondary_allocation_count--;
          return;
        }
    }
}

void
release_all_secondary_allocations(void)
{
  int32 index;

  for(index = SECONDARY_ALLOCATION_CAPACITY - 1; index >= 0; index--)
    {
      if(g_MEMORY_TRACKER.secondary_allocations[index] != NULL)
        {
          free_runtime_memory(g_MEMORY_TRACKER.secondary_allocations[index]);
          g_MEMORY_TRACKER.secondary_allocation_count--;
          g_MEMORY_TRACKER.secondary_allocations[index] = NULL;
        }
    }
}

void
FreePtr(void *memory_)
{
  if(memory_ == NULL)
    {
      return;
    }

  untrack_secondary_allocation(memory_);
  free_runtime_memory(memory_);
}

void *
allocate_secondary_memory(uint32 byte_count_)
{
  void *memory;

  memory = allocate_runtime_memory("AllocMem2", byte_count_, MEMORY_ALLOCATION_FLAGS);
  track_secondary_allocation(memory);
  return memory;
}

void *
NewPtr(uint32 byte_count_, const char *name_)
{
  void *memory;

  memory = allocate_runtime_memory(name_, byte_count_, MEMORY_ALLOCATION_FLAGS);
  track_secondary_allocation(memory);
  return memory;
}

void
initialize_memory_tracker(void)
{
  int32 index;

  g_MEMORY_TRACKER.primary_allocations =
    (void **) allocate_runtime_memory("MemPtrs",
                                      PRIMARY_ALLOCATION_CAPACITY * sizeof(void *),
                                      MEMORY_ALLOCATION_FLAGS);
  g_MEMORY_TRACKER.secondary_allocations =
    (void **) allocate_runtime_memory("MemPtrs2",
                                      SECONDARY_ALLOCATION_CAPACITY * sizeof(void *),
                                      MEMORY_ALLOCATION_FLAGS);

  for(index = 0; index < PRIMARY_ALLOCATION_CAPACITY; index++)
    {
      g_MEMORY_TRACKER.primary_allocations[index] = NULL;
    }
  for(index = 0; index < SECONDARY_ALLOCATION_CAPACITY; index++)
    {
      g_MEMORY_TRACKER.secondary_allocations[index] = NULL;
    }

  g_MEMORY_TRACKER.primary_allocation_count   = 0;
  g_MEMORY_TRACKER.secondary_allocation_count = 0;
  track_primary_allocation(g_MEMORY_TRACKER.primary_allocations);
  track_primary_allocation(g_MEMORY_TRACKER.secondary_allocations);
}

void *
malloc(int32 byte_count_)
{
  if(g_ALLOCATION_MODE == ALLOCATION_MODE_PRIMARY)
    return allocate_primary_memory((uint32) byte_count_);

  if(g_ALLOCATION_MODE == ALLOCATION_MODE_SECONDARY)
    return allocate_secondary_memory((uint32) byte_count_);

  return NULL;
}

void
free(void *memory_)
{
  if(g_ALLOCATION_MODE == ALLOCATION_MODE_PRIMARY)
    {
      free_primary_memory(memory_);
    }
  else if(g_ALLOCATION_MODE == ALLOCATION_MODE_SECONDARY)
    {
      FREE_SECONDARY_MEMORY(memory_);
    }
}
