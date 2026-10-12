#include "stream_message_items.h"
#include "streaming_audio_subscriber.h"
#include "streaming_thread.h"
#include "streaming_thread_stack.h"

int32
dispose_streaming_audio_subscriber(StreamingAudioContext *context_)
{
  if(context_ != NULL)
    {
      if(context_->thread_item > 0)
        {
          dispose_streaming_thread(context_->thread_item);
        }
      if(context_->thread_stack_block != NULL)
        {
          free_streaming_thread_stack(context_->thread_stack_block);
        }
      remove_message_port(context_->request_port);
      return_memory_pool_entry(g_S_AUDIO_CONTEXT_POOL, context_);
    }
  return 0;
}
