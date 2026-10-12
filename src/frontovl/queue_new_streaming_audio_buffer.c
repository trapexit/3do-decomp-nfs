#include "streaming_audio_buffer_queue.h"
#include "sdk_veneers.h"
#include "runtime_memory.h"
#include "runtime_error.h"

void
queue_new_streaming_audio_buffer(StreamingAudioContext *context, SubscriberMsgPtr message)
{
  StreamingAudioSampleChunk *chunk;
  int32 channelIndex;
  StreamingAudioChannel *channel;
  int32 status;

  chunk = (StreamingAudioSampleChunk *)message->msg.data.buffer;
  channelIndex = chunk->channel;
  channel = &context->channels[channelIndex];

  if ((channel->status & STREAMING_AUDIO_CHANNEL_ENABLED) != 0) {
    add_subscriber_message_to_tail(&channel->pending_messages, message);
    move_waiting_audio_messages(context, channelIndex);
  } else {
    status = ReplyMsg(message->msgItem, 0, message, sizeof(SubscriberMsg));
    if (status < 0) {
      runtime_debug_printf("QueueNewAudioBuffer() - ReplyMsg() failed: %ld\n", status);
      print_runtime_error(status);
      RUNTIME_EXIT_VENEER(0);
    }
  }
}