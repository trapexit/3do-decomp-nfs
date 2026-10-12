#include "data_stream_buffer_queue.h"
#include "data_stream_state.h"
#include "sdk_veneers.h"

int32
release_data_stream_chunk(DSStreamCBPtr stream_, int32 subscriber_status_, DSDataBufPtr buffer_)
{
  int32            refill_status;
  DataStreamState *stream_state;

  refill_status = 0;
  buffer_->useCount -= 1;
  if(buffer_->useCount == 0)
    {
      stream_      = buffer_->streamCBPtr;
      stream_state = (DataStreamState *) stream_;
      stream_state->completion_replies_pending -= 1;
      if(subscriber_status_ == 0)
        {
          refill_status = request_data_stream_buffer_fill(stream_, buffer_);
        }
      reply_to_data_stream_completion_request(stream_);
    }

  if(buffer_->useCount < 0)
    {
      printf("_ReleaseChunk() - use count negative!!!\n");
      abort_data_stream(stream_);
    }

  if((subscriber_status_ != 0) || (refill_status != 0))
    {
      abort_data_stream(stream_);
    }
  return 0;
}
