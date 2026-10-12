#include "streaming_audio_subscriber.h"
#include "data_stream_state.h"
#include "sdk_veneers.h"

#define CANDIDATE_CHANNEL_ENABLED       0x01
#define CANDIDATE_NO_ATTACHMENT_RUNNING 0

int32
begin_streaming_audio_playback(StreamingAudioContext *context, int32 channelIndex)
{
  StreamingAudioBuffer *buffer;
  StreamingAudioChannel *channel;
  StreamingAudioSampleChunk *sampleChunk;
  SubscriberMsgPtr message;

  channel = &context->channels[channelIndex];
  if (((channel->status & CANDIDATE_CHANNEL_ENABLED) == 0) ||
      (channel->in_use_count == 0) ||
      (channel->attachments_running != CANDIDATE_NO_ATTACHMENT_RUNNING)) {
    return 0;
  }

  buffer = channel->in_use_head;
  if (channelIndex == context->clock_channel) {
    message = (SubscriberMsgPtr) buffer->pending_message;
    sampleChunk = (StreamingAudioSampleChunk *) message->msg.data.buffer;
    set_data_stream_clock((DataStreamState *) context->stream_control,
                          sampleChunk->stream_time);
  }
  StartAttachment(buffer->attachment_item, 0);
  channel->attachments_running = 1;
  return unmute_audio_channel(context, channelIndex, 0);
}