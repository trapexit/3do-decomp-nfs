/* Frontovl data-acquisition context-pool lifecycle. */

#include "data_acquisition_context_pool.h"

MemoryPool *g_STREAM_REQUEST_POOL = NULL;

int32
close_stream_request_pool(void)
{
  delete_memory_pool(g_STREAM_REQUEST_POOL);
  g_STREAM_REQUEST_POOL = NULL;
  return 0;
}
