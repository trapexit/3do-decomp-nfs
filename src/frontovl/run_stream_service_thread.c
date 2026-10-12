#include "data_stream_buffer_queue.h"
#include "data_stream_delivery.h"
#include "data_stream_reply_handlers.h"
#include "data_stream_request_dispatcher.h"
#include "data_stream_request_queue.h"
#include "runtime_error.h"
#include "sdk_veneers.h"
#include "stream_message_items.h"
#include "streaming_thread.h"

#include "task.h"

#define STREAM_RUNNING_STATE 0x001
#define STREAM_END_OR_ERROR_STATE_MASK 0x202
#define STREAM_ACQUISITION_HELD_STATE 0x400

void
run_stream_service_thread(int32 argument_count_, DataStreamState *stream_)
{
  int32            status;
  DSRequestMsgPtr  request;
  DataAcqMsgPtr    acquisition_message;
  SubscriberMsgPtr subscriber_message;
  DSDataBufPtr     buffer;
  Item             message_item;
  Message         *reply_message;
  uint32           received_signals;
  uint32           service_signals;
  boolean          keep_running;

  (void) argument_count_;
  status = initialize_stream_service_thread(stream_);
  if(status != 0)
    {
      RUNTIME_EXIT_VENEER(0);
    }

  service_signals = stream_->request_port_signal | stream_->acquisition_reply_port_signal |
                    stream_->subscriber_reply_port_signal;
  keep_running    = TRUE;

  while(TRUE)
    {
      received_signals = WaitSignal(service_signals);

      if((received_signals & stream_->request_port_signal) != 0)
        {
          while(
            poll_for_stream_message(stream_->request_port, NULL, NULL, (void **) &request, &status))
            {
              status = dispatch_data_stream_request(stream_, request, &keep_running);
              if(status != 0)
                {
                  printf("reqMsg->whatToDo = %ld\n", request->whatToDo);
                }
              if(status < 0)
                {
                  printf("Failure in %s: $%lx\n",
                         "DataStreamThread - HandleDSRequest() #1",
                         status);
                  print_runtime_error(status);
                  RUNTIME_EXIT_VENEER(0);
                }
            }
        }

      if((received_signals & stream_->subscriber_reply_port_signal) != 0)
        {
          while(poll_for_stream_message(stream_->subscriber_reply_port,
                                        &message_item,
                                        &reply_message,
                                        (void **) &subscriber_message,
                                        &status))
            {
              handle_data_stream_subscriber_reply(stream_, subscriber_message, reply_message);
              if((!keep_running) && (stream_->subscriber_replies_pending == 0))
                {
                  goto thread_exit;
                }
            }
        }

      if((received_signals & stream_->acquisition_reply_port_signal) != 0)
        {
          while(poll_for_stream_message(stream_->acquisition_reply_port,
                                        &message_item,
                                        &reply_message,
                                        (void **) &acquisition_message,
                                        &status))
            {
              handle_data_stream_acquisition_reply(stream_, acquisition_message, reply_message);
            }
        }

      status = deliver_data_stream(stream_);
      if(status < 0)
        {
          printf("Failure in %s: $%lx\n", "DataStreamThread - DeliverData()", status);
          print_runtime_error(status);
          RUNTIME_EXIT_VENEER(0);
        }

      if((stream_->acquisition_port != 0) && ((stream_->flags & STREAM_RUNNING_STATE) != 0) &&
         ((stream_->flags & STREAM_ACQUISITION_HELD_STATE) == 0) &&
         ((stream_->flags & STREAM_END_OR_ERROR_STATE_MASK) == 0))
        {
          while((buffer = take_free_data_stream_buffer((DSStreamCBPtr) stream_)) != NULL)
            {
              request_data_stream_buffer_fill((DSStreamCBPtr) stream_, buffer);
            }
        }

      if(stream_->subscriber_replies_pending == 0)
        {
          request = take_data_stream_request((DSStreamCBPtr) stream_);
          if(request != NULL)
            {
              status = dispatch_data_stream_request(stream_, request, &keep_running);
              if(status < 0)
                {
                  printf("Failure in %s: $%lx\n",
                         "DataStreamThread - HandleDSRequest() #2",
                         status);
                  print_runtime_error(status);
                  RUNTIME_EXIT_VENEER(0);
                }
            }
        }
    }

thread_exit:
  release_data_stream_message_pools((DSStreamCBPtr) stream_);
  RUNTIME_EXIT_VENEER(0);
}
