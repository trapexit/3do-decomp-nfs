#include "data_stream_buffer_queue.h"
#include "data_stream_reply_handlers.h"
#include "data_stream_request_queue.h"
#include "memory_pool.h"
#include "runtime_error.h"
#include "sdk_veneers.h"

#define STREAM_ACTIVE_STATE_MASK 0x0D
#define STREAM_END_OF_FILE_STATE 0x02
#define STREAM_ACQUISITION_ABORT_STATE 0x300
#define STREAM_ACQUISITION_ERROR_STATE 0x200

void
handle_data_stream_acquisition_reply(DataStreamState *stream_,
                                     DataAcqMsgPtr    acquisition_message_,
                                     Message         *reply_message_)
{
  int32           status;
  DSRequestMsgPtr request;
  DSDataBufPtr    buffer;

  if((acquisition_message_->privatePtr != NULL) &&
     (acquisition_message_->privatePtr == stream_->request_head))
    {
      stream_->subscriber_replies_pending -= 1;
      if(stream_->subscriber_replies_pending == 0)
        {
          request = take_data_stream_request((DSStreamCBPtr) stream_);
          status =
            ReplyMsg(request->msgItem, reply_message_->msg_Result, request, sizeof(DSRequestMsg));
          if(status < 0)
            {
              printf("Failure in %s: $%lx\n",
                     "DataStreamThread - HandleDataAcqReply - ReplyMsg() ",
                     status);
              print_runtime_error(status);
              RUNTIME_EXIT_VENEER(0);
            }
        }
      else
        {
          printf("DataStreamThread - HandleDataAcqReply - "
                 "streamCBPtr->repliesPending wrong = %ld\n",
                 stream_->subscriber_replies_pending);
        }
    }
  else if(acquisition_message_->whatToDo == kAcqOpGetData)
    {
      buffer = acquisition_message_->msg.data.bufferPtr;
      if(reply_message_->msg_Result == 0)
        {
          buffer->next = NULL;
          if(stream_->filled_buffer_head == NULL)
            {
              stream_->filled_buffer_head = (DataStreamBuffer *) buffer;
              stream_->filled_buffer_tail = (DataStreamBuffer *) buffer;
            }
          else
            {
              stream_->filled_buffer_tail->next = (DataStreamBuffer *) buffer;
              stream_->filled_buffer_tail       = (DataStreamBuffer *) buffer;
            }
        }
      else
        {
          return_data_stream_buffer((DSStreamCBPtr) stream_, buffer);
          switch(reply_message_->msg_Result)
            {
              case kDSEndOfFileErr:
                stream_->flags |= STREAM_END_OF_FILE_STATE;
                break;

              case kDSWasFlushedErr:
                break;

              case kDSAbortErr:
                stream_->flags &= ~STREAM_ACTIVE_STATE_MASK;
                stream_->flags |= STREAM_ACQUISITION_ABORT_STATE | STREAM_END_OF_FILE_STATE;
                break;

              default:
                printf("DataStreamThread - HandleDataAcqReply - kAcqOpGetData "
                       "failed with msg_Result = %ld\n",
                       reply_message_->msg_Result);
                print_runtime_error(reply_message_->msg_Result);
                stream_->flags &= ~STREAM_ACTIVE_STATE_MASK;
                stream_->flags |= STREAM_ACQUISITION_ERROR_STATE | STREAM_END_OF_FILE_STATE;
                break;
            }
        }
    }
  else if(acquisition_message_->whatToDo != kAcqOpDisconnect)
    {
      printf("DataStreamThread - HandleDataAcqReply - unexpected dataacq "
             "reply, whatToDo = %ld\n",
             acquisition_message_->whatToDo);
    }

  return_memory_pool_entry(stream_->acquisition_message_pool, acquisition_message_);
}
