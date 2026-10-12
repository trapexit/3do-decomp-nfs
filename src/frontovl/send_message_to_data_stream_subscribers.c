#include "data_stream_state.h"
#include "data_stream_subscriber_broadcast.h"
#include "memory_pool.h"
#include "runtime_error.h"
#include "sdk_veneers.h"

#include "msgport.h"

int32
send_message_to_data_stream_subscribers(DSStreamCBPtr stream_, SubscriberMsgPtr message_)
{
  int32                 status;
  int32                 subscriber_index;
  DataStreamSubscriber *subscriber;
  SubscriberMsgPtr      message_copy;
  MemoryPool           *message_pool;
  DataStreamState      *stream_state;

  message_pool = (MemoryPool *) stream_->subsMsgPool;
  stream_state = (DataStreamState *) stream_;
  if(message_pool->free_count < stream_state->num_subscribers)
    {
      printf("_ForEachSubscriber - insufficient number of subscriber messages\n");
      return kDSNoMsgErr;
    }

  status                                   = 0;
  stream_state->subscriber_replies_pending = 0;
  for(subscriber_index = 0; subscriber_index < stream_state->num_subscribers; subscriber_index += 1)
    {
      subscriber   = &stream_state->subscribers[subscriber_index];
      message_copy = (SubscriberMsgPtr) allocate_memory_pool_entry(message_pool);
      if(message_copy == NULL)
        {
          printf("_ForEachSubscriber - ran out of subscriber messages\n");
          status = kDSNoMsgErr;
          break;
        }

      message_->msgItem = message_copy->msgItem;
      *message_copy     = *message_;
      status            = SendMsg(subscriber->subscriber_port,
                                  message_copy->msgItem,
                                  message_copy,
                                  sizeof(SubscriberMsg));
      if(status < 0)
        {
          printf("Failure in %s: $%lx\n",
                 "DataStreamThread - _ForEachSubscriber - "
                 "SendMsg() ",
                 status);
          print_runtime_error(status);
          RUNTIME_EXIT_VENEER(0);
          break;
        }

      stream_state->subscriber_replies_pending += 1;
    }

  return status;
}
