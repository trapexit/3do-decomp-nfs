#include "data_acquisition_state.h"

#include "data_acquisition_context_pool.h"
#include "kernel_item.h"
#include "runtime_io.h"
#include "runtime_memory.h"
#include "sdk_veneers.h"
#include "stream_message_items.h"
#include "streaming_thread.h"
#include "streaming_thread_stack.h"
#include "tracked_memory.h"

#include "kernel.h"
#include "task.h"

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]
#define BIG_ENDIAN_HIGH_BYTE_SHIFT 8
#define ITEM_POOL_ALLOCATION_NAME "itemPool"
#define ACQUISITION_IO_REQUEST_POOL_CAPACITY 8
#define NFS_ACQ_STACK_SIZE 4096
#define NFS_ACQ_THREAD_NAME "DataAcq"
#define NFS_ACQ_NO_MEMORY_ERROR (-992)
#define NFS_ACQ_NO_SIGNAL_ERROR (-991)
#define NFS_ACQ_SIGNAL_ERROR (-990)
#define NFS_ACQ_BAD_READ_ERROR (-1000)
#define NFS_ACQ_WRONG_BLOCK_ERROR_TEXT "DA: GotWrongBlock"
#define NFS_ACQ_READ_ERROR_TEXT "DA: ReadError"

COMPILE_TIME_SIZE_ASSERT(nfs_acquisition_record_extent,
                         sizeof(NfsDataAcquisitionContext) == NFS_ACQUISITION_RECORD_EXTENT);
COMPILE_TIME_SIZE_ASSERT(nfs_acquisition_error_state_extent,
                         sizeof(DataAcquisitionErrorState) == NFS_ACQUISITION_ERROR_STATE_EXTENT);

const char  G_DATA_ACQUISITION_NO_ERROR_TEXT[] = "No Errors";
const char *g_DATA_ACQUISITION_ERROR_TEXT      = G_DATA_ACQUISITION_NO_ERROR_TEXT;
DataAcquisitionErrorState g_DATA_ACQUISITION_ERROR_STATE;

COMPILE_TIME_SIZE_ASSERT(acquisition_error_text_extent,
                         sizeof(g_DATA_ACQUISITION_ERROR_TEXT) == 4);

int32
create_data_acquisition(NfsDataAcquisitionContext **context_out_,
                        char                       *file_name_,
                        int32                       priority_adjustment_)
{
  NfsDataAcquisitionContext *context;
  uint32                     received_signals;
  int32                      status;

  context = allocate_memory_pool_entry(g_STREAM_REQUEST_POOL);
  if(context == NULL)
    {
      return NFS_ACQ_NO_MEMORY_ERROR;
    }

  context->file_name      = file_name_;
  context->creator_task   = CURRENTTASK->t.n_Item;
  context->creator_signal = AllocSignal(0);
  if(context->creator_signal == 0)
    {
      status = NFS_ACQ_NO_SIGNAL_ERROR;
      goto cleanup;
    }

  status = create_streaming_thread((StreamingThreadEntry) run_data_acquisition_thread,
                                   NFS_ACQ_STACK_SIZE,
                                   (int32) CURRENTTASK->t.n_Priority + priority_adjustment_,
                                   NFS_ACQ_THREAD_NAME,
                                   &context->thread_stack,
                                   0,
                                   context);
  if(status <= 0)
    {
      goto cleanup;
    }
  context->thread_item = status;

  received_signals = WaitSignal(context->creator_signal);
  if(received_signals != context->creator_signal)
    {
      return NFS_ACQ_SIGNAL_ERROR;
    }

  FreeSignal(context->creator_signal);
  status = context->creator_status;
  if(status >= 0)
    {
      *context_out_ = context;
      return status;
    }

cleanup:
  if(context->thread_stack != NULL)
    {
      free_streaming_thread_stack(context->thread_stack);
    }
  return_memory_pool_entry(g_STREAM_REQUEST_POOL, context);
  return status;
}

void
dispose_data_acquisition(NfsDataAcquisitionContext *context_)
{
  DataAcqMsg message;
  Message   *reply_message;
  Item       message_item;
  Item       reply_port;
  int32      status;

  if(context_ == NULL)
    {
      return;
    }

  reply_port = create_message_port(NULL);
  if(reply_port < 0)
    {
      runtime_debug_printf("DisposeDataAcq - NewMsgPort() failed\n");
      return;
    }

  message_item = create_stream_message_item(reply_port);
  if(message_item < 0)
    {
      runtime_debug_printf("DisposeDataAcq - CreateMsgItem() failed\n");
      return;
    }

  message.whatToDo = kAcqOpExit;
  message.msgItem  = message_item;
  status           = SendMsg(context_->request_port, message_item, &message, sizeof(DataAcqMsg));
  if(status >= 0)
    {
      status = wait_for_stream_message(reply_port, NULL, &reply_message, NULL, message_item);
    }

  if(context_->thread_item > 0)
    {
      dispose_streaming_thread(context_->thread_item);
    }
  if(context_->thread_stack != NULL)
    {
      free_streaming_thread_stack(context_->thread_stack);
    }
  return_memory_pool_entry(g_STREAM_REQUEST_POOL, context_);
  remove_stream_message_item(message_item);
  remove_message_port(reply_port);
}

boolean
queue_data_acquisition_request(NfsDataAcquisitionContext *context_, DataAcqMsgPtr message_)
{
  boolean queue_was_empty;

  message_->link = NULL;
  if(context_->pending_request_head != NULL)
    {
      context_->pending_request_tail->link = message_;
      context_->pending_request_tail       = message_;
      queue_was_empty                      = 0;
    }
  else
    {
      context_->pending_request_head = message_;
      context_->pending_request_tail = message_;
      queue_was_empty                = 1;
    }

  return queue_was_empty;
}

DataAcqMsgPtr
dequeue_data_acquisition_request(NfsDataAcquisitionContext *context_)
{
  DataAcqMsgPtr message;

  message = context_->pending_request_head;
  if(message != NULL)
    {
      context_->pending_request_head = (DataAcqMsgPtr) message->link;
      if(context_->pending_request_tail == message)
        {
          context_->pending_request_tail = NULL;
        }
    }

  return message;
}

DataAcqMsgPtr
take_data_acquisition_request_by_io_item(NfsDataAcquisitionContext *context_,
                                         DataAcqMsgPtr             *queue_head_,
                                         Item                       io_request_)
{
  DataAcqMsgPtr message;
  DataAcqMsgPtr previous;

  (void) context_;
  previous = NULL;
  for(message = *queue_head_; message != NULL; message = (DataAcqMsgPtr) message->link)
    {
      if(message->msg.data.bufferPtr->ioreqItem == io_request_)
        {
          if(previous == NULL)
            {
              *queue_head_ = (DataAcqMsgPtr) message->link;
            }
          else
            {
              previous->link = message->link;
            }
          return message;
        }
      previous = message;
    }

  return NULL;
}

ItemPoolPtr
create_data_acquisition_io_pool(int32 item_count_, CreateProcPtr create_item_, void *argument_)
{
  ItemPoolPtr pool;
  ItemDescPtr descriptor;

  pool = (ItemPoolPtr) ALLOCATE_NAMED_SECONDARY_MEMORY(sizeof(ItemPool), ITEM_POOL_ALLOCATION_NAME);
  if(pool == NULL)
    {
      return NULL;
    }

  pool->numItemsInPool = item_count_;
  pool->inUseList      = NULL;
  pool->availList      = NULL;
  pool->itemDescBlockPtr =
    ALLOCATE_NAMED_SECONDARY_MEMORY(item_count_ * sizeof(ItemDesc), ITEM_POOL_ALLOCATION_NAME);
  if(pool->itemDescBlockPtr == NULL)
    {
      return pool;
    }

  descriptor = (ItemDescPtr) pool->itemDescBlockPtr;
  while(item_count_-- > 0)
    {
      descriptor->item = create_item_(argument_);
      if(descriptor->item <= 0)
        {
          break;
        }

      descriptor->next = pool->availList;
      pool->availList  = descriptor;
      descriptor++;
    }

  return pool;
}

Item
allocate_data_acquisition_io_item(ItemPoolPtr pool_)
{
  ItemDescPtr descriptor;
  Item        item;

  item       = 0;
  descriptor = pool_->availList;
  if(descriptor != NULL)
    {
      item             = descriptor->item;
      pool_->availList = descriptor->next;
      descriptor->next = pool_->inUseList;
      pool_->inUseList = descriptor;
    }

  return item;
}

void
return_data_acquisition_io_item(ItemPoolPtr pool_, Item item_)
{
  ItemDescPtr descriptor;
  ItemDescPtr previous;

  previous   = NULL;
  descriptor = pool_->inUseList;
  while(descriptor != NULL)
    {
      if(descriptor->item == item_)
        {
          if(previous == NULL)
            {
              pool_->inUseList = descriptor->next;
            }
          else
            {
              previous->next = descriptor->next;
            }
          descriptor->next = pool_->availList;
          pool_->availList = descriptor;
          return;
        }
      previous   = descriptor;
      descriptor = descriptor->next;
    }
}

void
delete_data_acquisition_io_pool(ItemPoolPtr pool_)
{
  ItemDescPtr descriptor;

  descriptor = pool_->availList;
  while(descriptor != NULL)
    {
      DeleteItem(descriptor->item);
      descriptor = descriptor->next;
    }

  FREE_SECONDARY_MEMORY(pool_->itemDescBlockPtr);
  FREE_SECONDARY_MEMORY(pool_);
}

Item
create_data_acquisition_io_request(void *argument_)
{
  NfsDataAcquisitionContext *context;

  context = argument_;
  return create_disk_file_io_request(context->file.item, context->io_reply_port);
}

int32
open_data_acquisition_stream_file(NfsDataAcquisitionContext *context_)
{
  int32 result;

  context_->file.item = 0;
  result              = open_disk_file_handle(context_->file_name, &context_->file);
  if(result != 0)
    {
      return result;
    }

  context_->io_request_pool = create_data_acquisition_io_pool(ACQUISITION_IO_REQUEST_POOL_CAPACITY,
                                                              create_data_acquisition_io_request,
                                                              context_);
  return 0;
}

void
flush_pending_data_acquisition_io(NfsDataAcquisitionContext *context_)
{
  DataAcqMsgPtr message;
  DataAcqMsgPtr last_aborted_message;

  for(message = context_->request_queue; message != NULL; message = (DataAcqMsgPtr) message->link)
    {
      AbortIO(message->msg.data.bufferPtr->ioreqItem);
    }

  last_aborted_message = NULL;
  for(message = context_->abort_queue; message != NULL; message = (DataAcqMsgPtr) message->link)
    {
      last_aborted_message = message;
    }

  if(last_aborted_message == NULL)
    {
      context_->abort_queue = context_->request_queue;
    }
  else
    {
      last_aborted_message->link = context_->request_queue;
    }
  context_->request_queue = NULL;
}

void
set_data_acquisition_file_data_offset(NfsDataAcquisitionContext *context_, int32 file_data_offset_)
{
  context_->file_data_offset = file_data_offset_;
}

void
set_data_acquisition_target_channel(NfsDataAcquisitionContext *context_, int32 target_channel_)
{
  context_->target_channel = target_channel_;
}

int32
connect_data_acquisition(NfsDataAcquisitionContext *context_, DataAcqMsgPtr message_)
{
  (void) context_;
  (void) message_;

  return 0;
}

int32
disconnect_data_acquisition(NfsDataAcquisitionContext *context_)
{
  flush_pending_data_acquisition_io(context_);
  return 0;
}

int32
map_data_acquisition_marker(NfsDataAcquisitionContext *context_,
                            uint32                     marker_value_,
                            int32                      marker_options_,
                            int32                     *file_offset_)
{
  if(marker_options_ != GOMARKER_ABSOLUTE && marker_options_ == GOMARKER_ABS_TIME)
    {
      context_->time_seek_pending  = 1;
      context_->target_stream_time = marker_value_;
      marker_value_                = context_->read_offset;
    }

  if(marker_value_ >= get_disk_file_byte_count(&context_->file))
    {
      return kDSEndOfFileErr;
    }

  *file_offset_ = marker_value_;
  return 0;
}

int32
seek_data_acquisition(NfsDataAcquisitionContext *context_, DataAcqMsgPtr message_)
{
  int32 file_offset;
  int32 result;

  result = map_data_acquisition_marker(context_,
                                       message_->msg.marker.value,
                                       message_->msg.marker.options,
                                       &file_offset);
  if(result != 0)
    {
      return result;
    }

  flush_pending_data_acquisition_io(context_);
  context_->read_offset           = file_offset;
  context_->expected_block_offset = file_offset;
  context_->wrong_block_reported  = 0;
  context_->end_of_file_reported  = 0;
  return 0;
}

static uint16
_read_big_endian_uint16(const uint16 *value_)
{
  const uint8 *bytes;

  bytes = (const uint8 *) value_;
  return (uint16) (((uint16) bytes[0] << BIG_ENDIAN_HIGH_BYTE_SHIFT) | bytes[1]);
}

int32
find_data_acquisition_frame(NfsDataAcquisitionContext         *context_,
                            const NfsFilmTableOfContentsChunk *table_of_contents_)
{
  uint32 elapsed_time;
  uint16 duration_count;
  uint16 duration;
  uint16 first_frame;
  int32  selected_index;
  int32  index;

  elapsed_time   = 0;
  selected_index = 0;
  duration_count = _read_big_endian_uint16(&table_of_contents_->duration_count);

  for(index = 0; index < duration_count; index++)
    {
      duration = _read_big_endian_uint16(&table_of_contents_->frame_durations[index]);
      if(duration == 0)
        {
          continue;
        }

      elapsed_time += duration;
      if(elapsed_time > context_->target_stream_time)
        {
          first_frame = _read_big_endian_uint16(&table_of_contents_->first_frame);
          return first_frame + selected_index;
        }
      selected_index = index;
    }

  if(table_of_contents_->terminal_frame != 0)
    {
      return table_of_contents_->terminal_frame;
    }

  first_frame = _read_big_endian_uint16(&table_of_contents_->first_frame);
  return first_frame + duration_count - 1;
}

void
scan_data_acquisition_buffer_for_seek(NfsDataAcquisitionContext *context_,
                                      void                      *buffer_,
                                      int32                      buffer_size_)
{
  NfsSubscriberChunk          *chunk;
  NfsFilmTableOfContentsChunk *table_of_contents;
  NfsCinePakFrameChunk        *frame;
  NfsSubscriberChunk          *first_chunk;
  uint8                       *chunk_bytes;
  uint8                       *first_bytes;
  int32                        remaining_bytes;
  int32                        block_index;
  int32                        block_offset;

  first_bytes     = (uint8 *) buffer_;
  chunk_bytes     = first_bytes;
  remaining_bytes = buffer_size_ - NFS_STREAM_CHUNK_COMMON_EXTENT;

  while(remaining_bytes > 0)
    {
      chunk = (NfsSubscriberChunk *) chunk_bytes;

      if(chunk->chunk_type == NFS_DACQ_CHUNK_TYPE &&
         chunk->chunk_subtype == NFS_TABLE_OF_CONTENTS_CHUNK_SUBTYPE)
        {
          table_of_contents = (NfsFilmTableOfContentsChunk *) chunk;
          if(_read_big_endian_uint16(&table_of_contents->version) == 1)
            {
              block_index = find_data_acquisition_frame(context_, table_of_contents);
              if(block_index < 0)
                {
                  break;
                }

              block_offset = buffer_size_ * block_index;
              if(block_offset > context_->read_offset)
                {
                  context_->read_offset           = block_offset;
                  context_->expected_block_offset = block_offset;
                  return;
                }
            }
        }
      else if(chunk->stream_time >= context_->target_stream_time &&
              chunk->chunk_type == NFS_FILM_CHUNK_TYPE &&
              chunk->chunk_subtype == NFS_FRAME_CHUNK_SUBTYPE &&
              chunk->channel == context_->target_channel)
        {
          frame = (NfsCinePakFrameChunk *) chunk;
          if(frame->frame_data[0] == 0)
            {
              context_->time_seek_pending = 0;
              if(chunk_bytes != first_bytes)
                {
                  first_chunk             = (NfsSubscriberChunk *) first_bytes;
                  first_chunk->chunk_type = NFS_FILL_CHUNK_TYPE;
                  first_chunk->chunk_size = chunk_bytes - first_bytes;
                }
              break;
            }
        }

      chunk_bytes += chunk->chunk_size;
      remaining_bytes -= chunk->chunk_size;
    }

  context_->read_offset += buffer_size_;
  context_->expected_block_offset = context_->read_offset;
}

int32
read_data_acquisition_for_seek(NfsDataAcquisitionContext *context_, DataAcqMsgPtr message_)
{
  DSDataBufPtr buffer;
  int32        readable_byte_count;
  int32        scan_byte_count;
  int32        result;

  buffer              = message_->msg.data.bufferPtr;
  readable_byte_count = get_disk_file_byte_count(&context_->file) - context_->file_data_offset;
  buffer->ioreqItem   = allocate_data_acquisition_io_item(context_->io_request_pool);
  if(buffer->ioreqItem == 0)
    {
      queue_data_acquisition_request(context_, message_);
      return 0;
    }

  buffer->ioreqItemPtr = (IOReq *) lookup_kernel_item(buffer->ioreqItem);
  result               = 0;
  while(context_->read_offset < readable_byte_count && context_->time_seek_pending == 1)
    {
      result = read_disk_file_async(&context_->file,
                                    buffer->ioreqItem,
                                    &buffer->streamData,
                                    message_->msg.data.bufferSize,
                                    context_->read_offset + context_->file_data_offset);
      if(result < 0)
        {
          break;
        }

      result = wait_for_disk_file_read(buffer->ioreqItem);
      if(result < 0)
        {
          break;
        }
      if(buffer->ioreqItemPtr->io_Error != 0)
        {
          result = buffer->ioreqItemPtr->io_Error;
          break;
        }

      if(context_->read_offset + message_->msg.data.bufferSize <= readable_byte_count)
        {
          scan_byte_count = message_->msg.data.bufferSize;
        }
      else
        {
          scan_byte_count = readable_byte_count - context_->read_offset;
        }
      scan_data_acquisition_buffer_for_seek(context_, &buffer->streamData, scan_byte_count);
    }

  if(context_->time_seek_pending == 0)
    {
      ReplyMsg(message_->msgItem, result, message_, sizeof(DataAcqMsg));
    }
  else
    {
      context_->time_seek_pending = 0;
    }

  return_data_acquisition_io_item(context_->io_request_pool, buffer->ioreqItem);
  if(context_->read_offset >= readable_byte_count)
    {
      if(context_->end_of_file_reported)
        {
          result = kDSWasFlushedErr;
        }
      else
        {
          result = kDSEndOfFileErr;
        }
      context_->end_of_file_reported = 1;
    }

  return result;
}

int32
request_data_acquisition_buffer(NfsDataAcquisitionContext *context_, DataAcqMsgPtr message_)
{
  DSDataBufPtr buffer;
  int32        result;

  if(context_->time_seek_pending == 1)
    {
      return read_data_acquisition_for_seek(context_, message_);
    }

  if((uint32) context_->read_offset >= get_disk_file_byte_count(&context_->file))
    {
      if(context_->end_of_file_reported)
        {
          result = kDSWasFlushedErr;
        }
      else
        {
          result = kDSEndOfFileErr;
        }
      context_->end_of_file_reported = 1;
      return result;
    }

  buffer            = message_->msg.data.bufferPtr;
  buffer->ioreqItem = allocate_data_acquisition_io_item(context_->io_request_pool);
  if(buffer->ioreqItem == 0)
    {
      queue_data_acquisition_request(context_, message_);
      return 0;
    }

  buffer->ioreqItemPtr = (IOReq *) lookup_kernel_item(buffer->ioreqItem);
  result               = read_disk_file_async(&context_->file,
                                              buffer->ioreqItem,
                                              &buffer->streamData,
                                              message_->msg.data.bufferSize,
                                              context_->read_offset + context_->file_data_offset);
  if(result >= 0)
    {
      context_->read_offset += message_->msg.data.bufferSize;
      message_->link          = context_->request_queue;
      context_->request_queue = message_;
    }

  return result;
}

int32
initialize_data_acquisition_thread(NfsDataAcquisitionContext *context_)
{
  int32 result;

  context_->creator_status        = -1;
  context_->thread_item           = 0;
  context_->abort_queue           = NULL;
  context_->request_port          = 0;
  context_->io_reply_port         = 0;
  context_->request_queue         = NULL;
  context_->pending_request_head  = NULL;
  context_->pending_request_tail  = NULL;
  context_->end_of_file_reported  = 0;
  context_->read_offset           = 0;
  context_->file_data_offset      = 0;
  context_->expected_block_offset = 0;
  context_->wrong_block_reported  = 0;
  context_->time_seek_pending     = 0;
  context_->target_stream_time    = 0;
  context_->target_channel        = 0;
  context_->io_request_pool       = NULL;

  result = create_message_port(&context_->request_signal);
  if(result > 0)
    {
      context_->request_port = result;
      result                 = create_message_port(&context_->io_reply_signal);
      if(result > 0)
        {
          context_->io_reply_port = result;
          result                  = open_data_acquisition_stream_file(context_);
          if(result == 0)
            {
              context_->creator_status = 0;
            }
        }
    }

  result = SendSignal(context_->creator_task, context_->creator_signal);
  if(context_->creator_status < 0 || result < 0)
    {
      return -1;
    }

  return 0;
}

void
run_data_acquisition_thread(int32 unused_, NfsDataAcquisitionContext *context_)
{
  DataAcqMsgPtr exit_message;
  DataAcqMsgPtr message;
  IOReq        *io_request_record;
  Item          io_request;
  uint32        awaited_signals;
  uint32        received_signals;
  int32         retry_count;
  int32         running;
  int32         wants_to_exit;
  int32         actual_byte_offset;
  int32         result;

  (void) unused_;
  wants_to_exit = 0;
  exit_message  = NULL;
  if(initialize_data_acquisition_thread(context_) != 0)
    {
      RUNTIME_EXIT_VENEER(0);
    }

  awaited_signals = context_->request_signal | context_->io_reply_signal;
  running         = 1;
  while(running != 0)
    {
      received_signals = WaitSignal(awaited_signals);

      if((received_signals & context_->io_reply_signal) != 0)
        {
          while(poll_for_stream_message(context_->io_reply_port,
                                        NULL,
                                        NULL,
                                        (void **) &io_request,
                                        &result))
            {
              message = take_data_acquisition_request_by_io_item(context_,
                                                                 &context_->request_queue,
                                                                 io_request);
              if(message != NULL)
                {
                  retry_count       = 0;
                  io_request_record = message->msg.data.bufferPtr->ioreqItemPtr;
                  result            = io_request_record->io_Error;
                  while(result != 0)
                    {
                      runtime_debug_printf("DataAcq read error %ld\n", result);
                      g_DATA_ACQUISITION_ERROR_TEXT = NFS_ACQ_READ_ERROR_TEXT;
                      g_DATA_ACQUISITION_ERROR_STATE.error_count++;
                      g_DATA_ACQUISITION_ERROR_STATE.last_error     = result;
                      g_DATA_ACQUISITION_ERROR_STATE.error_reported = 1;
                      if(retry_count >= 1)
                        {
                          break;
                        }

                      retry_count++;
                      result = read_disk_file_async(&context_->file,
                                                    io_request,
                                                    io_request_record->io_Info.ioi_Recv.iob_Buffer,
                                                    io_request_record->io_Info.ioi_Recv.iob_Len,
                                                    context_->expected_block_offset +
                                                      context_->file_data_offset);
                      if(result >= 0)
                        {
                          wait_for_io_completion(io_request);
                        }
                      result = io_request_record->io_Error;
                    }

                  if(result == 0)
                    {
                      actual_byte_offset = ((io_request_record->io_Info.ioi_Offset *
                                             context_->file.status.fs.ds_DeviceBlockSize) -
                                            context_->file_data_offset);
                      if(actual_byte_offset != context_->expected_block_offset)
                        {
                          if(context_->wrong_block_reported == 0)
                            {
                              result                         = NFS_ACQ_BAD_READ_ERROR;
                              context_->wrong_block_reported = 1;
                            }
                          runtime_debug_printf("DataAcq found block offset %ld, but expected %ld\n",
                                               actual_byte_offset,
                                               context_->expected_block_offset);
                          g_DATA_ACQUISITION_ERROR_TEXT = NFS_ACQ_WRONG_BLOCK_ERROR_TEXT;
                        }
                      else
                        {
                          context_->expected_block_offset += io_request_record->io_Actual;
                        }
                    }
                }
              else
                {
                  message = take_data_acquisition_request_by_io_item(context_,
                                                                     &context_->abort_queue,
                                                                     io_request);
                  if(message != NULL)
                    {
                      result = kDSWasFlushedErr;
                    }
                }

              if(message != NULL)
                {
                  ReplyMsg(message->msgItem, result, message, sizeof(DataAcqMsg));
                  return_data_acquisition_io_item(context_->io_request_pool, io_request);

                  message = dequeue_data_acquisition_request(context_);
                  if(message != NULL)
                    {
                      result = request_data_acquisition_buffer(context_, message);
                      if(result < 0)
                        {
                          ReplyMsg(message->msgItem, result, message, sizeof(DataAcqMsg));
                        }
                    }
                }
              else
                {
                  runtime_debug_printf("DataAcqThread got unknown i/o reply message!");
                }
            }
        }

      if((received_signals & context_->request_signal) != 0)
        {
          while(poll_for_stream_message(context_->request_port,
                                        NULL,
                                        NULL,
                                        (void **) &message,
                                        &result))
            {
              switch(message->whatToDo)
                {
                  case kAcqOpGetData:
                    result = request_data_acquisition_buffer(context_, message);
                    if(result < 0)
                      {
                        ReplyMsg(message->msgItem, result, message, sizeof(DataAcqMsg));
                      }
                    break;

                  case kAcqOpGoMarker:
                    result = seek_data_acquisition(context_, message);
                    if(result < 0)
                      {
                        ReplyMsg(message->msgItem, result, message, sizeof(DataAcqMsg));
                      }
                    break;

                  case kAcqOpConnect:
                    result = connect_data_acquisition(context_, message);
                    break;

                  case kAcqOpDisconnect:
                    result = disconnect_data_acquisition(context_);
                    break;

                  case kAcqOpExit:
                    wants_to_exit = 1;
                    if(exit_message == NULL)
                      {
                        exit_message = message;
                      }
                    else
                      {
                        runtime_debug_printf("DataAcqThread got multiple kAcqOpExit messages!\n");
                      }
                    break;

                  default:
                    break;
                }

              if(message->whatToDo != kAcqOpGetData && message->whatToDo != kAcqOpExit)
                {
                  ReplyMsg(message->msgItem, result, message, sizeof(DataAcqMsg));
                }
            }
        }

      if(wants_to_exit != 0 && context_->abort_queue == NULL && context_->request_queue == NULL)
        {
          ReplyMsg(exit_message->msgItem, 0, exit_message, sizeof(DataAcqMsg));
          running = 0;
        }
    }

  dispose_data_acquisition_resources(context_);
  context_->thread_item = 0;
  RUNTIME_EXIT_VENEER(0);
}

void
dispose_data_acquisition_resources(NfsDataAcquisitionContext *context_)
{
  if(context_->file.item != 0)
    {
      close_disk_file_handle(&context_->file);
      context_->file.item = 0;

      if(context_->io_request_pool != NULL)
        {
          delete_data_acquisition_io_pool(context_->io_request_pool);
          context_->io_request_pool = NULL;
        }
    }
}
