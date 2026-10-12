#include "streaming_audio_buffer_cleanup.h"

#include "runtime_memory.h"
#include "streaming_audio_buffer_queue.h"

#include "audio.h"
#include "kernel.h"

#define STREAMING_AUDIO_BUFFER_SAMPLE_CLEANUP_ERROR "UnloadSAudioSample() failed: %ld\n"
#define STREAMING_AUDIO_BUFFER_CUE_CLEANUP_ERROR "DeleteItem Cue failed: %ld\n"

int32
release_streaming_audio_buffer_items(void *argument_, void *pool_entry_)
{
  StreamingAudioBuffer  *buffer;
  StreamingAudioChannel *channel;
  int32                  status;

  channel = *(StreamingAudioChannel **) argument_;
  buffer  = (StreamingAudioBuffer *) pool_entry_;

  status = UnloadSample(buffer->sample_item);
  if(status < 0)
    {
      runtime_debug_printf(STREAMING_AUDIO_BUFFER_SAMPLE_CLEANUP_ERROR, status);
      return 0;
    }

  status = DeleteItem(buffer->cue_item);
  if(status < 0)
    {
      runtime_debug_printf(STREAMING_AUDIO_BUFFER_CUE_CLEANUP_ERROR, status);
      return 0;
    }

  channel->signal_mask &= ~buffer->signal;
  return 1;
}
