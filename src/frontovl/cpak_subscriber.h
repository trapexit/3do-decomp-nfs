#pragma once

#include "memory_pool.h"
#include "movie_frame_buffer.h"
#include "subscriber_message_queue.h"

#include "datastream.h"

#define CPAK_CHANNEL_INITIALIZED 0x00010000
#define CPAK_CHANNEL_PADDING_SIZE 3
#define CPAK_CONTEXT_BOOLEAN_PADDING_SIZE 3
#define CPAK_MAX_CHANNELS 8

typedef struct CpakChannelState
{
  uint32                 status;
  SubscriberMessageQueue message_queue;
  Item                   message_queue_semaphore;
  boolean                flush_on_sync;
  uint8                  padding[CPAK_CHANNEL_PADDING_SIZE];
  MovieImage             image;
} CpakChannelState;

typedef struct CpakContextState
{
  Item                   creator_task;
  uint32                 creator_signal;
  int32                  creator_status;
  Item                   thread_item;
  void                  *thread_stack_block;
  Item                   request_port;
  uint32                 request_port_signal;
  Item                   reply_port;
  uint32                 reply_port_signal;
  Item                   cue_item;
  uint32                 cue_signal;
  int32                  stream_clock_origin;
  boolean                timer_running;
  uint8                  timer_padding[CPAK_CONTEXT_BOOLEAN_PADDING_SIZE];
  void                  *codec_handler;
  void                  *film_codec;
  boolean                free_list_not_empty;
  uint8                  free_list_padding[CPAK_CONTEXT_BOOLEAN_PADDING_SIZE];
  SubscriberMessageQueue free_list;
  Item                   free_queue_semaphore;
  Item                   free_queue_signal;
  Item                   cpak_task;
  boolean                time_changed;
  uint8                  time_changed_padding[CPAK_CONTEXT_BOOLEAN_PADDING_SIZE];
  int32                  channel_count;
  CpakChannelState       channels[1];
} CpakContextState;

typedef struct CinepakFrame
{
  uint32 chunk_type;
  uint32 chunk_byte_count;
  uint32 time;
  uint32 channel;
  uint32 chunk_subtype;
  int32  duration;
  int32  frame_byte_count;
  uint8  frame_data[4];
} CinepakFrame;

typedef struct CpakRecordState
{
  void            *stream_control_block;
  CinepakHeader    header;
  CCB              cel;
  CinepakFrame    *current_frame;
  int32            channel;
  SubscriberMsgPtr current_message;
  uint32           last_current_time;
} CpakRecordState;

extern MemoryPool *g_CPAK_RECORD_POOL;
extern MemoryPool *g_CPAK_CONTEXT_POOL;
extern uint32      g_CPAK_FRAME_RESULT;

int32
handle_cpak_control(void *context_, SubscriberMsgPtr message_);
int32
handle_cpak_opening(void *context_, SubscriberMsgPtr message_);
int32
handle_cpak_closing(void *context_, SubscriberMsgPtr message_);
int32
handle_cpak_end_of_file(void *context_, SubscriberMsgPtr message_);
SubscriberMsgPtr
get_next_cpak_chunk_message(CpakContextState *context_, CpakRecordState *record_);
int32
poll_cpak_chunk(CpakContextState *context_,
                CpakRecordState  *record_,
                SubsChunkDataPtr *chunk_,
                SubscriberMsgPtr *message_);
int32
initialize_cpak_record(void             *stream_control_block_,
                       CpakContextState *context_,
                       CpakRecordState **record_,
                       int32             channel_,
                       boolean           flush_on_sync_);
int32
signal_cpak_free_messages(CpakContextState *context_);
int32
return_cpak_chunk_message(CpakContextState *context_, SubscriberMsgPtr message_);
void
reply_to_all_free_cpak_chunks(CpakContextState *context_);
void
flush_cpak_channel(CpakContextState *context_, CpakRecordState *record_, int32 channel_);
int32
destroy_cpak_record(CpakContextState *context_, CpakRecordState *record_, int32 channel_);
int32
get_cpak_current_time(const CpakRecordState *record_);
boolean
is_time_for_next_cpak_frame(CpakRecordState *record_);
boolean
is_cpak_callback_frame_due(const CpakRecordState *record_);
boolean
is_cpak_current_frame_due(CpakRecordState *record_);
uint32
process_cpak_frames(CpakContextState *context_,
                    CpakRecordState  *record_,
                    uint32            options_,
                    int32             reserved_,
                    uint32           *frame_time_,
                    void             *destination_,
                    int32             row_byte_count_);
int32
initialize_cpak_memory_pools(void);
int32
destroy_cpak_memory_pools(void);
int32
create_cpak_subscriber(CpakContextState **context_out_,
                       int32              channel_count_,
                       int32              priority_adjustment_);
int32
dispose_cpak_subscriber(CpakContextState *context_);
void
stop_cpak_channel(CpakContextState *context_, int32 channel_number_);
int32
get_cpak_channel_status(CpakContextState *context_, SubscriberMsgPtr message_);
int32
set_cpak_channel_status(CpakContextState *context_, SubscriberMsgPtr message_);
int32
stop_all_cpak_channels(CpakContextState *context_, SubscriberMsgPtr message_);
int32
queue_cpak_data(CpakContextState *context_, SubscriberMsgPtr message_);
void
flush_queued_cpak_channel(CpakContextState *context_, int32 channel_number_);
int32
synchronize_cpak_channels(CpakContextState *context_, SubscriberMsgPtr message_);
int32
abort_cpak_channels(CpakContextState *context_, SubscriberMsgPtr message_);
int32
initialize_cpak_thread(CpakContextState *context_);
void
cpak_subscriber_thread(int32 unused_, CpakContextState *context_);
