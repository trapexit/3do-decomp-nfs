#include "streaming_audio_buffer_queue.h"
#include "streaming_audio_subscriber.h"

int32
close_streaming_audio_channels(StreamingAudioContext *context_, SubscriberMsgPtr message_)
{
  int32 channel_number;

  (void) message_;
  for(channel_number = 0; channel_number < SA_SUBS_MAX_CHANNELS; ++channel_number)
    {
      close_streaming_audio_channel(context_, channel_number);
    }
  return 0;
}

int32
start_streaming_audio_channels(StreamingAudioContext *context_, SubscriberMsgPtr message_)
{
  int32 channel_number;

  for(channel_number = 0; channel_number < SA_SUBS_MAX_CHANNELS; ++channel_number)
    {
      if((message_->msg.start.options & SOPT_FLUSH) != 0)
        {
          flush_streaming_audio_channel(context_, channel_number);
        }
      start_streaming_audio_channel(context_, channel_number);
    }
  return 0;
}

int32
stop_streaming_audio_channels(StreamingAudioContext *context_, SubscriberMsgPtr message_)
{
  int32 channel_number;

  orphan_playing_streaming_audio_buffers(context_);
  for(channel_number = 0; channel_number < SA_SUBS_MAX_CHANNELS; ++channel_number)
    {
      if((message_->msg.stop.options & SOPT_FLUSH) != 0)
        {
          flush_streaming_audio_channel(context_, channel_number);
        }
      else
        {
          stop_streaming_audio_channel(context_, channel_number);
        }
    }
  return 0;
}

int32
synchronize_streaming_audio_channels(StreamingAudioContext *context_, SubscriberMsgPtr message_)
{
  int32 channel_number;

  orphan_playing_streaming_audio_buffers(context_);
  for(channel_number = 0; channel_number < SA_SUBS_MAX_CHANNELS; ++channel_number)
    {
      flush_streaming_audio_channel(context_, channel_number);
    }
  start_streaming_audio_channels(context_, message_);
  return 0;
}

int32
abort_streaming_audio_channels(StreamingAudioContext *context_, SubscriberMsgPtr message_)
{
  int32 channel_number;

  (void) message_;
  for(channel_number = 0; channel_number < SA_SUBS_MAX_CHANNELS; ++channel_number)
    {
      close_streaming_audio_channel(context_, channel_number);
    }
  return 0;
}
