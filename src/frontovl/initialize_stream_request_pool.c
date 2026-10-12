#include "data_acquisition_context_pool.h"

#define FRONTEND_STREAM_REQUEST_BYTES     0x88
#define FRONTEND_STREAM_REQUEST_NO_MEMORY 0xC20 - 0x1000

int32
initialize_stream_request_pool(int32 maximum_context_count_)
{
  g_STREAM_REQUEST_POOL = create_memory_pool(maximum_context_count_,
                                             FRONTEND_STREAM_REQUEST_BYTES);
  if (g_STREAM_REQUEST_POOL == 0)
    return FRONTEND_STREAM_REQUEST_NO_MEMORY;

  return 0;
}