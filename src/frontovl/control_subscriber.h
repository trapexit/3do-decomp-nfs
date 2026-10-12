#pragma once

#include "memory_pool.h"

#include "controlsubscriber.h"
#include "datastream.h"

#define CONTROL_SUBSCRIBER_CONTEXT_COUNT 4
#define CONTROL_SUBSCRIBER_CONTEXT_SIZE 0xE0
#define CONTROL_CUE_ANCHOR_COUNT 8
#define CONTROL_CUE_REQUEST_SIZE 24
#define CONTROL_CUE_CHUNK_VERSION 1

typedef struct ControlCueAnchor
{
  int32 chunk_type;
  int32 chunk_subtype;
  int32 primary_base_time;
  int32 alternate_base_time;
} ControlCueAnchor;

typedef struct ControlSubscriberContext
{
  CtrlContext      standard;
  int32            cue_anchor_count;
  ControlCueAnchor cue_anchors[CONTROL_CUE_ANCHOR_COUNT];
} ControlSubscriberContext;

typedef void (*ControlCueCallback)(void *argument_, int32 relative_stream_time_);

typedef struct ControlCueRequest
{
  int32              chunk_type;
  int32              chunk_subtype;
  int32              use_alternate_base_time;
  int32              trigger_time_offset;
  ControlCueCallback callback;
  void              *callback_argument;
} ControlCueRequest;

typedef struct ControlCueChunk
{
  uint32           chunk_type;
  int32            channel;
  int32            time;
  int32            duration;
  uint32           chunk_subtype;
  int32            version;
  int32            cue_anchor_count;
  ControlCueAnchor cue_anchors[1];
} ControlCueChunk;

typedef char ControlSubscriberContextSizeCheck
  [sizeof(ControlSubscriberContext) == CONTROL_SUBSCRIBER_CONTEXT_SIZE ? 1 : -1];
typedef char
  ControlCueRequestSizeCheck[sizeof(ControlCueRequest) == CONTROL_CUE_REQUEST_SIZE ? 1 : -1];

extern MemoryPool *g_CONTROL_SUBSCRIBER_CONTEXT_POOL;

int32
initialize_control_subscriber(void);
int32
close_control_subscriber(void);
int32
create_control_subscriber(ControlSubscriberContext **context_out_,
                          DSStreamCBPtr              stream_,
                          int32                      priority_adjustment_);
int32
dispose_control_subscriber(ControlSubscriberContext *context_);
void
load_control_cue_anchors(ControlSubscriberContext *context_,
                         int32                     chunk_time_,
                         int32                     cue_anchor_count_,
                         const ControlCueAnchor   *cue_anchors_);
void
dispatch_due_control_cues(ControlSubscriberContext *context_,
                          int32                     unused_time_,
                          ControlCueRequest        *cue_requests_,
                          int32                     cue_request_count_);
void
stop_control_channel(ControlSubscriberContext *context_, int32 channel_number_);
void
flush_control_channel(ControlSubscriberContext *context_, int32 channel_number_);
int32
process_control_data_chunk(SubscriberMsgPtr message_, ControlSubscriberContext *context_);
int32
get_control_channel_status(SubscriberMsgPtr message_, ControlSubscriberContext *context_);
int32
set_control_channel_subscription(SubscriberMsgPtr message_, ControlSubscriberContext *context_);
int32
stop_control_subscription(SubscriberMsgPtr message_, ControlSubscriberContext *context_);
int32
abort_control_subscription(SubscriberMsgPtr message_, ControlSubscriberContext *context_);
int32
initialize_control_subscriber_thread(ControlSubscriberContext *context_);
void
run_control_subscriber_service_thread(int32 argument_count_, ControlSubscriberContext *context_);

int32
handle_control_subscriber_control(SubscriberMsgPtr message_, void *context_);
int32
handle_control_subscriber_sync(SubscriberMsgPtr message_, void *context_);
int32
handle_control_subscriber_opening(SubscriberMsgPtr message_, void *context_);
int32
handle_control_subscriber_closing(SubscriberMsgPtr message_, void *context_);
int32
handle_control_subscriber_end_of_file(SubscriberMsgPtr message_, void *context_);
