#include "data_stream_state.h"
#include "data_stream_subscriber_requests.h"
#include "memory_pool.h"

#include "msgport.h"

enum
{
  K_INTERNAL_STREAM_CONTROL_REQUEST = -974
};

int32
control_data_stream_subscriber(DSStreamCBPtr stream_, DSRequestMsgPtr request_)
{
  int32            status;
  DSSubscriberPtr  subscriber;
  SubscriberMsgPtr message;
  DataStreamState *stream_state;

  status = find_data_stream_subscriber(stream_, request_->msg.control.streamType, &subscriber);
  if(status == K_INTERNAL_STREAM_CONTROL_REQUEST)
    {
      return handle_internal_data_stream_control(stream_, request_);
    }
  if(status != 0)
    {
      return status;
    }

  message = (SubscriberMsgPtr) allocate_memory_pool_entry((MemoryPool *) stream_->subsMsgPool);
  if(message == NULL)
    {
      return kDSNoMsgErr;
    }

  message->whatToDo                = kStreamOpControl;
  message->privatePtr              = request_;
  message->msg.control.controlArg1 = request_->msg.control.userDefinedOpcode;
  message->msg.control.controlArg2 = request_->msg.control.userDefinedArgPtr;
  status = SendMsg(subscriber->subscriberPort, message->msgItem, message, sizeof(SubscriberMsg));

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
