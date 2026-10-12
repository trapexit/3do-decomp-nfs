#include "cpak_subscriber.h"

#include "cinepak_codebook.h"
#include "cinepak_handler.h"
#include "cpak.h"
#include "data_stream_state.h"
#include "runtime_memory_ops.h"
#include "runtime_semaphore.h"
#include "runtime_string.h"
#include "sdk_veneers.h"
#include "stream_message_items.h"
#include "streaming_thread.h"
#include "streaming_thread_stack.h"
#include "tracked_memory.h"

#include "audio.h"
#include "codec.h"
#include "kernel.h"
#include "task.h"

MemoryPool *g_CPAK_CONTEXT_POOL = NULL;
MemoryPool *g_CPAK_RECORD_POOL  = NULL;
uint32      g_CPAK_FRAME_RESULT = 0;

#define CPAK_CHUNK_DIFFERENCE_FRAME 0x4446524D
#define CPAK_CHUNK_FRAME_HEADER 0x46484452
#define CPAK_CHUNK_FRAME 0x46524D45
#define CPAK_OPTION_KEY_FRAME 0x00000004
#define CPAK_OPTION_STOP_AFTER_FRAME 0x00000010
#define CPAK_OPTION_PROCESSING_MODE_MASK 0x00000030
#define CPAK_OPTION_STOP_AFTER_HEADER 0x00000100
#define CPAK_OPTION_COMPARE_HEADER 0x00000200
#define CPAK_OPTION_VIDEO_WINDOW 0x00000400
#define CPAK_RESULT_HEADER 0x00000100
#define CPAK_RESULT_HEADER_CHANGED 0x00000200
#define CPAK_FRAME_KEY 1
#define CPAK_FRAME_DIFFERENCE 2
#define CPAK_CONTEXT_POOL_ENTRY_COUNT 2
#define CPAK_CONTEXT_POOL_ENTRY_SIZE 0x01FC
#define CPAK_RECORD_POOL_ENTRY_COUNT 8
#define CPAK_SUBSCRIBER_STACK_SIZE 4096
#define CPAK_SUBSCRIBER_THREAD_NAME "CPAK"
#define CPAK_CREATION_NO_MEMORY -992
#define CPAK_CREATION_NO_SIGNAL -991
#define CPAK_CREATION_SIGNAL_ERROR -990
#define CPAK_SEMAPHORE_PRIORITY 100
#define CPAK_CHANNEL_SEMAPHORE_NAME_SIZE 8
#define CPAK_CHANNEL_NUMBER_NAME_SIZE 2
#define CPAK_SEMAPHORE_TERMINATOR_INDEX 2

int32
handle_cpak_control(void *context_, SubscriberMsgPtr message_)
{
  (void) context_;
  (void) message_;
  return 0;
}

int32
handle_cpak_opening(void *context_, SubscriberMsgPtr message_)
{
  (void) context_;
  (void) message_;
  return 0;
}

int32
handle_cpak_closing(void *context_, SubscriberMsgPtr message_)
{
  (void) context_;
  (void) message_;
  return 0;
}

int32
handle_cpak_end_of_file(void *context_, SubscriberMsgPtr message_)
{
  (void) context_;
  (void) message_;
  return 0;
}

SubscriberMsgPtr
get_next_cpak_chunk_message(CpakContextState *context_, CpakRecordState *record_)
{
  CpakChannelState *channel;
  SubscriberMsgPtr  message;

  channel = &context_->channels[record_->channel];
  LockSemaphore(channel->message_queue_semaphore, 1);
  message = get_next_subscriber_message(&channel->message_queue);
  UnlockSemaphore(channel->message_queue_semaphore);
  return message;
}

int32
poll_cpak_chunk(CpakContextState *context_,
                CpakRecordState  *record_,
                SubsChunkDataPtr *chunk_,
                SubscriberMsgPtr *message_)
{
  *message_ = get_next_cpak_chunk_message(context_, record_);
  if(*message_ == NULL)
    {
      return 0;
    }

  *chunk_ = (SubsChunkDataPtr) (*message_)->msg.data.buffer;
  return 1;
}

int32
initialize_cpak_record(void             *stream_control_block_,
                       CpakContextState *context_,
                       CpakRecordState **record_,
                       int32             channel_number_,
                       boolean           flush_on_sync_)
{
  CpakRecordState  *new_record;
  CpakChannelState *channel;

  new_record = (CpakRecordState *) allocate_memory_pool_entry(g_CPAK_RECORD_POOL);
  if(new_record == NULL)
    {
      return kDSNoMemErr;
    }

  fill_runtime_memory(new_record, 0, sizeof(CpakRecordState));
  new_record->current_frame        = NULL;
  new_record->current_message      = NULL;
  new_record->channel              = channel_number_;
  new_record->stream_control_block = stream_control_block_;
  new_record->last_current_time    = 0;
  *record_                         = new_record;

  channel                   = &context_->channels[channel_number_];
  channel->flush_on_sync    = flush_on_sync_;
  channel->image.x_position = 0;
  channel->image.y_position = 0;
  channel->status &= ~CPAK_CHANNEL_INITIALIZED;
  return 0;
}

int32
signal_cpak_free_messages(CpakContextState *context_)
{
  int32 status;

  status = 0;
  if(context_->free_list_not_empty)
    {
      status                        = SendSignal(context_->cpak_task, context_->free_queue_signal);
      context_->free_list_not_empty = 0;
    }
  return status;
}

int32
return_cpak_chunk_message(CpakContextState *context_, SubscriberMsgPtr message_)
{
  LockSemaphore(context_->free_queue_semaphore, 1);
  add_subscriber_message_to_tail(&context_->free_list, message_);
  UnlockSemaphore(context_->free_queue_semaphore);
  context_->free_list_not_empty = 1;
  signal_cpak_free_messages(context_);
  return 0;
}

void
reply_to_all_free_cpak_chunks(CpakContextState *context_)
{
  SubscriberMsgPtr message;

  LockSemaphore(context_->free_queue_semaphore, 1);
  while((message = get_next_subscriber_message(&context_->free_list)) != NULL)
    {
      ReplyMsg(message->msgItem, 0, message, 0);
    }
  UnlockSemaphore(context_->free_queue_semaphore);
}

void
flush_cpak_channel(CpakContextState *context_, CpakRecordState *record_, int32 channel_number_)
{
  SubsChunkDataPtr chunk;
  SubscriberMsgPtr message;

  (void) channel_number_;
  if(record_->current_message != NULL)
    {
      return_cpak_chunk_message(context_, record_->current_message);
      record_->current_message = NULL;
    }

  while(poll_cpak_chunk(context_, record_, &chunk, &message))
    {
      return_cpak_chunk_message(context_, message);
    }

  record_->current_frame = NULL;
  reply_to_all_free_cpak_chunks(context_);
}

int32
destroy_cpak_record(CpakContextState *context_, CpakRecordState *record_, int32 channel_number_)
{
  flush_cpak_channel(context_, record_, channel_number_);
  return_memory_pool_entry(g_CPAK_RECORD_POOL, record_);
  return 0;
}

int32
get_cpak_current_time(const CpakRecordState *record_)
{
  CinepakFrame *current_frame;

  current_frame = record_->current_frame;
  if(current_frame == NULL)
    {
      return -1;
    }
  return current_frame->time;
}

boolean
is_time_for_next_cpak_frame(CpakRecordState *record_)
{
  CinepakFrame *current_frame;
  uint32        current_time;

  if(get_data_stream_clock((DataStreamState *) record_->stream_control_block, &current_time) < 0)
    {
      return 0;
    }

  if(record_->last_current_time > current_time)
    {
      record_->last_current_time = current_time;
      return 1;
    }

  record_->last_current_time = current_time;
  current_frame              = record_->current_frame;
  if(current_frame == NULL)
    {
      return 1;
    }
  return current_time > current_frame->time + current_frame->duration;
}

boolean
is_cpak_callback_frame_due(const CpakRecordState *record_)
{
  uint32 current_time;

  if(get_data_stream_clock((DataStreamState *) record_->stream_control_block, &current_time) < 0)
    {
      return 0;
    }
  if(record_->last_current_time > current_time)
    {
      return 1;
    }
  if(record_->current_frame == NULL)
    {
      return 1;
    }
  return current_time >= record_->current_frame->time;
}

boolean
is_cpak_current_frame_due(CpakRecordState *record_)
{
  CinepakFrame *current_frame;
  uint32        current_time;

  if(get_data_stream_clock((DataStreamState *) record_->stream_control_block, &current_time) < 0)
    {
      return 0;
    }

  if(record_->last_current_time > current_time)
    {
      record_->last_current_time = current_time;
      return 1;
    }

  record_->last_current_time = current_time;
  current_frame              = record_->current_frame;
  if(current_frame == NULL)
    {
      return 1;
    }
  return current_time >= current_frame->time;
}

uint32
process_cpak_frames(CpakContextState *context_,
                    CpakRecordState  *record_,
                    uint32            options_,
                    int32             reserved_,
                    uint32           *frame_time_,
                    void             *destination_,
                    int32             row_byte_count_)
{
  CpakChannelState   *channel;
  MovieImage         *image;
  CinepakHeader       previous_header;
  SubsChunkDataPtr    chunk;
  CinepakFrame       *frame;
  SubscriberMsgPtr    message;
  RuntimeVideoWindow *video_window;
  uint32              processing_mode;
  uint32              result;
  uint32              frame_mode;
  uint32              current_time;
  boolean             continue_processing;

  (void) reserved_;
  channel             = &context_->channels[record_->channel];
  image               = &channel->image;
  processing_mode     = options_ & CPAK_OPTION_PROCESSING_MODE_MASK;
  result              = 0;
  continue_processing = 1;

  while(continue_processing)
    {
      if((!is_time_for_next_cpak_frame(record_)) && (processing_mode == 0))
        {
          break;
        }
      processing_mode = 0;

      if(!poll_cpak_chunk(context_, record_, &chunk, &message))
        {
          break;
        }

      frame = (CinepakFrame *) chunk;
      switch(frame->chunk_subtype)
        {
          case CPAK_CHUNK_FRAME_HEADER:
            result |= CPAK_RESULT_HEADER;
            if((options_ & CPAK_OPTION_STOP_AFTER_HEADER) != 0)
              {
                continue_processing = 0;
              }

            if((options_ & CPAK_OPTION_COMPARE_HEADER) != 0)
              {
                RUNTIME_MEMCPY_VENEER(&previous_header, &record_->header, sizeof(CinepakHeader));
              }
            RUNTIME_MEMCPY_VENEER(&record_->header, chunk, sizeof(CinepakHeader));
            if((options_ & CPAK_OPTION_COMPARE_HEADER) != 0)
              {
                if((previous_header.version != record_->header.version) ||
                   (previous_header.compression_type != record_->header.compression_type) ||
                   (previous_header.height != record_->header.height) ||
                   (previous_header.width != record_->header.width))
                  {
                    result |= CPAK_RESULT_HEADER_CHANGED;
                    continue_processing = 0;
                  }
              }

            if(((options_ & CPAK_OPTION_VIDEO_WINDOW) != 0) && (image->base_address == NULL))
              {
                create_movie_video_window(image, &record_->header);
              }
            channel->status |= CPAK_CHANNEL_INITIALIZED;
            record_->current_frame = NULL;
            return_cpak_chunk_message(context_, message);
            break;

          case CPAK_CHUNK_DIFFERENCE_FRAME:
          case CPAK_CHUNK_FRAME:
            record_->current_frame = frame;
            if(record_->current_message != NULL)
              {
                return_cpak_chunk_message(context_, record_->current_message);
              }

            frame_mode = CPAK_FRAME_DIFFERENCE;
            if((frame->chunk_subtype == CPAK_CHUNK_FRAME) && (frame->frame_data[0] == 0))
              {
                frame_mode = CPAK_FRAME_KEY;
              }

            if((options_ & frame_mode) != 0)
              {
                if((channel->status & CPAK_CHANNEL_INITIALIZED) != 0)
                  {
                    pre_decompress_cinepak_frame((CinepakCodecState *) context_->film_codec,
                                                 frame->frame_data);
                    if((options_ & CPAK_OPTION_VIDEO_WINDOW) != 0)
                      {
                        Decompress((codec) context_->film_codec,
                                   frame->frame_data,
                                   (char *) image->base_address,
                                   image->row_byte_count);
                      }
                    else
                      {
                        Decompress((codec) context_->film_codec,
                                   frame->frame_data,
                                   (char *) destination_,
                                   row_byte_count_);
                      }
                  }
                result |= frame_mode;
                *frame_time_ = frame->time;
                if((options_ & CPAK_OPTION_STOP_AFTER_FRAME) != 0)
                  {
                    continue_processing = 0;
                  }
                if((frame_mode == CPAK_FRAME_KEY) && ((options_ & CPAK_OPTION_KEY_FRAME) != 0))
                  {
                    options_ |= CPAK_FRAME_KEY | CPAK_FRAME_DIFFERENCE;
                  }
              }
            record_->current_message = message;
            break;

          default:
            break;
        }
    }

  if((get_data_stream_clock((DataStreamState *) record_->stream_control_block, &current_time) >=
      0) &&
     (record_->current_frame != NULL) &&
     (record_->current_frame->time + record_->current_frame->duration < current_time))
    {
      if(record_->current_message != NULL)
        {
          return_cpak_chunk_message(context_, record_->current_message);
          record_->current_message = NULL;
          record_->current_frame   = NULL;
        }
    }

  if((options_ & CPAK_OPTION_VIDEO_WINDOW) != 0)
    {
      if(record_->current_frame == NULL)
        {
          g_CPAK_FRAME_RESULT = 0;
        }
      else
        {
          video_window = (RuntimeVideoWindow *) image->video_window;
          if(video_window != NULL)
            {
              g_CPAK_FRAME_RESULT = (uint32) video_window->bitmap;
            }
          else
            {
              g_CPAK_FRAME_RESULT = 0;
            }
        }
    }
  else
    {
      g_CPAK_FRAME_RESULT = result;
    }
  return result;
}

int32
initialize_cpak_memory_pools(void)
{
  g_CPAK_CONTEXT_POOL =
    create_memory_pool(CPAK_CONTEXT_POOL_ENTRY_COUNT, CPAK_CONTEXT_POOL_ENTRY_SIZE);
  if(g_CPAK_CONTEXT_POOL == NULL)
    return -1;

  g_CPAK_RECORD_POOL = create_memory_pool(CPAK_RECORD_POOL_ENTRY_COUNT, sizeof(CpakRecordState));
  if(g_CPAK_CONTEXT_POOL == NULL)
    return -1;

  return 0;
}

int32
destroy_cpak_memory_pools(void)
{
  FREE_SECONDARY_MEMORY(g_CPAK_CONTEXT_POOL);
  g_CPAK_CONTEXT_POOL = NULL;
  FREE_SECONDARY_MEMORY(g_CPAK_RECORD_POOL);
  g_CPAK_RECORD_POOL = NULL;
  return 0;
}

int32
create_cpak_subscriber(CpakContextState **context_out_,
                       int32              channel_count_,
                       int32              priority_adjustment_)
{
  CpakContextState *context;
  uint32            received_signals;
  int32             status;

  context = allocate_memory_pool_entry(g_CPAK_CONTEXT_POOL);
  if(context == NULL)
    return CPAK_CREATION_NO_MEMORY;

  fill_runtime_memory(context, 0, CPAK_CONTEXT_POOL_ENTRY_SIZE);
  context->creator_task   = CURRENTTASK->t.n_Item;
  context->channel_count  = channel_count_;
  context->creator_signal = AllocSignal(0);
  if(context->creator_signal == 0)
    {
      status = CPAK_CREATION_NO_SIGNAL;
      goto cleanup;
    }

  status = create_streaming_thread((StreamingThreadEntry) cpak_subscriber_thread,
                                   CPAK_SUBSCRIBER_STACK_SIZE,
                                   (int32) CURRENTTASK->t.n_Priority + priority_adjustment_,
                                   CPAK_SUBSCRIBER_THREAD_NAME,
                                   &context->thread_stack_block,
                                   0,
                                   context);
  if(status <= 0)
    goto cleanup;

  context->thread_item = status;
  received_signals     = WaitSignal(context->creator_signal);
  if(received_signals != context->creator_signal)
    return CPAK_CREATION_SIGNAL_ERROR;

  FreeSignal(context->creator_signal);
  status = context->creator_status;
  if(status >= 0)
    {
      *context_out_ = context;
      return status;
    }

cleanup:
  if(context->thread_stack_block != NULL)
    free_streaming_thread_stack(context->thread_stack_block);

  return_memory_pool_entry(g_CPAK_CONTEXT_POOL, context);
  return status;
}

int32
dispose_cpak_subscriber(CpakContextState *context_)
{
  if(context_ != NULL)
    {
      if(context_->thread_item > 0)
        {
          dispose_streaming_thread(context_->thread_item);
        }
      if(context_->thread_stack_block != NULL)
        {
          free_streaming_thread_stack(context_->thread_stack_block);
        }
      return_memory_pool_entry(g_CPAK_CONTEXT_POOL, context_);
    }

  delete_cinepak_codec(context_->film_codec);
  delete_cinepak_handler(context_->codec_handler);
  return 0;
}

void
stop_cpak_channel(CpakContextState *context_, int32 channel_number_)
{
  if(channel_number_ < CPAK_MAX_CHANNELS)
    {
      context_->channels[channel_number_].status &= ~CHAN_ENABLED;
    }
}

int32
get_cpak_channel_status(CpakContextState *context_, SubscriberMsgPtr message_)
{
  int32 channel_number;

  channel_number = message_->msg.channel.number;
  if(channel_number < CPAK_MAX_CHANNELS)
    {
      return context_->channels[channel_number].status;
    }
  return 0;
}

int32
set_cpak_channel_status(CpakContextState *context_, SubscriberMsgPtr message_)
{
  int32 channel_number;

  channel_number = message_->msg.channel.number;
  if(channel_number < CPAK_MAX_CHANNELS)
    {
      context_->channels[channel_number].status |= message_->msg.channel.status & ~CHAN_SYSBITS;
    }
  return 0;
}

int32
stop_all_cpak_channels(CpakContextState *context_, SubscriberMsgPtr message_)
{
  int32 channel_number;

  (void) message_;
  for(channel_number = 0; channel_number < CPAK_MAX_CHANNELS; channel_number++)
    {
      stop_cpak_channel(context_, channel_number);
    }
  return 0;
}

int32
queue_cpak_data(CpakContextState *context_, SubscriberMsgPtr message_)
{
  SubsChunkDataPtr chunk;
  int32            channel_number;

  chunk          = (SubsChunkDataPtr) message_->msg.data.buffer;
  channel_number = chunk->channel;
  if(channel_number < context_->channel_count)
    {
      add_subscriber_message_to_tail(&context_->channels[channel_number].message_queue, message_);
    }
  else
    {
      ReplyMsg(message_->msgItem, 0, message_, 0);
    }
  return 0;
}

void
flush_queued_cpak_channel(CpakContextState *context_, int32 channel_number_)
{
  CpakChannelState *channel;
  SubscriberMsgPtr  message;
  SubscriberMsgPtr  next_message;

  if(channel_number_ < CPAK_MAX_CHANNELS)
    {
      channel = &context_->channels[channel_number_];
      LockSemaphore(channel->message_queue_semaphore, 1);
      message = channel->message_queue.head;
      while(message != NULL)
        {
          next_message = (SubscriberMsgPtr) message->link;
          ReplyMsg(message->msgItem, 0, message, sizeof(SubscriberMsgPtr));
          message = next_message;
        }
      channel->message_queue.head = NULL;
      channel->message_queue.tail = NULL;
      UnlockSemaphore(channel->message_queue_semaphore);
    }
}

int32
synchronize_cpak_channels(CpakContextState *context_, SubscriberMsgPtr message_)
{
  int32 channel_number;

  (void) message_;
  reply_to_all_free_cpak_chunks(context_);
  for(channel_number = 0; channel_number < CPAK_MAX_CHANNELS; channel_number++)
    {
      if(context_->channels[channel_number].flush_on_sync == 1)
        {
          flush_queued_cpak_channel(context_, channel_number);
        }
    }
  context_->time_changed = 1;
  return 0;
}

int32
abort_cpak_channels(CpakContextState *context_, SubscriberMsgPtr message_)
{
  int32 channel_number;

  (void) message_;
  for(channel_number = 0; channel_number < CPAK_MAX_CHANNELS; channel_number++)
    {
      flush_queued_cpak_channel(context_, channel_number);
      stop_cpak_channel(context_, channel_number);
    }
  return 0;
}

int32
initialize_cpak_thread(CpakContextState *context_)
{
  CpakChannelState *channel;
  int32             status;
  int32             channel_number;
  char              semaphore_name[CPAK_CHANNEL_SEMAPHORE_NAME_SIZE];
  char              channel_name[CPAK_CHANNEL_NUMBER_NAME_SIZE];

  context_->creator_status      = -1;
  context_->stream_clock_origin = 0;
  context_->request_port        = 0;
  context_->cue_item            = 0;
  context_->cue_signal          = 0;

  status = OpenAudioFolio();
  if(status < 0)
    {
      context_->creator_status = status;
    }
  else
    {
      context_->cpak_task      = CURRENTTASK->t.n_Item;
      context_->free_list.head = NULL;
      context_->free_queue_semaphore =
        create_runtime_semaphore("freeCPakQueue", CPAK_SEMAPHORE_PRIORITY);
      context_->free_queue_signal   = AllocSignal(0);
      context_->free_list_not_empty = 0;

      context_->codec_handler = create_cinepak_handler();
      if(context_->codec_handler == NULL)
        {
          context_->creator_status = -1;
        }
      else
        {
          context_->film_codec = create_cinepak_codec(context_->codec_handler, 0);
          if(context_->film_codec == NULL)
            {
              context_->creator_status = -1;
            }
          else
            {
              channel = context_->channels;
              for(channel_number = 0; channel_number < CPAK_MAX_CHANNELS; channel_number++)
                {
                  channel->status                                 = CHAN_ENABLED;
                  channel->message_queue.head                     = NULL;
                  channel->message_queue.tail                     = NULL;
                  channel->flush_on_sync                          = 0;
                  channel->image.base_address                     = NULL;
                  semaphore_name[0]                               = 'c';
                  semaphore_name[1]                               = 'p';
                  semaphore_name[CPAK_SEMAPHORE_TERMINATOR_INDEX] = 0;
                  channel_name[0]                                 = (char) ('0' + channel_number);
                  channel_name[1]                                 = 0;
                  append_runtime_string(semaphore_name, channel_name);
                  channel->message_queue_semaphore =
                    create_runtime_semaphore(semaphore_name, CPAK_SEMAPHORE_PRIORITY);
                  channel++;
                }

              status = create_message_port(&context_->request_port_signal);
              if(status > 0)
                {
                  context_->request_port = status;
                  status = CreateSizedItem(MKNODEID(AUDIONODE, AUDIO_CUE_NODE), NULL, 0);
                  if(status > 0)
                    {
                      context_->cue_item   = status;
                      context_->cue_signal = GetCueSignal(context_->cue_item);
                      if(context_->cue_signal != 0)
                        {
                          context_->creator_status = 0;
                        }
                    }
                }
            }
        }
    }

  status = SendSignal(context_->creator_task, context_->creator_signal);
  if((context_->creator_status < 0) || (status < 0))
    {
      return -1;
    }
  return 0;
}

void
cpak_subscriber_thread(int32 unused_, CpakContextState *context_)
{
  int32            status;
  SubscriberMsgPtr message;
  uint32           signal_bits;
  uint32           any_signal;
  int32            channel_number;
  boolean          keep_running;

  (void) unused_;
  keep_running = 1;
  status       = initialize_cpak_thread(context_);
  if(status != 0)
    {
      RUNTIME_EXIT_VENEER(0);
    }

  any_signal = context_->cue_signal | context_->request_port_signal | context_->free_queue_signal;
  while(keep_running)
    {
      signal_bits = WaitSignal(any_signal);
      if((signal_bits & context_->free_queue_signal) != 0)
        {
          reply_to_all_free_cpak_chunks(context_);
        }

      if((signal_bits & context_->request_port_signal) != 0)
        {
          while(poll_for_stream_message(context_->request_port,
                                        NULL,
                                        NULL,
                                        (void **) &message,
                                        &status))
            {
              switch(message->whatToDo)
                {
                  case kStreamOpData:
                    status = queue_cpak_data(context_, message);
                    break;

                  case kStreamOpGetChan:
                    status = get_cpak_channel_status(context_, message);
                    break;

                  case kStreamOpSetChan:
                    status = set_cpak_channel_status(context_, message);
                    break;

                  case kStreamOpControl:
                    status = handle_cpak_control(context_, message);
                    break;

                  case kStreamOpSync:
                    status = synchronize_cpak_channels(context_, message);
                    break;

                  case kStreamOpOpening:
                    status = handle_cpak_opening(context_, message);
                    break;

                  case kStreamOpClosing:
                    status       = handle_cpak_closing(context_, message);
                    keep_running = 0;
                    break;

                  case kStreamOpStop:
                    status = stop_all_cpak_channels(context_, message);
                    break;

                  case kStreamOpEOF:
                    status = handle_cpak_end_of_file(context_, message);
                    break;

                  case kStreamOpAbort:
                    status       = abort_cpak_channels(context_, message);
                    keep_running = 0;
                    break;

                  default:
                    break;
                }

              if(message->whatToDo != kStreamOpData)
                {
                  ReplyMsg(message->msgItem, status, message, 0);
                }
            }
        }
    }

  for(channel_number = 0; channel_number < CPAK_MAX_CHANNELS; channel_number++)
    {
      flush_queued_cpak_channel(context_, channel_number);
    }
  RUNTIME_EXIT_VENEER(0);
}
