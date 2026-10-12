#include "data_acquisition_state.h"
#include "data_stream_buffer_queue.h"
#include "data_stream_delivery.h"
#include "data_stream_state.h"
#include "data_stream_subscriber_requests.h"
#include "kernel_item.h"
#include "memory_pool.h"
#include "sdk_veneers.h"

#include "debug.h"

#define STREAM_RUNNING_STATE 0x01
#define STREAM_DELIVERY_HELD_STATE 0x800
#define STREAM_CHUNK_ALIGNMENT_MASK 0x03
#define STREAM_BUFFER_CHUNK_LIMIT_OFFSET 8
#define STREAM_DELIVERY_ERROR_STATE 0x200
#define STREAM_INVALID_CHUNK_ERROR_TEXT "DS:invalid chunk"

int32
deliver_data_stream(DataStreamState *stream_)
{
  DSDataBufPtr buffer;
  void        *first_chunk;
  int32        status;

  if((stream_->flags & STREAM_RUNNING_STATE) == 0)
    {
      return 0;
    }

  /* Retail's no-work paths return the caller's service signal mask. */
  status = (int32) (stream_->request_port_signal | stream_->acquisition_reply_port_signal |
                    stream_->subscriber_reply_port_signal);

  while((stream_->flags & STREAM_DELIVERY_HELD_STATE) == 0)
    {
      buffer = take_filled_data_stream_buffer((DSStreamCBPtr) stream_);
      if(buffer == NULL)
        {
          break;
        }

      buffer->useCount = 0;
      first_chunk      = (void *) &buffer->streamData;
      status           = deliver_data_stream_buffer_chunks(stream_, buffer, first_chunk);
      if(status != 0)
        {
          return status;
        }

      if(buffer->useCount == 0)
        {
          status = request_data_stream_buffer_fill((DSStreamCBPtr) stream_, buffer);
        }
      else
        {
          stream_->completion_replies_pending += 1;
        }
    }

  return status;
}

int32
deliver_data_stream_buffer_chunks(DataStreamState *stream_,
                                  DSDataBufPtr     buffer_,
                                  void            *first_chunk_)
{
  DataStreamSubscriber *subscriber;
  SubscriberMsgPtr      subscriber_message;
  DSSubscriberPtr       sdk_subscriber;
  StreamChunkPtr        chunk;
  uint8                *chunk_limit;
  int32                 status;

  chunk       = (StreamChunkPtr) first_chunk_;
  chunk_limit = ((uint8 *) buffer_ + stream_->buffer_data_size + STREAM_BUFFER_CHUNK_LIMIT_OFFSET);
  while(((uint8 *) chunk < chunk_limit) && ((stream_->flags & STREAM_DELIVERY_ERROR_STATE) == 0))
    {
      if(((chunk->streamChunkSize & STREAM_CHUNK_ALIGNMENT_MASK) != 0) ||
         ((int32) chunk->streamChunkSize <= 0))
        {
          kprintf("Invalid chunk in data (chunk size = 0x%lx)\n", chunk->streamChunkSize);
          g_DATA_ACQUISITION_ERROR_TEXT = STREAM_INVALID_CHUNK_ERROR_TEXT;
          abort_data_stream((DSStreamCBPtr) stream_);
          return kDSBadChunkSizeErr;
        }

      status = find_data_stream_subscriber((DSStreamCBPtr) stream_,
                                           chunk->streamChunkType,
                                           &sdk_subscriber);
      if(status == 0)
        {
          subscriber         = (DataStreamSubscriber *) sdk_subscriber;
          subscriber_message = allocate_memory_pool_entry(stream_->subscriber_message_pool);
          if(subscriber_message != NULL)
            {
              subscriber_message->whatToDo        = kStreamOpData;
              subscriber_message->privatePtr      = buffer_;
              subscriber_message->msg.data.buffer = chunk;
              status                              = SendMsg(subscriber->subscriber_port,
                                                            subscriber_message->msgItem,
                                                            subscriber_message,
                                                            sizeof(SubscriberMsg));
            }
          else
            {
              status = kDSNoMsgErr;
            }

          if((status < 0) && (subscriber_message != NULL))
            {
              /* Preserve the retail item lookup before aborting the stream. */
              (void) lookup_kernel_item(subscriber_message->msgItem);
            }
          if(status < 0)
            {
              abort_data_stream((DSStreamCBPtr) stream_);
              return status;
            }

          buffer_->useCount += 1;
          subscriber->runtime_state += 1;
        }
      else
        {
          status = 0;
        }

      chunk = (StreamChunkPtr) ((uint8 *) chunk + chunk->streamChunkSize);
    }

  return 0;
}
