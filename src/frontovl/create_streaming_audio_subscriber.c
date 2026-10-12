#include "streaming_audio_subscriber.h"
#include "streaming_thread.h"
#include "streaming_thread_stack.h"

#include "kernel.h"
#include "task.h"

#define STREAMING_AUDIO_STACK_SIZE 4096
#define STREAMING_AUDIO_THREAD_NAME "SNDS"
#define STREAMING_CREATION_NO_MEMORY -992
#define STREAMING_CREATION_NO_SIGNAL -991
#define STREAMING_CREATION_SIGNAL_ERROR -990

int32
create_streaming_audio_subscriber(StreamingAudioContext **context_out_,
                                  DSStreamCBPtr           stream_,
                                  int32                   priority_adjustment_)
{
  StreamingAudioContext *context;
  uint32                 received_signals;
  int32                  status;

  context = allocate_memory_pool_entry(g_S_AUDIO_CONTEXT_POOL);
  if(context == NULL)
    {
      return STREAMING_CREATION_NO_MEMORY;
    }

  context->creator_task   = CURRENTTASK->t.n_Item;
  context->stream_control = stream_;
  context->creator_signal = AllocSignal(0);
  if(context->creator_signal == 0)
    {
      status = STREAMING_CREATION_NO_SIGNAL;
      goto cleanup;
    }

  status = create_streaming_thread((StreamingThreadEntry) run_streaming_audio_service_thread,
                                   STREAMING_AUDIO_STACK_SIZE,
                                   (int32) CURRENTTASK->t.n_Priority + priority_adjustment_,
                                   STREAMING_AUDIO_THREAD_NAME,
                                   &context->thread_stack_block,
                                   0,
                                   context);
  if(status > 0)
    {
      context->thread_item = status;
    }
  else
    {
      goto cleanup;
    }

  received_signals = WaitSignal(context->creator_signal);
  if(received_signals != context->creator_signal)
    {
      return STREAMING_CREATION_SIGNAL_ERROR;
    }

  FreeSignal(context->creator_signal);
  status = context->creator_status;
  if(status >= 0)
    {
      *context_out_ = context;
      return status;
    }

cleanup:
  if(context->thread_stack_block != NULL)
    {
      free_streaming_thread_stack(context->thread_stack_block);
    }
  return_memory_pool_entry(g_S_AUDIO_CONTEXT_POOL, context);
  return status;
}
