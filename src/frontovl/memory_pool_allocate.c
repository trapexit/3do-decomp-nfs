#include "memory_pool.h"

void *
allocate_memory_pool_entry(MemoryPool *pool)
{
  MemoryPoolEntry *entry;

  entry = pool->available_entries;
  if (entry == 0)
    return 0;

  pool->available_entries = entry->next;
  pool->free_count -= 1;

  return (void *)entry->data;
}