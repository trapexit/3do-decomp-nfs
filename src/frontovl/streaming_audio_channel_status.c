#include "memory_pool.h"
#include "runtime_memory.h"
#include "streaming_audio_buffer_cleanup.h"
#include "streaming_audio_buffer_queue.h"
#include "streaming_audio_subscriber.h"

#include "audio.h"
#include "kernel.h"
#include "msgport.h"

#define STREAMING_AUDIO_CHANNEL_WRITABLE_MASK (~0x0000fffeU)
#define STREAMING_AUDIO_CHANNEL_ENABLED 0x01
#define STREAMING_AUDIO_CHANNEL_STARTING 0x02
#define STREAMING_AUDIO_REPLY_ERROR_INDEX 1
#define STREAMING_AUDIO_PENDING_REPLY_ERROR_INDEX 1
#define STREAMING_AUDIO_BUFFER_REPLY_ERROR_INDEX 2
#define SAUDIO_ERROR_DELETE_ITEM_FAILED (-6990)

int32
start_streaming_audio_channel(StreamingAudioContext *context_, int32 channel_number_)
{
  StreamingAudioChannel *channel;

  channel = &context_->channels[channel_number_];
  if((channel->status & STREAMING_AUDIO_CHANNEL_ENABLED) != 0 &&
     (channel->status & STREAMING_AUDIO_CHANNEL_STARTING) == 0)
    {
      channel->status |= STREAMING_AUDIO_CHANNEL_STARTING;
      move_waiting_audio_messages(context_, channel_number_);
      return begin_streaming_audio_playback(context_, channel_number_);
    }

  return channel->status;
}

int32
stop_streaming_audio_channel(StreamingAudioContext *context_, int32 channel_number_)
{
  StreamingAudioBuffer  *buffer;
  StreamingAudioChannel *channel;
  SubscriberMsgPtr       message;
  uint32                 current_signals;
  int32                  status;

  channel = &context_->channels[channel_number_];
  status  = 0;
  if(((channel->status & STREAMING_AUDIO_CHANNEL_ENABLED) != 0) &&
     ((channel->status & STREAMING_AUDIO_CHANNEL_STARTING) != 0))
    {
      buffer = get_next_streaming_audio_buffer(channel);
      if(buffer != NULL)
        {
          mute_audio_channel(context_, channel_number_, 0);
          StopAttachment(buffer->attachment_item, NULL);
          channel->attachments_running = 0;
          current_signals              = (GetCurrentSignals() & buffer->signal);
          if(current_signals != 0)
            WaitSignal(buffer->signal);

          message = (SubscriberMsgPtr) buffer->pending_message;
          status  = ReplyMsg(message->msgItem, 0, message, sizeof(SubscriberMsg));
          if(status < 0)
            runtime_debug_printf("StopSAudioChannel() - ReplyMsg() #%ld Failure in %ld\n",
                                 STREAMING_AUDIO_REPLY_ERROR_INDEX,
                                 status);

          buffer->pending_message = NULL;
          reset_streaming_audio_buffer(buffer);
          context_->all_buffer_signals &= ~buffer->signal;
          return_memory_pool_entry(channel->buffer_pool, buffer);
        }

      channel->status &= ~STREAMING_AUDIO_CHANNEL_STARTING;
    }

  return status;
}

int32
flush_streaming_audio_channel(StreamingAudioContext *context_, int32 channel_number_)
{
  StreamingAudioBuffer  *buffer;
  StreamingAudioChannel *channel;
  SubscriberMsgPtr       message;
  int32                  status;

  channel = &context_->channels[channel_number_];
  status  = 0;
  if((channel->status & STREAMING_AUDIO_CHANNEL_ENABLED) != 0)
    {
      status = stop_streaming_audio_channel(context_, channel_number_);
      if(status < 0)
        {
          runtime_debug_printf("FlushSAudioChannel() - StopSAudioChannel() failed: %ld\n", status);
        }

      while((message = get_next_subscriber_message(&channel->pending_messages)) != NULL)
        {
          status = ReplyMsg(message->msgItem, 0, message, sizeof(SubscriberMsg));
          if(status < 0)
            {
              runtime_debug_printf("FlushSAudioChannel() - ReplyMsg() #%ld failed: %ld\n",
                                   STREAMING_AUDIO_PENDING_REPLY_ERROR_INDEX,
                                   status);
            }
        }

      while((buffer = get_next_streaming_audio_buffer(channel)) != NULL)
        {
          message = (SubscriberMsgPtr) buffer->pending_message;
          status  = ReplyMsg(message->msgItem, 0, message, sizeof(SubscriberMsg));
          if(status < 0)
            {
              runtime_debug_printf("FlushSAudioChannel() - ReplyMsg() #%ld failed: %ld\n",
                                   STREAMING_AUDIO_BUFFER_REPLY_ERROR_INDEX,
                                   status);
            }

          buffer->pending_message = NULL;
          reset_streaming_audio_buffer(buffer);
          context_->all_buffer_signals &= ~buffer->signal;
          return_memory_pool_entry(channel->buffer_pool, buffer);
        }
    }

  return status;
}

int32
close_streaming_audio_channel(StreamingAudioContext *context_, int32 channel_number_)
{
  StreamingAudioChannel *channel;
  StreamingAudioChannel *cleanup_channel;
  int32                  status;

  channel = &context_->channels[channel_number_];
  flush_streaming_audio_channel(context_, channel_number_);

  if(channel->channel_instrument > 0)
    {
      status = DeleteItem(channel->channel_instrument);
      if(status < 0)
        {
          return SAUDIO_ERROR_DELETE_ITEM_FAILED;
        }
      channel->channel_instrument = 0;
    }

  if(channel->output.instrument > 0)
    {
      StopInstrument(channel->output.instrument, NULL);
      status = DeleteItem(channel->output.instrument);
      if(status < 0)
        {
          return SAUDIO_ERROR_DELETE_ITEM_FAILED;
        }
      channel->output.instrument = 0;
    }

  if(channel->output.left_envelope > 0)
    {
      StopInstrument(channel->output.left_envelope, NULL);
      status = DeleteItem(channel->output.left_envelope);
      if(status < 0)
        {
          return SAUDIO_ERROR_DELETE_ITEM_FAILED;
        }
      channel->output.left_envelope = 0;
    }

  if(channel->output.right_envelope > 0)
    {
      StopInstrument(channel->output.right_envelope, NULL);
      status = DeleteItem(channel->output.right_envelope);
      if(status < 0)
        {
          return SAUDIO_ERROR_DELETE_ITEM_FAILED;
        }
      channel->output.right_envelope = 0;
    }

  if(channel->buffer_pool != NULL)
    {
      cleanup_channel = channel;
      for_each_free_memory_pool_entry(channel->buffer_pool,
                                      release_streaming_audio_buffer_items,
                                      &cleanup_channel);
      delete_memory_pool(channel->buffer_pool);
    }

  channel->status                   = 0;
  channel->buffer_count             = 0;
  channel->buffer_pool              = NULL;
  channel->channel_instrument       = 0;
  channel->instrument_started       = 0;
  channel->attachments_running      = 0;
  channel->channel_flags[0]         = 0;
  channel->channel_flags[1]         = 0;
  channel->signal_mask              = 0;
  channel->pending_messages.head    = NULL;
  channel->pending_messages.tail    = NULL;
  channel->in_use_count             = 0;
  channel->in_use_head              = NULL;
  channel->in_use_tail              = NULL;
  channel->output.instrument        = 0;
  channel->output.channel_count     = 0;
  channel->output.left_envelope     = 0;
  channel->output.left_target_knob  = 0;
  channel->output.right_envelope    = 0;
  channel->output.right_target_knob = 0;
  channel->output.current_amplitude = -1;
  channel->output.saved_amplitude   = 0;
  channel->output.muted             = 0;
  channel->output.externally_muted  = 0;
  channel->output.flags[0]          = 0;
  channel->output.flags[1]          = 0;
  channel->output.current_pan       = 0;
  return 0;
}

int32
set_streaming_audio_channel_status(StreamingAudioContext *context_, SubscriberMsgPtr message_)
{
  StreamingAudioChannel *channel;
  int32                  channel_number;
  int32                  was_enabled;
  int32                  is_enabled;

  channel_number = message_->msg.channel.number;
  channel        = &context_->channels[channel_number];

  if(channel_number < SA_SUBS_MAX_CHANNELS)
    {
      was_enabled = (channel->status & 1) != 0;
      is_enabled  = (message_->msg.channel.status & 1) != 0;

      if(was_enabled && !is_enabled)
        {
          flush_streaming_audio_channel(context_, channel_number);
          channel->status |= message_->msg.channel.status & STREAMING_AUDIO_CHANNEL_WRITABLE_MASK;
        }
      else if(!was_enabled && is_enabled)
        {
          channel->status |= message_->msg.channel.status & STREAMING_AUDIO_CHANNEL_WRITABLE_MASK;
          start_streaming_audio_channel(context_, channel_number);
        }
      else
        {
          channel->status |= message_->msg.channel.status & STREAMING_AUDIO_CHANNEL_WRITABLE_MASK;
        }
    }
  return 0;
}

int32
get_streaming_audio_channel_status(StreamingAudioContext *context_, SubscriberMsgPtr message_)
{
  int32 channel_number;

  channel_number = message_->msg.channel.number;
  if(channel_number < SA_SUBS_MAX_CHANNELS)
    {
      return context_->channels[channel_number].status;
    }
  return 0;
}
