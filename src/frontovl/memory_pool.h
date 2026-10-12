#pragma once

#include "types.h"

#define MEMORY_POOL_DATA_SIZE 4

typedef struct MemoryPoolEntry
{
  struct MemoryPoolEntry *next;
  uint8                   data[MEMORY_POOL_DATA_SIZE];
} MemoryPoolEntry;

typedef struct MemoryPool
{
  int32            item_count;
  int32            free_count;
  MemoryPoolEntry *available_entries;
  uint8            data[MEMORY_POOL_DATA_SIZE];
} MemoryPool;

typedef int32 (*MemoryPoolEntryFunction)(void *argument_, void *pool_entry_);

MemoryPool *
create_memory_pool(int32 item_count_, int32 entry_size_);
void
delete_memory_pool(MemoryPool *pool_);
void *
allocate_memory_pool_entry(MemoryPool *pool_);
void
return_memory_pool_entry(MemoryPool *pool_, void *pool_entry_);
int32
for_each_free_memory_pool_entry(MemoryPool             *pool_,
                                MemoryPoolEntryFunction entry_function_,
                                void                   *argument_);
