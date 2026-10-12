#pragma once

#include "memory_pool.h"
#include "streaming_audio_buffer_queue.h"

#include "datastream.h"
#include "saudiosubscriber.h"

#define SAUDIO_ERROR_COULDNT_CREATE_CONTEXT_POOL (-6989)
#define SAUDIO_ERROR_CHANNEL_OUT_OF_RANGE (-6999)

#define SAUDIO_ERROR_TEMPLATE_NOT_FOUND (-7000)

typedef struct StreamingAudioBufferInitialization
{
  StreamingAudioChannel *channel;
  SAudioHeaderChunk     *header;
} StreamingAudioBufferInitialization;

extern MemoryPool *g_S_AUDIO_CONTEXT_POOL;
extern TemplateRec g_INITIAL_TEMPLATES[];
extern int32       g_STREAMING_AUDIO_MAX_TEMPLATE_COUNT;

int32
initialize_streaming_audio_subscriber(void);
int32
close_streaming_audio_subscriber(void);
int32
create_streaming_audio_subscriber(StreamingAudioContext **context_out_,
                                  DSStreamCBPtr           stream_,
                                  int32                   priority_adjustment_);
int32
dispose_streaming_audio_subscriber(StreamingAudioContext *context_);
int32
process_streaming_audio_data(StreamingAudioContext *context_, SubscriberMsgPtr message_);
int32
set_streaming_audio_channel_status(StreamingAudioContext *context_, SubscriberMsgPtr message_);
int32
get_streaming_audio_channel_status(StreamingAudioContext *context_, SubscriberMsgPtr message_);
int32
dispatch_streaming_audio_control(StreamingAudioContext *context_, SubscriberMsgPtr message_);
int32
close_streaming_audio_channels(StreamingAudioContext *context_, SubscriberMsgPtr message_);
int32
start_streaming_audio_channels(StreamingAudioContext *context_, SubscriberMsgPtr message_);
int32
stop_streaming_audio_channels(StreamingAudioContext *context_, SubscriberMsgPtr message_);
int32
synchronize_streaming_audio_channels(StreamingAudioContext *context_, SubscriberMsgPtr message_);
int32
abort_streaming_audio_channels(StreamingAudioContext *context_, SubscriberMsgPtr message_);
int32
initialize_streaming_audio_thread(StreamingAudioContext *context_);
int32
initialize_streaming_audio_channel(StreamingAudioContext *context_, SAudioHeaderChunk *header_);
int32
initialize_streaming_audio_buffer(void *argument_, void *pool_entry_);
int32
close_streaming_audio_channel(StreamingAudioContext *context_, int32 channel_number_);
int32
begin_streaming_audio_playback(StreamingAudioContext *context_, int32 channel_number_);
int32
flush_streaming_audio_channel(StreamingAudioContext *context_, int32 channel_number_);
int32
start_streaming_audio_channel(StreamingAudioContext *context_, int32 channel_number_);
int32
stop_streaming_audio_channel(StreamingAudioContext *context_, int32 channel_number_);
void
run_streaming_audio_service_thread(int32 argument_count_, StreamingAudioContext *context_);

int32
handle_streaming_audio_opening(void *context_, SubscriberMsgPtr message_);
int32
handle_streaming_audio_end_of_file(void *context_, SubscriberMsgPtr message_);
