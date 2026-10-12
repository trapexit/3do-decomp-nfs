#include "game_session_resources.h"

#include "claim_flag.h"
#include "game_data_path.h"
#include "headered_file.h"
#include "memory.h"
#include "render_thread_signal.h"
#include "road_data.h"
#include "simulation_resource_cleanup.h"
#include "string_utilities.h"
#include "track_data_path.h"
#include "track_disk_stream.h"
#include "vbl_delay.h"

#include "filestreamfunctions.h"
#include "mem.h"
#include "task.h"

#include "stddef.h"
#include "stdio.h"
#include "stdlib.h"

#define GAME_SESSION_RESOURCE_BUFFER_BYTES 0x75400
#define GAME_SESSION_DISK_STREAM_BUFFER_BYTES 0x2800
#define GAME_SESSION_PACKET_PATH_BYTES 100
#define GAME_SESSION_PACKET_HEADER_WORDS 2
#define GAME_SESSION_OBJECT_RESOURCE_PACKET_INDEX 2
#define GAME_SESSION_PACKET_PATH_FORMAT "%sDriveData/DriveArt/%s_PKT_%.3d"
#define GAME_SESSION_RESOURCE_MEMORY_FLAGS 0
#define GAME_SESSION_STREAM_INDEX_NONE -1
#define GAME_SESSION_WORKER_STACK_BYTES 4000
#define GAME_SESSION_WORKER_PRIORITY 8
#define GAME_SESSION_START_DELAY_FIELDS 2
#define GAME_SESSION_WAIT_DELAY_FIELDS 1
#define GAME_SESSION_RENDER_WAIT_COMMAND 3
#define GAME_SESSION_WORKER_NAME "Fido"
#define GAME_SESSION_QUEUE_FULL_EXIT_STATUS 1

GameSessionState g_GAME_SESSION_STATE = { { 0 },
                                          0,
                                          0,
                                          0,
                                          { 0 },
                                          { { 0, 0 } },
                                          0,
                                          0,
                                          0,
                                          { 0 },
                                          0,
                                          { 0 },
                                          { 0, NULL, 0 },
                                          0,
                                          0,
                                          { 0 },
                                          { 0 },
                                          { 0 },
                                          { NULL },
                                          0,
                                          GAME_SESSION_STREAM_INDEX_NONE,
                                          0,
                                          0 };

uint32 g_GAME_SESSION_STATE_PADDING = 0;

GameSessionResourceRequest g_GAME_SESSION_RESOURCE_REQUEST = { 0, 0, 0 };

char  g_GAME_SESSION_TRACK_NAME[GAME_SESSION_TRACK_NAME_BYTES];
int32 g_GAME_SESSION_RESOURCE_BUFFER_INDEX;

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

COMPILE_TIME_SIZE_ASSERT(game_session_state_size, sizeof(GameSessionState) == 0x98);
COMPILE_TIME_SIZE_ASSERT(game_session_state_object_size, sizeof(g_GAME_SESSION_STATE) == 0x98);
COMPILE_TIME_SIZE_ASSERT(game_session_request_queue_offset,
                         offsetof(GameSessionState, request_queue) == 0x10);
COMPILE_TIME_SIZE_ASSERT(game_session_request_write_index_offset,
                         offsetof(GameSessionState, request_write_index) == 0x50);
COMPILE_TIME_SIZE_ASSERT(game_session_request_read_index_offset,
                         offsetof(GameSessionState, request_read_index) == 0x54);
COMPILE_TIME_SIZE_ASSERT(game_session_loaded_packet_indices_offset,
                         offsetof(GameSessionState, loaded_packet_indices) == 0x74);
COMPILE_TIME_SIZE_ASSERT(game_session_resource_claim_flags_offset,
                         offsetof(GameSessionState, resource_buffer_claim_flags) == 0x80);
COMPILE_TIME_SIZE_ASSERT(game_session_resource_request_size,
                         sizeof(GameSessionResourceRequest) == 0x0C);

void
initialize_game_session_resources(void)
{
  int32 buffer_index;

  clear_claim_flag(&g_GAME_SESSION_RESOURCE_REQUEST.claimed);
  for(buffer_index = 0; buffer_index < GAME_SESSION_RESOURCE_BUFFER_COUNT; buffer_index += 1)
    {
      clear_claim_flag(&g_GAME_SESSION_STATE.resource_buffer_claim_flags[buffer_index]);
      g_GAME_SESSION_STATE.resource_buffers[buffer_index] =
        allocate_named_memory("RRSM_Buffer",
                              GAME_SESSION_RESOURCE_BUFFER_BYTES,
                              GAME_SESSION_RESOURCE_MEMORY_FLAGS);
    }
  g_GAME_SESSION_STATE.request_write_index = 0;
  g_GAME_SESSION_STATE.request_read_index  = 0;
}

void
release_game_session_resources(void)
{
  int32 buffer_index;

  for(buffer_index = 0; buffer_index < GAME_SESSION_RESOURCE_BUFFER_COUNT; buffer_index += 1)
    {
      if(g_GAME_SESSION_STATE.resource_buffers[buffer_index] != NULL)
        {
          free_memory(g_GAME_SESSION_STATE.resource_buffers[buffer_index]);
          g_GAME_SESSION_STATE.resource_buffers[buffer_index] = NULL;
        }
    }
  release_object_render_resources();
}

void
release_game_session_resources_for_shutdown(void)
{
  release_game_session_resources();
}

void
load_drive_art_packet(int32 packet_index_, void *buffer_)
{
  char    path[GAME_SESSION_PACKET_PATH_BYTES];
  Stream *stream;

  sprintf(path,
          GAME_SESSION_PACKET_PATH_FORMAT,
          g_GAME_DATA_PATH,
          G_TRACK_DATA_PATH,
          packet_index_);
  stream = OpenDiskStream(path, GAME_SESSION_DISK_STREAM_BUFFER_BYTES);
  ReadDiskStream(stream, (char *) buffer_, GAME_SESSION_RESOURCE_BUFFER_BYTES);
  CloseDiskStream(stream);
}

void *
load_game_session_resource_packet(int32 packet_index_, int32 buffer_index_)
{
  void *buffer;

  buffer = g_GAME_SESSION_STATE.resource_buffers[buffer_index_];
  load_drive_art_packet(packet_index_, buffer);
  return resolve_headered_file_offsets(buffer, 1);
}

void *
acquire_game_session_resource_packet(int32 buffer_index_, int32 packet_index_)
{
  int32 *claim;
  void **packets;
  void  *packet;

  claim = &g_GAME_SESSION_STATE.resource_buffer_claim_flags[buffer_index_];
  if(claim_flag(claim) != 1)
    return NULL;

  packets = (void **) g_GAME_SESSION_STATE.resource_buffers[buffer_index_];
  packet  = packets[packet_index_ + GAME_SESSION_PACKET_HEADER_WORDS];
  clear_claim_flag(claim);
  return packet;
}

void *
acquire_game_session_resource_packet_entry(int32 buffer_index_,
                                           int32 packet_index_,
                                           int32 entry_index_)
{
  int32  *claim;
  void ***packets;
  void   *entry;

  claim = &g_GAME_SESSION_STATE.resource_buffer_claim_flags[buffer_index_];
  if(claim_flag(claim) != 1)
    return NULL;

  packets = (void ***) g_GAME_SESSION_STATE.resource_buffers[buffer_index_];
  entry   = packets[packet_index_ + GAME_SESSION_PACKET_HEADER_WORDS][entry_index_];
  clear_claim_flag(claim);
  return entry;
}

void *
acquire_game_session_resource_packet_subentry(int32 buffer_index_,
                                              int32 packet_index_,
                                              int32 entry_index_,
                                              int32 subentry_index_)
{
  int32   *claim;
  void ****packets;
  void    *subentry;

  claim = &g_GAME_SESSION_STATE.resource_buffer_claim_flags[buffer_index_];
  if(claim_flag(claim) != 1)
    return NULL;

  packets = (void ****) g_GAME_SESSION_STATE.resource_buffers[buffer_index_];
  subentry =
    packets[packet_index_ + GAME_SESSION_PACKET_HEADER_WORDS][entry_index_][subentry_index_];
  clear_claim_flag(claim);
  return subentry;
}

void *
acquire_game_session_resource_packet_value(int32 buffer_index_,
                                           int32 packet_index_,
                                           int32 entry_index_,
                                           int32 subentry_index_,
                                           int32 value_index_)
{
  int32    *claim;
  void *****packets;
  void     *value;

  claim = &g_GAME_SESSION_STATE.resource_buffer_claim_flags[buffer_index_];
  if(claim_flag(claim) != 1)
    return NULL;

  packets = (void *****) g_GAME_SESSION_STATE.resource_buffers[buffer_index_];
  value   = packets[packet_index_ + GAME_SESSION_PACKET_HEADER_WORDS][entry_index_][subentry_index_]
                   [value_index_];
  clear_claim_flag(claim);
  return value;
}

void
process_game_session_resource_requests(void)
{
  int32 buffer_index;
  void *object_resources;

  if(g_GAME_SESSION_STATE.open_request_pending != 0)
    {
      initialize_road_data(g_GAME_SESSION_TRACK_NAME);
      g_GAME_SESSION_STATE.stream_operation_complete = 1;
      g_GAME_SESSION_STATE.open_request_pending      = 0;
    }
  if(g_GAME_SESSION_STATE.close_request_pending != 0)
    {
      shutdown_road_data();
      g_GAME_SESSION_STATE.close_request_pending     = 0;
      g_GAME_SESSION_STATE.stream_operation_complete = 0;
    }
  if(g_GAME_SESSION_STATE.stream_index >= 0)
    {
      update_track_stream_window(g_GAME_SESSION_STATE.stream_index);
      g_GAME_SESSION_STATE.stream_index = GAME_SESSION_STREAM_INDEX_NONE;
    }
  if(g_GAME_SESSION_STATE.resource_load_pending == 0)
    return;

  buffer_index = g_GAME_SESSION_RESOURCE_REQUEST.buffer_index;
  load_game_session_resource_packet(g_GAME_SESSION_RESOURCE_REQUEST.packet_index, buffer_index);
  clear_claim_flag(&g_GAME_SESSION_STATE.resource_buffer_claim_flags[buffer_index]);
  clear_claim_flag(&g_GAME_SESSION_RESOURCE_REQUEST.claimed);
  object_resources =
    acquire_game_session_resource_packet(buffer_index, GAME_SESSION_OBJECT_RESOURCE_PACKET_INDEX);
  initialize_object_render_resources(object_resources);
  g_GAME_SESSION_STATE.resource_load_pending = 0;
}

void
open_game_session_stream(const char *track_name_)
{
  g_GAME_SESSION_STATE.stream_operation_complete = 0;
  copy_string(g_GAME_SESSION_TRACK_NAME, track_name_);
  g_GAME_SESSION_STATE.open_request_pending = 1;
  SendSignal(g_GAME_SESSION_STATE.task_item, g_GAME_SESSION_STATE.signal_mask);
  while(g_GAME_SESSION_STATE.stream_operation_complete == 0)
    send_render_io_command(GAME_SESSION_RENDER_WAIT_COMMAND);
}

void
close_game_session_stream(void)
{
  g_GAME_SESSION_STATE.close_request_pending = 1;
  SendSignal(g_GAME_SESSION_STATE.task_item, g_GAME_SESSION_STATE.signal_mask);
  while(g_GAME_SESSION_STATE.stream_operation_complete != 0)
    delay_vbls(GAME_SESSION_WAIT_DELAY_FIELDS);
}

int32
submit_game_session_resource_request(int32 packet_index_, int32 buffer_index_)
{
  if(claim_flag(&g_GAME_SESSION_RESOURCE_REQUEST.claimed) != 1)
    return 1;

  claim_flag(&g_GAME_SESSION_STATE.resource_buffer_claim_flags[buffer_index_]);
  g_GAME_SESSION_RESOURCE_REQUEST.packet_index = packet_index_;
  g_GAME_SESSION_RESOURCE_REQUEST.buffer_index = buffer_index_;
  g_GAME_SESSION_STATE.resource_load_pending   = 1;
  ScavengeMem();
  SendSignal(g_GAME_SESSION_STATE.task_item, g_GAME_SESSION_STATE.signal_mask);
  ScavengeMem();
  g_GAME_SESSION_STATE.loaded_packet_indices[buffer_index_] = packet_index_;
  return 0;
}

void
process_next_queued_game_session_resource_request(void)
{
  GameSessionQueuedRequest *request;

  if(g_GAME_SESSION_STATE.request_read_index == g_GAME_SESSION_STATE.request_write_index)
    return;

  request = &g_GAME_SESSION_STATE.request_queue[g_GAME_SESSION_STATE.request_read_index];
  if(submit_game_session_resource_request(request->packet_index, request->buffer_index) != 0)
    return;

  g_GAME_SESSION_STATE.request_read_index += 1;
  if(g_GAME_SESSION_STATE.request_read_index >= GAME_SESSION_REQUEST_QUEUE_CAPACITY)
    g_GAME_SESSION_STATE.request_read_index = 0;
}

int32
queue_game_session_resource_request(int32 packet_index_, int32 buffer_index_)
{
  GameSessionQueuedRequest *request;

  request = &g_GAME_SESSION_STATE.request_queue[g_GAME_SESSION_STATE.request_write_index];
  request->packet_index = packet_index_;
  request->buffer_index = buffer_index_;
  g_GAME_SESSION_STATE.request_write_index += 1;
  if(g_GAME_SESSION_STATE.request_write_index >= GAME_SESSION_REQUEST_QUEUE_CAPACITY)
    g_GAME_SESSION_STATE.request_write_index = 0;
  if(g_GAME_SESSION_STATE.request_write_index == g_GAME_SESSION_STATE.request_read_index)
    exit(GAME_SESSION_QUEUE_FULL_EXIT_STATUS);

  return g_GAME_SESSION_STATE.request_read_index;
}

void
run_game_session_worker(void)
{
  uint32 received_signals;

  g_GAME_SESSION_STATE.signal_mask = AllocSignal(0);
  if(g_GAME_SESSION_STATE.signal_mask == 0)
    return;

  g_GAME_SESSION_STATE.shutdown_requested = 1;
  g_GAME_SESSION_STATE.start_complete     = 1;
  while(g_GAME_SESSION_STATE.shutdown_requested != 0)
    {
      received_signals = WaitSignal(g_GAME_SESSION_STATE.signal_mask);
      if(((received_signals & g_GAME_SESSION_STATE.signal_mask) != 0) &&
         (g_GAME_SESSION_STATE.shutdown_requested != 0))
        process_game_session_resource_requests();
    }
  ScavengeMem();
  FreeSignal(g_GAME_SESSION_STATE.signal_mask);
  ScavengeMem();
  g_GAME_SESSION_STATE.shutdown_complete = 1;
  exit(0);
}

int32
initialize_game_session(void)
{
  g_GAME_SESSION_STATE.start_complete = 0;
  initialize_game_session_resources();
  g_GAME_SESSION_STATE.task_item =
    create_task_with_allocated_stack(run_game_session_worker,
                                     GAME_SESSION_WORKER_STACK_BYTES,
                                     GAME_SESSION_WORKER_PRIORITY,
                                     GAME_SESSION_WORKER_NAME,
                                     &g_GAME_SESSION_STATE.worker_stack_allocation);
  g_GAME_SESSION_STATE.start_requested = 1;
  while(g_GAME_SESSION_STATE.start_complete == 0)
    delay_vbls(GAME_SESSION_START_DELAY_FIELDS);

  return g_GAME_SESSION_STATE.task_item != 0;
}

int32
finish_game_session(void)
{
  release_game_session_resources_for_shutdown();
  g_GAME_SESSION_STATE.shutdown_requested = 0;
  g_GAME_SESSION_STATE.shutdown_complete  = 0;
  while(g_GAME_SESSION_STATE.shutdown_complete == 0)
    {
      SendSignal(g_GAME_SESSION_STATE.task_item, g_GAME_SESSION_STATE.signal_mask);
      delay_vbls(GAME_SESSION_WAIT_DELAY_FIELDS);
    }

  return release_task_stack_allocation(&g_GAME_SESSION_STATE.worker_stack_allocation);
}
