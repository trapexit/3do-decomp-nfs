#include "streaming_audio_buffer_queue.h"

boolean
add_streaming_audio_buffer_to_tail(StreamingAudioChannel *channel, StreamingAudioBuffer *buffer)
{
  buffer->next = 0;
  if (channel->in_use_head != 0) {
    channel->in_use_tail->next = buffer;
    channel->in_use_tail = buffer;
    channel->in_use_count += 1;
    return 0;
  } else {
    channel->in_use_head = buffer;
    channel->in_use_tail = buffer;
    channel->in_use_count += 1;
    return 1;
  }
}