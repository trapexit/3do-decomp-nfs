#include "data_stream_subscriber_broadcast.h"
#include "data_stream_subscriber_requests.h"
#include "memory_pool.h"

#include "msgport.h"

int32
close_data_stream(DSStreamCBPtr stream_, DSRequestMsgPtr request_)
{
  int32         status;
  DataAcqMsgPtr acquisition_message;
  SubscriberMsg subscriber_message;

  status = 0;
  if(stream_->acquirePort != 0)
    {
      acquisition_message =
        (DataAcqMsgPtr) allocate_memory_pool_entry((MemoryPool *) stream_->dataMsgPool);
      if(acquisition_message == NULL)
        {
          status = kDSNoMsgErr;
        }
      else
        {
          acquisition_message->whatToDo   = kAcqOpDisconnect;
          acquisition_message->privatePtr = NULL;
          status                          = SendMsg(stream_->acquirePort,
                                                    acquisition_message->msgItem,
                                                    acquisition_message,
                                                    sizeof(DataAcqMsg));
        }
    }

  subscriber_message.whatToDo   = kStreamOpClosing;
  subscriber_message.privatePtr = request_;
  send_message_to_data_stream_subscribers(stream_, &subscriber_message);
  return status;
}
