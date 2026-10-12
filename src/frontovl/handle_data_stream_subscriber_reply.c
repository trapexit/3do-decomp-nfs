#include "data_stream_buffer_queue.h"
#include "data_stream_reply_handlers.h"
#include "data_stream_request_queue.h"
#include "data_stream_requests.h"
#include "data_stream_subscriber_requests.h"
#include "runtime_error.h"
#include "sdk_veneers.h"

void
handle_data_stream_subscriber_reply(DataStreamState *stream_,
                                    SubscriberMsgPtr subscriber_message_,
                                    Message         *reply_message_)
{
  int32                 status;
  DSRequestMsgPtr       request;
  DSSubscriberPtr       sdk_subscriber;
  DataStreamSubscriber *subscriber;
  DSDataType            stream_type;

  if((subscriber_message_->privatePtr != NULL) &&
     (subscriber_message_->privatePtr == stream_->request_head))
    {
      stream_->subscriber_replies_pending -= 1;
      if(stream_->subscriber_replies_pending == 0)
        {
          request = take_data_stream_request((DSStreamCBPtr) stream_);
          if(subscriber_message_->whatToDo == kStreamOpGetChan)
            {
              *request->msg.getChannel.channelStatusPtr = reply_message_->msg_Result;
            }

          status =
            ReplyMsg(request->msgItem, reply_message_->msg_Result, request, sizeof(DSRequestMsg));
          if(status < 0)
            {
              printf("Failure in %s: $%lx\n",
                     "DataStreamThread - HandleSubscriberReply - ReplyMsg() ",
                     status);
              print_runtime_error(status);
              RUNTIME_EXIT_VENEER(0);
            }
        }
    }
  else if(subscriber_message_->whatToDo == kStreamOpData)
    {
      stream_type = *(DSDataType *) subscriber_message_->msg.data.buffer;
      status = find_data_stream_subscriber((DSStreamCBPtr) stream_, stream_type, &sdk_subscriber);
      if(status == 0)
        {
          subscriber = (DataStreamSubscriber *) sdk_subscriber;
          subscriber->runtime_state -= 1;
        }

      release_data_stream_chunk((DSStreamCBPtr) stream_,
                                reply_message_->msg_Result,
                                (DSDataBufPtr) subscriber_message_->privatePtr);
    }
  else if((subscriber_message_->whatToDo != kStreamOpAbort) &&
          (subscriber_message_->whatToDo != kStreamOpEOF))
    {
      printf("DataStreamThread - unexpected subscriber reply, whatToDo = %ld\n",
             subscriber_message_->whatToDo);
    }

  return_memory_pool_entry(stream_->subscriber_message_pool, subscriber_message_);
}
