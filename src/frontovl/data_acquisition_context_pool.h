#pragma once

#include "memory_pool.h"

extern MemoryPool *g_STREAM_REQUEST_POOL;

int32
initialize_stream_request_pool(int32 maximum_context_count_);
int32
close_stream_request_pool(void);
