#pragma once

#include "memory_pool.h"
#include "subscriber_message_queue.h"

#include "types.h"

#define STREAMING_AUDIO_CHANNEL_SUFFIX_SIZE 0x24
#define STREAMING_AUDIO_CONTEXT_SIZE 0x2D4
#define STREAMING_AUDIO_CHANNEL_COUNT 8
#define STREAMING_AUDIO_SAMPLE_PREFIX_SIZE 0x08
#define STREAMING_AUDIO_SAMPLE_PLACEHOLDER_SIZE 4
#define STREAMING_AUDIO_CHANNEL_ENABLED 0x01
#define STREAMING_AUDIO_CHANNEL_ACTIVE 0x02

typedef struct StreamingAudioBuffer
{
  int32                        sample_item;
  int32                        attachment_item;
  int32                        cue_item;
  uint32                       signal;
  void                        *pending_message;
  struct StreamingAudioBuffer *next;
} StreamingAudioBuffer;

typedef struct StreamingAudioOutput
{
  int32 instrument;
  int32 channel_count;
  int32 left_envelope;
  int32 left_target_knob;
  int32 right_envelope;
  int32 right_target_knob;
  int32 current_amplitude;
  int32 saved_amplitude;
  uint8 muted;
  uint8 externally_muted;
  uint8 flags[2];
  int32 current_pan;
} StreamingAudioOutput;

typedef struct StreamingAudioChannel
{
  uint32                 status;
  int32                  buffer_count;
  MemoryPool            *buffer_pool;
  int32                  channel_instrument;
  uint8                  instrument_started;
  uint8                  attachments_running;
  uint8                  channel_flags[2];
  uint32                 signal_mask;
  SubscriberMessageQueue pending_messages;
  int32                  in_use_count;
  StreamingAudioBuffer  *in_use_head;
  StreamingAudioBuffer  *in_use_tail;
  StreamingAudioOutput   output;
} StreamingAudioChannel;

typedef struct StreamingAudioContext
{
  Item                  creator_task;
  uint32                creator_signal;
  int32                 creator_status;
  Item                  thread_item;
  void                 *thread_stack_block;
  Item                  request_port;
  uint32                request_port_signal;
  void                 *stream_control;
  void                 *template_array;
  uint32                all_buffer_signals;
  int32                 output_template_item;
  int32                 envelope_template_item;
  int32                 clock_channel;
  StreamingAudioChannel channels[STREAMING_AUDIO_CHANNEL_COUNT];
} StreamingAudioContext;

typedef char StreamingAudioContextSizeCheck
  [sizeof(StreamingAudioContext) == STREAMING_AUDIO_CONTEXT_SIZE ? 1 : -1];

typedef struct StreamingAudioSampleChunk
{
  uint8  prefix[STREAMING_AUDIO_SAMPLE_PREFIX_SIZE];
  uint32 stream_time;
  int32  channel;
  uint32 chunk_subtype;
  int32  sample_byte_count;
  uint8  samples[STREAMING_AUDIO_SAMPLE_PLACEHOLDER_SIZE];
} StreamingAudioSampleChunk;

typedef char StreamingAudioSampleChunkSizeCheck[sizeof(StreamingAudioSampleChunk) == 28 ? 1 : -1];

boolean
add_streaming_audio_buffer_to_tail(StreamingAudioChannel *channel_, StreamingAudioBuffer *buffer_);
StreamingAudioBuffer *
get_next_streaming_audio_buffer(StreamingAudioChannel *channel_);
void
orphan_playing_streaming_audio_buffers(StreamingAudioContext *context_);
void
reset_streaming_audio_buffer(StreamingAudioBuffer *buffer_);
void
move_waiting_audio_messages(StreamingAudioContext *context_, int32 channel_index_);
void
queue_new_streaming_audio_buffer(StreamingAudioContext *context_, SubscriberMsgPtr message_);
void
find_completed_audio_buffer(StreamingAudioContext  *context_,
                            uint32                  signal_bits_,
                            StreamingAudioChannel **found_channel_,
                            StreamingAudioBuffer  **found_buffer_);
void
handle_completed_audio_buffers(StreamingAudioContext *context_, uint32 signal_bits_);
int32
set_audio_channel_amplitude(StreamingAudioContext *context_,
                            int32                  channel_index_,
                            int32                  amplitude_);
int32
set_audio_channel_pan(StreamingAudioContext *context_, int32 channel_index_, int32 pan_);
int32
get_audio_channel_amplitude(StreamingAudioContext *context_,
                            int32                  channel_index_,
                            int32                 *amplitude_);
int32
get_audio_channel_pan(StreamingAudioContext *context_, int32 channel_index_, int32 *pan_);
int32
mute_audio_channel(StreamingAudioContext *context_, int32 channel_index_, int32 external_);
int32
unmute_audio_channel(StreamingAudioContext *context_, int32 channel_index_, int32 external_);
