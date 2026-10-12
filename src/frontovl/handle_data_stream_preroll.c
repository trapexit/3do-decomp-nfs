#include "data_stream_buffer_queue.h"
#include "data_stream_subscriber_requests.h"
#include "memory_pool.h"
#include "runtime_error.h"
#include "sdk_veneers.h"

#include "msgport.h"

enum
{
  K_DATA_STREAM_PREROLL_BLOCKED_STATE = 0x400
};

int32
handle_data_stream_preroll(DSStreamCBPtr stream_, DSRequestMsgPtr request_)
{
  int32         status;
  DSDataBufPtr  buffer;
  DataAcqMsgPtr message;

  (void) request_;
  status = 0;
  stream_->streamFlags &= ~K_DATA_STREAM_PREROLL_BLOCKED_STATE;
  while((buffer = take_free_data_stream_buffer(stream_)) != NULL)
    {
      message = (DataAcqMsgPtr) allocate_memory_pool_entry((MemoryPool *) stream_->dataMsgPool);
      if(message == NULL)
        {
          status = kDSNoMsgErr;
          break;
        }

      message->whatToDo            = kAcqOpGetData;
      message->privatePtr          = NULL;
      message->msg.data.bufferPtr  = buffer;
      message->msg.data.bufferSize = stream_->bufDataSize;
      status = SendMsg(stream_->acquirePort, message->msgItem, message, sizeof(DataAcqMsg));
      if(status < 0)
        {
          printf("Failure in %s: $%lx\n", "DoPreRollStream()", status);
          print_runtime_error(status);
          RUNTIME_EXIT_VENEER(0);
          break;
        }
    }
  return status;
}
