#include "control_subscriber.h"
#include "sdk_veneers.h"
#include "stream_message_items.h"

#include "kernel.h"

void
run_control_subscriber_service_thread(int32 argument_count_, ControlSubscriberContext *context_)
{
  CtrlContext     *standard;
  SubscriberMsgPtr message;
  SubscriberMsgPtr subscriber_reply;
  DSRequestMsgPtr  stream_request_reply;
  uint32           awaited_signals;
  uint32           received_signals;
  int32            channel_number;
  int32            keep_running;
  int32            status;

  (void) argument_count_;
  standard = &context_->standard;
  status   = initialize_control_subscriber_thread(context_);
  if(status != 0)
    {
      RUNTIME_EXIT_VENEER(0);
    }

  awaited_signals = (standard->requestPortSignal | standard->cueSignal |
                     standard->subsReplyPortSignal | standard->dsReqReplyPortSignal);
  keep_running    = 1;
  while(keep_running != 0)
    {
      received_signals = WaitSignal(awaited_signals);

      if((received_signals & standard->subsReplyPortSignal) != 0)
        {
          while(poll_for_stream_message(standard->subsReplyPort,
                                        NULL,
                                        NULL,
                                        (void **) &subscriber_reply,
                                        &status))
            {
              return_memory_pool_entry((MemoryPool *) standard->subsMsgPool, subscriber_reply);
            }
        }

      if((received_signals & standard->dsReqReplyPortSignal) != 0)
        {
          while(poll_for_stream_message(standard->dsReqReplyPort,
                                        NULL,
                                        NULL,
                                        (void **) &stream_request_reply,
                                        &status))
            {
              return_memory_pool_entry((MemoryPool *) standard->dsReqMsgPool, stream_request_reply);
            }
        }

      if((received_signals & standard->requestPortSignal) != 0)
        {
          while(
            poll_for_stream_message(standard->requestPort, NULL, NULL, (void **) &message, &status))
            {
              switch(message->whatToDo)
                {
                  case kStreamOpData:
                    status = process_control_data_chunk(message, context_);
                    break;

                  case kStreamOpGetChan:
                    status = get_control_channel_status(message, context_);
                    break;

                  case kStreamOpSetChan:
                    status = set_control_channel_subscription(message, context_);
                    break;

                  case kStreamOpControl:
                    status = handle_control_subscriber_control(message, context_);
                    break;

                  case kStreamOpSync:
                    status = handle_control_subscriber_sync(message, context_);
                    break;

                  case kStreamOpStart:
                    break;

                  case kStreamOpStop:
                    status = stop_control_subscription(message, context_);
                    break;

                  case kStreamOpOpening:
                    status = handle_control_subscriber_opening(message, context_);
                    break;

                  case kStreamOpClosing:
                    status       = handle_control_subscriber_closing(message, context_);
                    keep_running = 0;
                    break;

                  case kStreamOpEOF:
                    status = handle_control_subscriber_end_of_file(message, context_);
                    break;

                  case kStreamOpAbort:
                    status       = abort_control_subscription(message, context_);
                    keep_running = 0;
                    break;

                  default:
                    break;
                }

              ReplyMsg(message->msgItem, status, message, sizeof(SubscriberMsgPtr));
            }
        }
    }

  for(channel_number = 0; channel_number < CTRL_MAX_CHANNELS; channel_number++)
    {
      flush_control_channel(context_, channel_number);
    }
  standard->threadItem = 0;
  delete_memory_pool((MemoryPool *) standard->dsReqMsgPool);
  delete_memory_pool((MemoryPool *) standard->subsMsgPool);
  RUNTIME_EXIT_VENEER(0);
}
