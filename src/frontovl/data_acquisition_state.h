#pragma once

#include "disk_file.h"

#include "dataacq.h"

#define NFS_ACQUISITION_RECORD_EXTENT 0x88
#define NFS_ACQUISITION_ERROR_STATE_EXTENT 0x0C
#define NFS_STREAM_CHUNK_COMMON_EXTENT 0x14
#define NFS_DACQ_CHUNK_TYPE 0x44414351
#define NFS_FILL_CHUNK_TYPE 0x46494C4C
#define NFS_FILM_CHUNK_TYPE 0x46494C4D
#define NFS_FRAME_CHUNK_SUBTYPE 0x46524D45
#define NFS_TABLE_OF_CONTENTS_CHUNK_SUBTYPE 0x46544F43

typedef struct NfsSubscriberChunk
{
  uint32 chunk_type;
  uint32 chunk_size;
  uint32 stream_time;
  int32  channel;
  uint32 chunk_subtype;
} NfsSubscriberChunk;

typedef struct NfsFilmTableOfContentsChunk
{
  uint32 chunk_type;
  uint32 chunk_size;
  uint32 stream_time;
  int32  channel;
  uint32 chunk_subtype;
  uint16 version;
  uint16 reserved;
  uint32 terminal_frame;
  uint16 first_frame;
  uint16 duration_count;
  uint16 frame_durations[1];
} NfsFilmTableOfContentsChunk;

typedef struct NfsCinePakFrameChunk
{
  uint32 chunk_type;
  uint32 chunk_size;
  uint32 stream_time;
  int32  channel;
  uint32 chunk_subtype;
  int32  duration;
  int32  frame_size;
  uint8  frame_data[1];
} NfsCinePakFrameChunk;

typedef struct DataAcquisitionErrorState
{
  uint32 error_count;
  int32  last_error;
  uint8  error_reported;
  uint8  reserved[3];
} DataAcquisitionErrorState;

typedef struct NfsDataAcquisitionContext
{
  Item           creator_task;
  uint32         creator_signal;
  int32          creator_status;
  char          *file_name;
  ItemPoolPtr    io_request_pool;
  Item           thread_item;
  void          *thread_stack;
  Item           request_port;
  uint32         request_signal;
  Item           io_reply_port;
  uint32         io_reply_signal;
  DataAcqMsgPtr  request_queue;
  DataAcqMsgPtr  abort_queue;
  DataAcqMsgPtr  pending_request_head;
  DataAcqMsgPtr  pending_request_tail;
  boolean        end_of_file_reported;
  int32          read_offset;
  DiskFileHandle file;
  int32          file_data_offset;
  int32          expected_block_offset;
  boolean        wrong_block_reported;
  int32          time_seek_pending;
  uint32         target_stream_time;
  int32          target_channel;
} NfsDataAcquisitionContext;

extern const char  G_DATA_ACQUISITION_NO_ERROR_TEXT[];
extern const char *g_DATA_ACQUISITION_ERROR_TEXT;
extern DataAcquisitionErrorState g_DATA_ACQUISITION_ERROR_STATE;

int32
create_data_acquisition(NfsDataAcquisitionContext **context_out_,
                        char                       *file_name_,
                        int32                       priority_adjustment_);
void
dispose_data_acquisition(NfsDataAcquisitionContext *context_);
void
flush_pending_data_acquisition_io(NfsDataAcquisitionContext *context_);
boolean
queue_data_acquisition_request(NfsDataAcquisitionContext *context_, DataAcqMsgPtr message_);
DataAcqMsgPtr
dequeue_data_acquisition_request(NfsDataAcquisitionContext *context_);
DataAcqMsgPtr
take_data_acquisition_request_by_io_item(NfsDataAcquisitionContext *context_,
                                         DataAcqMsgPtr             *queue_head_,
                                         Item                       io_request_);
ItemPoolPtr
create_data_acquisition_io_pool(int32 item_count_, CreateProcPtr create_item_, void *argument_);
Item
allocate_data_acquisition_io_item(ItemPoolPtr pool_);
void
return_data_acquisition_io_item(ItemPoolPtr pool_, Item item_);
void
delete_data_acquisition_io_pool(ItemPoolPtr pool_);
Item
create_data_acquisition_io_request(void *argument_);
int32
open_data_acquisition_stream_file(NfsDataAcquisitionContext *context_);
void
set_data_acquisition_file_data_offset(NfsDataAcquisitionContext *context_, int32 file_data_offset_);
void
set_data_acquisition_target_channel(NfsDataAcquisitionContext *context_, int32 target_channel_);
int32
connect_data_acquisition(NfsDataAcquisitionContext *context_, DataAcqMsgPtr message_);
int32
disconnect_data_acquisition(NfsDataAcquisitionContext *context_);
int32
map_data_acquisition_marker(NfsDataAcquisitionContext *context_,
                            uint32                     marker_value_,
                            int32                      marker_options_,
                            int32                     *file_offset_);
int32
seek_data_acquisition(NfsDataAcquisitionContext *context_, DataAcqMsgPtr message_);
int32
find_data_acquisition_frame(NfsDataAcquisitionContext         *context_,
                            const NfsFilmTableOfContentsChunk *table_of_contents_);
void
scan_data_acquisition_buffer_for_seek(NfsDataAcquisitionContext *context_,
                                      void                      *buffer_,
                                      int32                      buffer_size_);
int32
read_data_acquisition_for_seek(NfsDataAcquisitionContext *context_, DataAcqMsgPtr message_);
int32
request_data_acquisition_buffer(NfsDataAcquisitionContext *context_, DataAcqMsgPtr message_);
int32
initialize_data_acquisition_thread(NfsDataAcquisitionContext *context_);
void
run_data_acquisition_thread(int32 unused_, NfsDataAcquisitionContext *context_);
void
dispose_data_acquisition_resources(NfsDataAcquisitionContext *context_);
