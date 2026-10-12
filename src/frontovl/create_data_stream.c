#include "data_stream_context_pool.h"
#include "streaming_thread.h"
#include "streaming_thread_stack.h"

#include "kernel.h"
#include "task.h"

#define STREAM_SERVICE_STACK_SIZE 4096
#define STREAM_CREATION_NO_MEMORY -992
#define STREAM_CREATION_NO_SIGNAL -991
#define STREAM_CREATION_SIGNAL_FAILURE -990

int32
create_data_stream(DataStreamState **stream_out_,
                   void             *buffer_list_,
                   int32             buffer_size_,
                   int32             priority_adjustment_,
                   int32             subscriber_message_count_)
{
  int32            status;
  DataStreamState *stream;
  uint32           received_signals;

  stream = allocate_memory_pool_entry(g_DATA_STREAM_CONTEXT_POOL);
  if(stream == NULL)
    {
      return STREAM_CREATION_NO_MEMORY;
    }

  stream->buffer_data_size         = buffer_size_;
  stream->free_buffer_head         = buffer_list_;
  stream->subscriber_message_count = subscriber_message_count_;
  stream->creator_task             = CURRENTTASK->t.n_Item;

  stream->creator_signal = AllocSignal(0);
  if(stream->creator_signal == 0)
    {
      status = STREAM_CREATION_NO_SIGNAL;
      goto cleanup;
    }

  /* Pass the new stream context as the service thread's second argument. */
  status = create_streaming_thread((StreamingThreadEntry) run_stream_service_thread,
                                   STREAM_SERVICE_STACK_SIZE,
                                   (int32) CURRENTTASK->t.n_Priority + priority_adjustment_,
                                   NULL,
                                   &stream->thread_stack_memory,
                                   0,
                                   stream);
  if(status > 0)
    {
      stream->thread_item = status;
    }
  else
    {
      goto cleanup;
    }

  received_signals = WaitSignal(stream->creator_signal);
  if(received_signals != stream->creator_signal)
    {
      return STREAM_CREATION_SIGNAL_FAILURE;
    }

  FreeSignal(stream->creator_signal);

  status = stream->creator_status;
  if(status >= 0)
    {
      *stream_out_ = stream;
      return status;
    }

cleanup:
  if(stream->thread_stack_memory != NULL)
    {
      free_streaming_thread_stack(stream->thread_stack_memory);
    }

  return_memory_pool_entry(g_DATA_STREAM_CONTEXT_POOL, stream);
  return status;
}
