#include "data_stream_state.h"
#include "data_stream_subscriber_requests.h"
#include "memory_pool.h"

#include "msgport.h"

int32
set_data_stream_channel_status(DSStreamCBPtr stream_, DSRequestMsgPtr request_)
{
  int32            status;
  DSSubscriberPtr  subscriber;
  SubscriberMsgPtr message;
  DataStreamState *stream_state;

  status = find_data_stream_subscriber(stream_, request_->msg.setChannel.streamType, &subscriber);
  if(status != 0)
    {
      return status;
    }

  message = (SubscriberMsgPtr) allocate_memory_pool_entry((MemoryPool *) stream_->subsMsgPool);
  if(message == NULL)
    {
      return kDSNoMsgErr;
    }

  message->whatToDo           = kStreamOpSetChan;
  message->privatePtr         = request_;
  message->msg.channel.number = request_->msg.setChannel.channelNumber;
  message->msg.channel.status = request_->msg.setChannel.channelStatus;
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
