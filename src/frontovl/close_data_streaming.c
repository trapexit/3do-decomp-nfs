#include "data_stream_context_pool.h"

int32
close_data_streaming(void)
{
  delete_memory_pool(g_DATA_STREAM_CONTEXT_POOL);
  g_DATA_STREAM_CONTEXT_POOL = NULL;
  return 0;
}
