#pragma once

#include "data_stream_state.h"
#include "memory_pool.h"

#include "datastream.h"

extern MemoryPool *g_DATA_STREAM_CONTEXT_POOL;

int32
initialize_data_streaming(int32 maximum_stream_count_);
int32
close_data_streaming(void);
int32
create_data_stream(DataStreamState **stream_out_,
                   void             *buffer_list_,
                   int32             buffer_size_,
                   int32             priority_adjustment_,
                   int32             subscriber_message_count_);
int32
dispose_data_stream(Item message_item_, DataStreamState *stream_);
