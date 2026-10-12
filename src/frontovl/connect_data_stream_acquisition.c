#include "data_stream_buffer_queue.h"
#include "data_stream_state.h"
#include "data_stream_subscriber_requests.h"
#include "memory_pool.h"

#include "msgport.h"

int32
connect_data_stream_acquisition(DSStreamCBPtr stream_, DSRequestMsgPtr request_)
{
  int32            status;
  Item             new_acquisition_port;
  DSDataBufPtr     buffer;
  DataAcqMsgPtr    message;
  DataStreamState *stream_state;

  new_acquisition_port = request_->msg.connect.acquirePort;
  if(stream_->acquirePort == new_acquisition_port)
    {
      return 0;
    }

  if(stream_->acquirePort != 0)
    {
      message = (DataAcqMsgPtr) allocate_memory_pool_entry((MemoryPool *) stream_->dataMsgPool);
      if(message == NULL)
        {
          return kDSNoMsgErr;
        }
      message->whatToDo   = kAcqOpDisconnect;
      message->privatePtr = NULL;
      status = SendMsg(stream_->acquirePort, message->msgItem, message, sizeof(DataAcqMsg));
      if(status < 0)
        {
          return status;
        }
    }

  while((buffer = take_filled_data_stream_buffer(stream_)) != NULL)
    {
      return_data_stream_buffer(stream_, buffer);
    }
  stream_->acquirePort = new_acquisition_port;
  if(stream_->acquirePort == 0)
    {
      return 0;
    }

  message = (DataAcqMsgPtr) allocate_memory_pool_entry((MemoryPool *) stream_->dataMsgPool);
  if(message == NULL)
    {
      return kDSNoMsgErr;
    }
  message->whatToDo                = kAcqOpConnect;
  message->privatePtr              = request_;
  message->msg.connect.streamCBPtr = stream_;
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
