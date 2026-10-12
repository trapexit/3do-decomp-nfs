#include "memory_pool.h"
#include "sdk_veneers.h"
#include "stream_message_items.h"
#include "streaming_thread.h"

#include "task.h"

#define STREAM_BUFFER_ALIGNMENT_MASK 0x03
#define STREAM_ADDITIONAL_ACQUISITION_MESSAGES 6
#define STREAM_MESSAGE_SIZE 24
#define STREAM_INITIALIZATION_ERROR -981
#define STREAM_BAD_BUFFER_ALIGNMENT_ERROR -983
#define STREAM_NO_PORT_ERROR -995
#define STREAM_NO_MEMORY_ERROR -992
#define STREAM_NO_MESSAGE_ERROR -994

int32
initialize_stream_service_thread(DataStreamState *stream_)
{
  int32        status;
  DSDataBufPtr buffer;
  int32        buffer_count;

  stream_->creator_status             = STREAM_INITIALIZATION_ERROR;
  stream_->flags                      = 0;
  stream_->clock_offset               = 0;
  stream_->num_subscribers            = 0;
  stream_->filled_buffer_head         = NULL;
  stream_->filled_buffer_tail         = NULL;
  stream_->acquisition_port           = 0;
  stream_->subscriber_replies_pending = 0;
  stream_->request_head               = NULL;
  stream_->request_tail               = NULL;

  buffer_count = 0;
  buffer       = (DSDataBufPtr) stream_->free_buffer_head;
  while(buffer != NULL)
    {
      if(((uint32) buffer & STREAM_BUFFER_ALIGNMENT_MASK) != 0)
        {
          return STREAM_BAD_BUFFER_ALIGNMENT_ERROR;
        }
      buffer->streamCBPtr = (DSStreamCBPtr) stream_;
      buffer_count += 1;
      buffer = buffer->next;
    }

  stream_->total_buffer_count         = buffer_count;
  stream_->current_free_buffer_count  = buffer_count;
  stream_->completion_replies_pending = 0;
  stream_->end_of_stream_request      = NULL;

  status = OpenAudioFolio();
  if(status < 0)
    {
      stream_->creator_status = status;
      goto initialization_finished;
    }

  stream_->creator_status = STREAM_NO_PORT_ERROR;
  stream_->request_port   = create_message_port(&stream_->request_port_signal);
  if(stream_->request_port <= 0)
    {
      goto initialization_finished;
    }
  stream_->acquisition_reply_port = create_message_port(&stream_->acquisition_reply_port_signal);
  if(stream_->acquisition_reply_port <= 0)
    {
      goto initialization_finished;
    }
  stream_->subscriber_reply_port = create_message_port(&stream_->subscriber_reply_port_signal);
  if(stream_->subscriber_reply_port <= 0)
    {
      goto initialization_finished;
    }

  stream_->creator_status = STREAM_NO_MEMORY_ERROR;
  stream_->subscriber_message_pool =
    create_memory_pool(stream_->subscriber_message_count, STREAM_MESSAGE_SIZE);
  if(stream_->subscriber_message_pool == NULL)
    {
      goto initialization_finished;
    }
  stream_->acquisition_message_pool =
    create_memory_pool(buffer_count + STREAM_ADDITIONAL_ACQUISITION_MESSAGES, STREAM_MESSAGE_SIZE);
  if(stream_->acquisition_message_pool == NULL)
    {
      goto initialization_finished;
    }

  stream_->creator_status = STREAM_NO_MESSAGE_ERROR;
  if(!fill_pool_with_stream_message_items(stream_->subscriber_message_pool,
                                          stream_->subscriber_reply_port))
    {
      goto initialization_finished;
    }
  if(!fill_pool_with_stream_message_items(stream_->acquisition_message_pool,
                                          stream_->acquisition_reply_port))
    {
      goto initialization_finished;
    }

  stream_->creator_status = 0;

initialization_finished:
  status = SendSignal(stream_->creator_task, stream_->creator_signal);
  if(status >= 0)
    {
      status = stream_->creator_status;
    }

  if(status < 0)
    {
      release_data_stream_message_pools((DSStreamCBPtr) stream_);
      return status;
    }
  return 0;
}
