#pragma once

#include "task_stack_allocation.h"

#include "types.h"

#define GAME_SESSION_RESOURCE_BUFFER_COUNT 1
#define GAME_SESSION_REQUEST_QUEUE_CAPACITY 8
#define GAME_SESSION_LOADED_PACKET_INDEX_COUNT 3
#define GAME_SESSION_TRACK_NAME_BYTES 104

typedef struct GameSessionQueuedRequest
{
  int32 packet_index;
  int32 buffer_index;
} GameSessionQueuedRequest;

typedef struct GameSessionState
{
  uint8                    reserved_00[0x04];
  Item                     task_item;
  uint32                   signal_mask;
  uint8                    shutdown_requested;
  uint8                    reserved_0_d[0x03];
  GameSessionQueuedRequest request_queue[GAME_SESSION_REQUEST_QUEUE_CAPACITY];
  int32                    request_write_index;
  int32                    request_read_index;
  uint8                    start_requested;
  uint8                    reserved_59[0x03];
  volatile uint8           start_complete;
  uint8                    reserved_5_d[0x03];
  TaskStackAllocation      worker_stack_allocation;
  int32                    stream_operation_complete;
  volatile uint8           shutdown_complete;
  uint8                    reserved_71[0x03];
  int32                    loaded_packet_indices[GAME_SESSION_LOADED_PACKET_INDEX_COUNT];
  int32                    resource_buffer_claim_flags[GAME_SESSION_RESOURCE_BUFFER_COUNT];
  void                    *resource_buffers[GAME_SESSION_RESOURCE_BUFFER_COUNT];
  int32                    resource_load_pending;
  int32                    stream_index;
  int32                    open_request_pending;
  int32                    close_request_pending;
} GameSessionState;

typedef struct GameSessionResourceRequest
{
  int32 claimed;
  int32 packet_index;
  int32 buffer_index;
} GameSessionResourceRequest;

extern GameSessionState           g_GAME_SESSION_STATE;
extern uint32                     g_GAME_SESSION_STATE_PADDING;
extern GameSessionResourceRequest g_GAME_SESSION_RESOURCE_REQUEST;
extern char                       g_GAME_SESSION_TRACK_NAME[GAME_SESSION_TRACK_NAME_BYTES];
extern int32                      g_GAME_SESSION_RESOURCE_BUFFER_INDEX;

void
initialize_game_session_resources(void);
void
release_game_session_resources(void);
void
release_game_session_resources_for_shutdown(void);
void
load_drive_art_packet(int32 packet_index_, void *buffer_);
void *
load_game_session_resource_packet(int32 packet_index_, int32 buffer_index_);
void *
acquire_game_session_resource_packet(int32 buffer_index_, int32 packet_index_);
void *
acquire_game_session_resource_packet_entry(int32 buffer_index_,
                                           int32 packet_index_,
                                           int32 entry_index_);
void *
acquire_game_session_resource_packet_subentry(int32 buffer_index_,
                                              int32 packet_index_,
                                              int32 entry_index_,
                                              int32 subentry_index_);
void *
acquire_game_session_resource_packet_value(int32 buffer_index_,
                                           int32 packet_index_,
                                           int32 entry_index_,
                                           int32 subentry_index_,
                                           int32 value_index_);
void
process_game_session_resource_requests(void);
void
open_game_session_stream(const char *track_name_);
void
close_game_session_stream(void);
int32
submit_game_session_resource_request(int32 packet_index_, int32 buffer_index_);
void
process_next_queued_game_session_resource_request(void);
int32
queue_game_session_resource_request(int32 packet_index_, int32 buffer_index_);
void
run_game_session_worker(void);
int32
initialize_game_session(void);
int32
finish_game_session(void);
