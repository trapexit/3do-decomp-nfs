#pragma once

#include "data_stream_state.h"

#include "datastream.h"

typedef void (*StreamingThreadEntry)(int32 argument_count_, void *argument_);

Item
create_streaming_thread(StreamingThreadEntry thread_entry_,
                        int32                stack_size_,
                        int32                priority_,
                        const char          *thread_name_,
                        void               **stack_memory_out_,
                        int32                argument_count_,
                        void                *argument_);
extern char g_THREAD_NAME_HEX_DIGITS[20];

char *
make_thread_name(char       *destination_,
                 int32       destination_size_,
                 const char *prefix_,
                 int32       identifier_);
int32
dispose_streaming_thread(Item thread_item_);
int32
initialize_stream_service_thread(DataStreamState *stream_);
void
release_data_stream_message_pools(DSStreamCBPtr stream_);
void
run_stream_service_thread(int32 argument_count_, DataStreamState *stream_);
