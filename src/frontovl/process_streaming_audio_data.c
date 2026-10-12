#include "types.h"
#include "streaming_audio_subscriber.h"
#include "sdk_veneers.h"
#include "runtime_memory.h"
#include "runtime_error.h"
#define STREAMER_AUDIO_SHDR_TYPE  0x53484452
#define STREAMER_AUDIO_SSMP_TYPE  0x53534D50
#define STREAMER_AUDIO_MESSAGE_SIGNAL_INDEX 2

int32
process_streaming_audio_data(StreamingAudioContext *context, SubscriberMsgPtr message)
{
  int32 status;
  SubscriberMsgPtr subscriber_message;
  SubsChunkData    *chunk;
  uint32            message_type;
  int32             channel;

  subscriber_message = message;
  chunk = (SubsChunkData *)subscriber_message->msg.data.buffer;
  message_type = chunk->subChunkType;
  channel = chunk->channel;

  if (message_type == STREAMER_AUDIO_SSMP_TYPE)
  {
    queue_new_streaming_audio_buffer(context, subscriber_message);
    begin_streaming_audio_playback(context, channel);
    return 0;
  }

  if (message_type == STREAMER_AUDIO_SHDR_TYPE)
  {
    status = initialize_streaming_audio_channel(context, (SAudioHeaderChunk *)chunk);
    if (status < 0)
    {
      runtime_debug_printf("Initializaion for SAudio channel %ld failed; error = %ld\n",
                           channel, status);
      close_streaming_audio_channel(context, channel);
      return status;
    }
    status = ReplyMsg(subscriber_message->msgItem, status, subscriber_message, sizeof(SubscriberMsg));
    if (status < 0)
    {
      runtime_debug_printf("QueueNewAudioBuffer() - ReplyMsg() #1 failed: $%lx\n", status);
      print_runtime_error(status);
      RUNTIME_EXIT_VENEER(0);
    }
    return 0;
  }

  status = ReplyMsg(subscriber_message->msgItem, 0, subscriber_message, sizeof(SubscriberMsg));
  if (status < 0)
  {
    runtime_debug_printf("Failure in %s: $%lx\n", "process_streaming_audio_data", status);
    print_runtime_error(status);
    RUNTIME_EXIT_VENEER(0);
  }
  return 0;
}