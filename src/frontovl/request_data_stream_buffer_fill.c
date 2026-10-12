#include "data_stream_buffer_queue.h"
#include "memory_pool.h"

#include "msgport.h"

enum
{
  K_STREAM_RUNNING_STATE             = 0x1,
  K_STREAM_REFILL_BLOCKED_STATE_MASK = 0x602
};

int32
request_data_stream_buffer_fill(DSStreamCBPtr stream_, DSDataBufPtr buffer_)
{
  DataAcqMsgPtr message;
  int32         status;

  if((stream_->acquirePort == 0) || ((stream_->streamFlags & K_STREAM_RUNNING_STATE) == 0) ||
     ((stream_->streamFlags & K_STREAM_REFILL_BLOCKED_STATE_MASK) != 0))
    {
      return_data_stream_buffer(stream_, buffer_);
      return 0;
    }

  message = (DataAcqMsgPtr) allocate_memory_pool_entry((MemoryPool *) stream_->dataMsgPool);
  if(message == NULL)
    {
      return kDSNoMsgErr;
    }

  message->whatToDo            = kAcqOpGetData;
  message->privatePtr          = NULL;
  message->msg.data.bufferPtr  = buffer_;
  message->msg.data.bufferSize = stream_->bufDataSize;
  status = SendMsg(stream_->acquirePort, message->msgItem, message, sizeof(DataAcqMsg));
  if(status < 0)
    {
      return status;
    }
  return 0;
}
