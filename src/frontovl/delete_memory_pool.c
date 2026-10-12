#include "memory_pool.h"
#include "tracked_memory.h"

void
delete_memory_pool(MemoryPool *pool_)
{
  volatile int32 release_completed;

  release_completed = 0;
  FREE_SECONDARY_MEMORY(pool_);
  release_completed = 1;
  (void) release_completed;
}
