#include "data_stream_state.h"
#include "data_stream_subscriber_requests.h"
#include "memory_pool.h"

#include "msgport.h"

int32
register_data_stream_subscriber(DSStreamCBPtr stream_, DSRequestMsgPtr request_)
{
  int32                 status;
  DSSubscriberPtr       existing_subscriber;
  DataStreamSubscriber *subscriber;
  SubscriberMsgPtr      message;
  DataStreamState      *stream_state;

  status =
    find_data_stream_subscriber(stream_, request_->msg.subscribe.dataType, &existing_subscriber);
  if(status != STREAM_SUBSCRIBER_RETAIL_MISS_ERROR)
    {
      if(request_->msg.subscribe.subscriberPort == 0)
        {
          remove_data_stream_subscriber(stream_, existing_subscriber);
          return 0;
        }
      return kDSSubDuplicateErr;
    }

  stream_state = (DataStreamState *) stream_;
  if(stream_state->num_subscribers >= MAX_STREAM_SUBSCRIBERS)
    {
      return kDSSubMaxErr;
    }

  subscriber = &stream_state->subscribers[stream_state->num_subscribers];
  stream_state->num_subscribers += 1;
  subscriber->stream_type     = request_->msg.subscribe.dataType;
  subscriber->subscriber_port = request_->msg.subscribe.subscriberPort;
  subscriber->runtime_state   = 0;

  message = (SubscriberMsgPtr) allocate_memory_pool_entry((MemoryPool *) stream_->subsMsgPool);
  if(message == NULL)
    {
      return kDSNoMsgErr;
    }

  message->whatToDo   = kStreamOpOpening;
  message->privatePtr = request_;
  status = SendMsg(subscriber->subscriber_port, message->msgItem, message, sizeof(SubscriberMsg));
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
