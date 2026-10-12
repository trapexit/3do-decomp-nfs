#include "data_stream_context_pool.h"
#include "data_stream_requests.h"
#include "streaming_thread.h"
#include "streaming_thread_stack.h"

#define STREAM_REQUEST_CLOSE 1

int32
dispose_data_stream(Item message_item_, DataStreamState *stream_)
{
  int32        status;
  DSRequestMsg request;

  request.whatToDo = STREAM_REQUEST_CLOSE;
  status = send_data_stream_request(message_item_, FALSE, (DSStreamCBPtr) stream_, &request);

  if(stream_->thread_item > 0)
    {
      dispose_streaming_thread(stream_->thread_item);
    }
  if(stream_->thread_stack_memory != NULL)
    {
      free_streaming_thread_stack(stream_->thread_stack_memory);
    }

  return_memory_pool_entry(g_DATA_STREAM_CONTEXT_POOL, stream_);
  return status;
}
