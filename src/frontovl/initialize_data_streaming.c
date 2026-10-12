#include "data_stream_context_pool.h"

MemoryPool *g_DATA_STREAM_CONTEXT_POOL = NULL;

#define STREAM_STATE_SIZE            0x13C
#define STREAM_CONTEXT_POOL_INIT_ERROR (-0x3E0)

int32
initialize_data_streaming(int32 maximum_stream_count_)
{
  g_DATA_STREAM_CONTEXT_POOL = create_memory_pool(maximum_stream_count_,
                                                  STREAM_STATE_SIZE);
  if (g_DATA_STREAM_CONTEXT_POOL == 0)
    return STREAM_CONTEXT_POOL_INIT_ERROR;

  return 0;
}