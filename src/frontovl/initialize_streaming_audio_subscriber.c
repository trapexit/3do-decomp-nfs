#include "streaming_audio_subscriber.h"
#include "memory_pool.h"

int32
initialize_streaming_audio_subscriber(void)
{
  g_S_AUDIO_CONTEXT_POOL = create_memory_pool(STREAMING_AUDIO_CHANNEL_COUNT,
                                              sizeof(StreamingAudioContext));
  if (g_S_AUDIO_CONTEXT_POOL == 0)
    return SAUDIO_ERROR_COULDNT_CREATE_CONTEXT_POOL;

  return 0;
}