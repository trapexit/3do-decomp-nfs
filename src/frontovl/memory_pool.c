#include "memory_pool.h"

#include "tracked_memory.h"

#define MEMORY_POOL_ALIGNMENT_PADDING 3U
#define MEMORY_POOL_ALIGNMENT_MASK 0xFFFFFFFCU
#define MEMORY_POOL_NAME "mempool"

MemoryPool *
create_memory_pool(int32 item_count_, int32 entry_size_)
{
  int32            actual_entry_size;
  int32            pool_byte_count;
  MemoryPool      *pool;
  MemoryPoolEntry *entry;
  MemoryPoolEntry *previous_entry;

  actual_entry_size =
    (int32) (((uint32) entry_size_ + MEMORY_POOL_ALIGNMENT_PADDING) & MEMORY_POOL_ALIGNMENT_MASK);
  actual_entry_size += sizeof(MemoryPoolEntry *);
  pool_byte_count = sizeof(MemoryPool) + item_count_ * actual_entry_size;
  pool = (MemoryPool *) ALLOCATE_NAMED_SECONDARY_MEMORY(pool_byte_count, MEMORY_POOL_NAME);

  if(pool != NULL)
    {
      pool->item_count = item_count_;
      pool->free_count = item_count_;
      entry            = (MemoryPoolEntry *) pool->data;
      previous_entry   = NULL;

      while(item_count_-- > 0)
        {
          entry->next    = previous_entry;
          previous_entry = entry;
          entry          = (MemoryPoolEntry *) ((uint8 *) entry + actual_entry_size);
        }

      pool->available_entries = previous_entry;
    }

  return pool;
}

void
return_memory_pool_entry(MemoryPool *pool_, void *pool_entry_)
{
  MemoryPoolEntry *entry;

  entry       = (MemoryPoolEntry *) ((uint8 *) pool_entry_ - sizeof(MemoryPoolEntry *));
  entry->next = pool_->available_entries;
  pool_->available_entries = entry;
  pool_->free_count += 1;
}

int32
for_each_free_memory_pool_entry(MemoryPool             *pool_,
                                MemoryPoolEntryFunction entry_function_,
                                void                   *argument_)
{
  MemoryPoolEntry *entry;

  for(entry = pool_->available_entries; entry != NULL; entry = entry->next)
    {
      if(!entry_function_(argument_, entry->data))
        {
          return 0;
        }
    }

  return 1;
}
