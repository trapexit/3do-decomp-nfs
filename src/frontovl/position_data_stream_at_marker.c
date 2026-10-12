#include "data_stream_buffer_queue.h"
#include "data_stream_state.h"
#include "data_stream_subscriber_requests.h"
#include "memory_pool.h"

#include "msgport.h"

enum
{
  K_DATA_STREAM_END_STATE_MASK        = 0x202,
  K_DATA_STREAM_PREROLL_BLOCKED_STATE = 0x400
};

int32
position_data_stream_at_marker(DSStreamCBPtr stream_, DSRequestMsgPtr request_)
{
  int32            status;
  DSDataBufPtr     buffer;
  DataAcqMsgPtr    message;
  DataStreamState *stream_state;

  message = (DataAcqMsgPtr) allocate_memory_pool_entry((MemoryPool *) stream_->dataMsgPool);
  if(message == NULL)
    {
      return kDSNoMsgErr;
    }

  stream_->streamFlags &= ~K_DATA_STREAM_END_STATE_MASK;
  stream_->streamFlags &= ~K_DATA_STREAM_PREROLL_BLOCKED_STATE;
  while((buffer = take_filled_data_stream_buffer(stream_)) != NULL)
    {
      return_data_stream_buffer(stream_, buffer);
    }

  message->whatToDo           = kAcqOpGoMarker;
  message->privatePtr         = request_;
  message->msg.marker.value   = request_->msg.goMarker.markerValue;
  message->msg.marker.options = request_->msg.goMarker.options;
  status = SendMsg(stream_->acquirePort, message->msgItem, message, sizeof(DataAcqMsg));

  stream_state = (DataStreamState *) stream_;
  if(status < 0)
    {
      stream_state->subscriber_replies_pending = 0;
    }
  else
    {
      stream_state->subscriber_replies_pending = 1;
    }
  return status;
}
