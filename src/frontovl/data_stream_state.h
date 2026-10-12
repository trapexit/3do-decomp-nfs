#pragma once

#include "types.h"

struct DSRequestMsg;

#define MAX_STREAM_SUBSCRIBERS 16

typedef struct DataStreamBuffer
{
  int32                    use_count;
  struct DataStreamBuffer *next;
} DataStreamBuffer;

typedef struct DataStreamSubscriber
{
  uint32 stream_type;
  Item   subscriber_port;
  int32  runtime_state;
} DataStreamSubscriber;

typedef struct DataStreamState
{
  Item                 creator_task;
  uint32               creator_signal;
  int32                creator_status;
  Item                 thread_item;
  void                *thread_stack_memory;
  Item                 acquisition_port;
  int32                buffer_data_size;
  DataStreamBuffer    *free_buffer_head;
  int32                subscriber_message_count;
  uint32               flags;
  uint32               clock_offset;
  uint32               last_valid_clock;
  uint32               branch_destination;
  Item                 request_port;
  uint32               request_port_signal;
  Item                 acquisition_reply_port;
  uint32               acquisition_reply_port_signal;
  struct MemoryPool   *acquisition_message_pool;
  Item                 subscriber_reply_port;
  uint32               subscriber_reply_port_signal;
  struct MemoryPool   *subscriber_message_pool;
  DataStreamBuffer    *filled_buffer_head;
  DataStreamBuffer    *filled_buffer_tail;
  int32                total_buffer_count;
  int32                current_free_buffer_count;
  int32                completion_replies_pending;
  struct DSRequestMsg *end_of_stream_request;
  int32                subscriber_replies_pending;
  struct DSRequestMsg *request_head;
  struct DSRequestMsg *request_tail;
  int32                num_subscribers;
  DataStreamSubscriber subscribers[MAX_STREAM_SUBSCRIBERS];
} DataStreamState;

int32
is_data_stream_marker(DataStreamState *stream_, uint32 marker_value_, boolean *is_marker_);
boolean
has_filled_data_stream_buffers(DataStreamState *stream_);
int32
get_filled_data_stream_buffer_count(DataStreamState *stream_);
boolean
is_data_stream_at_end(DataStreamState *stream_);
int32
set_data_stream_absolute_branch(DataStreamState *stream_);
int32
set_data_stream_branch_destination(DataStreamState *stream_, uint32 branch_destination_);
int32
set_data_stream_clock(DataStreamState *stream_, uint32 stream_clock_);
int32
get_data_stream_clock(DataStreamState *stream_, uint32 *stream_clock_);
