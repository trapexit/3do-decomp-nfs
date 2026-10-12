#include "streaming_audio_buffer_queue.h"

#include "data_stream_state.h"
#include "runtime_memory.h"

#include "audio.h"
#include "msgport.h"

#define STREAMING_AUDIO_CHANNEL_ENABLED 0x01
#define STREAMING_AUDIO_CHANNEL_ACTIVE 0x02
#define STREAMING_AUDIO_CHANNEL_NO_ATTACHMENT_RUNNING 0
#define STREAMING_AUDIO_BUFFER_TAG_COUNT 3
#define STREAMING_AUDIO_END_TAG_INDEX 2
#define STREAMING_AUDIO_RELEASED_SAMPLE_MARKER 0xDEADBEEF
#define STREAMING_AUDIO_MAX_AMPLITUDE 0x7FFF
#define STREAMING_AUDIO_CENTER_PAN 0x4000
#define STREAMING_AUDIO_PAN_RANGE 0x8000
#define STREAMING_AUDIO_PAN_SHIFT 14
#define STREAMING_AUDIO_STEREO_CHANNEL_COUNT 2
#define STREAMING_AUDIO_VOLUME_RANGE_ERROR (-6998)
#define STREAMING_AUDIO_PAN_RANGE_ERROR (-6997)
#define STREAMING_AUDIO_CHANNEL_COUNT_ERROR (-6991)

StreamingAudioBuffer *
get_next_streaming_audio_buffer(StreamingAudioChannel *channel_)
{
  StreamingAudioBuffer *buffer;

  buffer = channel_->in_use_head;
  if(buffer != NULL)
    {
      channel_->in_use_head = buffer->next;
      if(channel_->in_use_tail == buffer)
        {
          channel_->in_use_tail = NULL;
        }
      channel_->in_use_count -= 1;
    }

  return buffer;
}

void
orphan_playing_streaming_audio_buffers(StreamingAudioContext *context_)
{
  int32                  channel_index;
  StreamingAudioChannel *channel;
  StreamingAudioBuffer  *buffer;

  for(channel_index = 0; channel_index < STREAMING_AUDIO_CHANNEL_COUNT; channel_index += 1)
    {
      channel = &context_->channels[channel_index];
      if((channel->status & STREAMING_AUDIO_CHANNEL_ENABLED) != 0 &&
         (channel->status & STREAMING_AUDIO_CHANNEL_ACTIVE) != 0)
        {
          buffer = channel->in_use_head;
          if(buffer != NULL)
            {
              LinkAttachments(buffer->attachment_item, 0);
            }
        }
    }
}

void
reset_streaming_audio_buffer(StreamingAudioBuffer *buffer_)
{
  TagArg tags[STREAMING_AUDIO_BUFFER_TAG_COUNT];

  tags[0].ta_Tag                             = AF_TAG_ADDRESS;
  tags[0].ta_Arg                             = NULL;
  tags[1].ta_Tag                             = AF_TAG_NUMBYTES;
  tags[1].ta_Arg                             = NULL;
  tags[STREAMING_AUDIO_END_TAG_INDEX].ta_Tag = TAG_END;

  SetAudioItemInfo(buffer_->sample_item, tags);
  LinkAttachments(buffer_->attachment_item, 0);
}

void
move_waiting_audio_messages(StreamingAudioContext *context_, int32 channel_index_)
{
  StreamingAudioSampleChunk *sample_chunk;
  StreamingAudioChannel     *channel;
  StreamingAudioBuffer      *buffer;
  SubscriberMsgPtr           message;
  TagArg                     tags[STREAMING_AUDIO_BUFFER_TAG_COUNT];

  channel = &context_->channels[channel_index_];
  if((channel->status & STREAMING_AUDIO_CHANNEL_ACTIVE) == 0)
    return;

  if((channel->output.instrument <= 0) && (channel->pending_messages.head != NULL))
    {
      runtime_debug_printf("\n\n SERIOUS ERROR on channel %ld! -- Data buffers arrived "
                           "on an uninitalized channel!\n\n",
                           channel_index_);
      return;
    }

  while(channel->pending_messages.head != NULL)
    {
      buffer = allocate_memory_pool_entry(channel->buffer_pool);
      if(buffer == NULL)
        break;

      message      = get_next_subscriber_message(&channel->pending_messages);
      sample_chunk = (StreamingAudioSampleChunk *) message->msg.data.buffer;

      tags[0].ta_Tag                             = AF_TAG_ADDRESS;
      tags[0].ta_Arg                             = &sample_chunk->samples[0];
      tags[1].ta_Tag                             = AF_TAG_NUMBYTES;
      tags[1].ta_Arg                             = (void *) sample_chunk->sample_byte_count;
      tags[STREAMING_AUDIO_END_TAG_INDEX].ta_Tag = TAG_END;
      SetAudioItemInfo(buffer->sample_item, tags);

      buffer->pending_message = message;
      context_->all_buffer_signals |= buffer->signal;
      if(channel->in_use_count != 0)
        {
          LinkAttachments(channel->in_use_tail->attachment_item, buffer->attachment_item);
        }
      add_streaming_audio_buffer_to_tail(channel, buffer);
    }
}

void
find_completed_audio_buffer(StreamingAudioContext  *context_,
                            uint32                  signal_bits_,
                            StreamingAudioChannel **found_channel_,
                            StreamingAudioBuffer  **found_buffer_)
{
  int32                  channel_index;
  StreamingAudioChannel *channel;
  StreamingAudioBuffer  *buffer;
  uint32                 channel_signal_bits;

  for(channel_index = 0; channel_index < STREAMING_AUDIO_CHANNEL_COUNT; channel_index += 1)
    {
      channel             = &context_->channels[channel_index];
      buffer              = channel->in_use_head;
      channel_signal_bits = signal_bits_ & channel->signal_mask;
      if(channel_signal_bits != 0)
        {
          if((channel_signal_bits & buffer->signal) != 0)
            {
              buffer = get_next_streaming_audio_buffer(channel);
            }
          else
            {
              buffer = NULL;
              runtime_debug_printf("\n\n SERIOUS ERROR on channel %ld! -- A buffer completion "
                                   "signal was received out of order!\n\n",
                                   channel_index);
            }
          break;
        }
    }

  *found_channel_ = channel;
  *found_buffer_  = buffer;
}

void
handle_completed_audio_buffers(StreamingAudioContext *context_, uint32 signal_bits_)
{
  SubscriberMsgPtr           message;
  StreamingAudioSampleChunk *sample_chunk;
  StreamingAudioChannel     *channel;
  StreamingAudioBuffer      *buffer;
  int32                      channel_index;
  int32                      clock_channel_completed;
  int32                      status;

  clock_channel_completed = 0;
  signal_bits_ &= context_->all_buffer_signals;

  while(signal_bits_ != 0)
    {
      find_completed_audio_buffer(context_, signal_bits_, &channel, &buffer);
      if(buffer == NULL)
        {
          return;
        }

      signal_bits_ &= ~buffer->signal;
      context_->all_buffer_signals &= ~buffer->signal;

      message       = (SubscriberMsgPtr) buffer->pending_message;
      sample_chunk  = (StreamingAudioSampleChunk *) message->msg.data.buffer;
      channel_index = sample_chunk->channel;
      if(channel_index == context_->clock_channel)
        {
          clock_channel_completed = 1;
        }

      *(uint32 *) &sample_chunk->samples[0] = STREAMING_AUDIO_RELEASED_SAMPLE_MARKER;
      status = ReplyMsg(message->msgItem, 0, message, sizeof(SubscriberMsg));
      if(status < 0)
        {
          runtime_debug_printf("HandleCompletedBuffers() - ReplyMsg() #1 failed: %ld\n", status);
        }

      buffer->pending_message = NULL;
      reset_streaming_audio_buffer(buffer);
      return_memory_pool_entry(channel->buffer_pool, buffer);
      move_waiting_audio_messages(context_, channel_index);

      if(channel->in_use_count == 0 && channel->pending_messages.head == NULL)
        {
          channel->attachments_running = 0;
          mute_audio_channel(context_, channel_index, 0);
        }
    }

  if(clock_channel_completed != 0)
    {
      channel = &context_->channels[context_->clock_channel];
      buffer  = channel->in_use_head;
      if(buffer != NULL)
        {
          message      = (SubscriberMsgPtr) buffer->pending_message;
          sample_chunk = (StreamingAudioSampleChunk *) message->msg.data.buffer;
          set_data_stream_clock((DataStreamState *) context_->stream_control,
                                sample_chunk->stream_time);
        }
    }
}

int32
set_audio_channel_amplitude(StreamingAudioContext *context_, int32 channel_index_, int32 amplitude_)
{
  StreamingAudioChannel *channel;
  int32                  left_amplitude;
  int32                  right_amplitude;

  channel = &context_->channels[channel_index_];
  if((amplitude_ < 0) || (amplitude_ > STREAMING_AUDIO_MAX_AMPLITUDE))
    return STREAMING_AUDIO_VOLUME_RANGE_ERROR;

  if((channel->output.instrument > 0) && (channel->output.muted == 0))
    {
      if(channel->output.channel_count == STREAMING_AUDIO_STEREO_CHANNEL_COUNT)
        {
          TweakRawKnob(channel->output.left_target_knob, amplitude_);
        }
      else
        {
          if(channel->output.current_pan < STREAMING_AUDIO_CENTER_PAN)
            {
              left_amplitude = amplitude_;
              right_amplitude =
                ((channel->output.current_pan * amplitude_) >> STREAMING_AUDIO_PAN_SHIFT);
            }
          else
            {
              right_amplitude = amplitude_;
              left_amplitude =
                (((STREAMING_AUDIO_PAN_RANGE - channel->output.current_pan) * amplitude_) >>
                 STREAMING_AUDIO_PAN_SHIFT);
            }

          TweakRawKnob(channel->output.left_target_knob, left_amplitude);
          TweakRawKnob(channel->output.right_target_knob, right_amplitude);
        }
    }

  channel->output.current_amplitude = amplitude_;
  if(channel->output.muted != 0)
    channel->output.saved_amplitude = amplitude_;

  return 0;
}

int32
set_audio_channel_pan(StreamingAudioContext *context_, int32 channel_index_, int32 pan_)
{
  StreamingAudioChannel *channel;
  int32                  left_amplitude;
  int32                  right_amplitude;

  channel = &context_->channels[channel_index_];
  if(pan_ < 0 || pan_ > STREAMING_AUDIO_MAX_AMPLITUDE)
    {
      return STREAMING_AUDIO_PAN_RANGE_ERROR;
    }

  if(channel->output.instrument > 0 && channel->output.muted == 0)
    {
      if(channel->output.channel_count != 1)
        {
          return STREAMING_AUDIO_CHANNEL_COUNT_ERROR;
        }
      if(pan_ < STREAMING_AUDIO_CENTER_PAN)
        {
          left_amplitude  = channel->output.current_amplitude;
          right_amplitude = (pan_ * channel->output.current_amplitude) >> STREAMING_AUDIO_PAN_SHIFT;
        }
      else
        {
          right_amplitude = channel->output.current_amplitude;
          left_amplitude =
            ((STREAMING_AUDIO_PAN_RANGE - pan_) * channel->output.current_amplitude) >>
            STREAMING_AUDIO_PAN_SHIFT;
        }
      TweakRawKnob(channel->output.left_target_knob, left_amplitude);
      TweakRawKnob(channel->output.right_target_knob, right_amplitude);
    }

  channel->output.current_pan = pan_;
  return 0;
}

int32
get_audio_channel_amplitude(StreamingAudioContext *context_,
                            int32                  channel_index_,
                            int32                 *amplitude_)
{
  StreamingAudioChannel *channel;

  channel = &context_->channels[channel_index_];
  if(channel->output.muted != 0)
    {
      *amplitude_ = channel->output.saved_amplitude;
    }
  else
    {
      *amplitude_ = channel->output.current_amplitude;
    }
  return 0;
}

int32
get_audio_channel_pan(StreamingAudioContext *context_, int32 channel_index_, int32 *pan_)
{
  *pan_ = context_->channels[channel_index_].output.current_pan;
  return 0;
}

int32
mute_audio_channel(StreamingAudioContext *context_, int32 channel_index_, int32 external_)
{
  StreamingAudioChannel *channel;
  int32                  status;

  channel = &context_->channels[channel_index_];
  status  = 0;
  if(external_ != 0)
    {
      channel->output.externally_muted = (uint8) external_;
    }
  if(channel->output.muted == 0)
    {
      get_audio_channel_amplitude(context_, channel_index_, &channel->output.saved_amplitude);
      status                = set_audio_channel_amplitude(context_, channel_index_, 0);
      channel->output.muted = 1;
    }
  return status;
}

int32
unmute_audio_channel(StreamingAudioContext *context_, int32 channel_index_, int32 external_)
{
  StreamingAudioChannel *channel;
  uint8                  is_external;
  int32                  status;

  channel     = &context_->channels[channel_index_];
  is_external = (uint8) external_;
  status      = 0;
  if(channel->output.muted == 0)
    return status;

  if(is_external != 0)
    {
      if(channel->output.externally_muted == 0)
        return status;
    }
  else if(channel->output.externally_muted != 0)
    {
      return status;
    }

  channel->output.muted = 0;
  status = set_audio_channel_amplitude(context_, channel_index_, channel->output.saved_amplitude);
  channel->output.externally_muted = 0;
  return status;
}
