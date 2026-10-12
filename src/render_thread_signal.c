#include "render_thread_signal.h"

#include "bitmap_clip.h"
#include "camera_io.h"
#include "distance_slice_window.h"
#include "frontend_transition.h"
#include "game_bootstrap.h"
#include "game_session_resources.h"
#include "horizon_render.h"
#include "horizon_resources.h"
#include "io_offset_request.h"
#include "memory.h"
#include "pause_game_shape.h"
#include "render_frame.h"
#include "road_data.h"
#include "road_render.h"
#include "simulation_snapshot.h"
#include "simulation_thread_signal.h"
#include "string_utilities.h"
#include "track_disk_stream.h"
#include "vbl_delay.h"
#include "vehicle_simulation_state.h"
#include "window_memory.h"

#include "graphics.h"
#include "mem.h"
#include "task.h"

#include "stddef.h"
#include "stdlib.h"

typedef char SizeAssertRenderThreadShutdownCompleteOffset
  [(offsetof(RenderThreadState, shutdown_complete) == 0x0C) ? 1 : -1];
typedef char SizeAssertRenderThreadPipelineInitializedOffset
  [(offsetof(RenderThreadState, pipeline_initialized) == 0x04) ? 1 : -1];
typedef char SizeAssertRenderThreadRunRequestedOffset
  [(offsetof(RenderThreadState, run_requested) == 0x30) ? 1 : -1];
typedef char SizeAssertRenderThreadStackAllocationOffset
  [(offsetof(RenderThreadState, stack_allocation) == 0x34) ? 1 : -1];
typedef char SizeAssertRenderThreadFrameCompleteOffset
  [(offsetof(RenderThreadState, frame_complete) == 0x44) ? 1 : -1];
typedef char
  SizeAssertRenderThreadRenderPhaseOffset[(offsetof(RenderThreadState, render_phase) == 0x48) ? 1 :
                                                                                                -1];
typedef char SizeAssertRenderThreadResourcePacketZeroOffset
  [(offsetof(RenderThreadState, road_resource_packet_zero) == 0x70) ? 1 : -1];
typedef char SizeAssertRenderThreadResourcePacketOneOffset
  [(offsetof(RenderThreadState, road_resource_packet_one) == 0x74) ? 1 : -1];
typedef char
  SizeAssertRenderThreadResourceStorageSize[(sizeof(RenderThreadResourceStorage) == 0x80) ? 1 : -1];
typedef char SizeAssertRenderThreadRoadObjectCelOffsetsOffset
  [(offsetof(RenderThreadResourceStorage, road_object_cel_frame_offsets) == 0x08) ? 1 : -1];

#define RENDER_THREAD_STACK_BYTES 4000
#define RENDER_THREAD_PRIORITY 2
#define RENDER_THREAD_WAIT_DELAY_FIELDS 2
#define RENDER_THREAD_NAME "Render"
#define RENDER_WORK_WINDOW_WIDTH 64
#define RENDER_WORK_WINDOW_HEIGHT 28
#define RENDER_WORK_WINDOW_MEMORY_FLAGS 0
#define RENDER_PIPELINE_CCB_PRESERVED_FLAGS 0x9FFFFFFF
#define TRACK_RENDER_PROFILE_COUNT 4
#define TRACK_RENDER_PROFILE_WORD_COUNT 8
#define TRACK_RENDER_DEFAULT_PROFILE 3
#define TRACK_RENDER_PROFILE_ALPINE 0
#define TRACK_RENDER_PROFILE_CITY 1
#define TRACK_RENDER_PROFILE_COASTAL 2
#define TRACK_RENDER_PREFIX_BYTES 3
#define TRACK_RENDER_PREFIX_TERMINATOR_INDEX 2
#define TRACK_RENDER_LOWERCASE_MASK 0x20
#define TRACK_RENDER_ALPINE_PREFIX "al"
#define TRACK_RENDER_CITY_PREFIX "cl"
#define TRACK_RENDER_COASTAL_PREFIX "cy"
#define TRACK_RENDER_VALUE_ALPINE 25
#define TRACK_RENDER_VALUE_CITY 19
#define TRACK_RENDER_VALUE_COASTAL 28
#define TRACK_RENDER_VALUE_DEFAULT 23
#define TRACK_RENDER_OPTION_COUNT_ALPINE 14
#define TRACK_RENDER_OPTION_COUNT_STANDARD 8
#define TRACK_RENDER_LIMIT_LARGE 16
#define TRACK_RENDER_LIMIT_SMALL 12
#define TRACK_PROFILE_WORD_00000000 0x00000000
#define TRACK_PROFILE_WORD_00000101 0x00000101
#define TRACK_PROFILE_WORD_01010101 0x01010101
#define TRACK_PROFILE_WORD_01010202 0x01010202
#define TRACK_PROFILE_WORD_02020202 0x02020202
#define TRACK_PROFILE_WORD_02020303 0x02020303
#define TRACK_PROFILE_WORD_03030303 0x03030303
#define RENDER_PIPELINE_FIRST_RESOURCE_PACKET_INDEX 0
#define RENDER_PIPELINE_SECOND_RESOURCE_PACKET_INDEX 1
#define RENDER_PIPELINE_SIMULATION_MODE 7
#define RENDER_PIPELINE_SECONDARY_ROAD_SECTION_COUNT 3
#define RENDER_PIPELINE_DISTANCE_SLICE_INDEX 11

RenderThreadState        g_RENDER_THREAD_STATE;
extern RenderThreadState g_RENDER_THREAD_STATE_FLAGS;
extern RenderThreadState g_RENDER_THREAD_STATE_COORDINATION;
extern int32             g_RENDER_THREAD_STATE_FRAME_COMPLETE;
extern RenderThreadState g_RENDER_THREAD_STATE_VRAM_REQUEST;
extern RenderThreadState g_RENDER_THREAD_STATE_PROFILE_SELECTION;
extern RenderThreadState g_RENDER_THREAD_STATE_LIMIT_STORAGE;
extern RenderThreadState g_RENDER_THREAD_STATE_ACTIVE_LIMIT;
extern RenderThreadState g_RENDER_THREAD_STATE_TRAILING_FLAG;
volatile uint8           g_RENDER_THREAD_START_COMPLETE;
uint32                  *g_RENDER_PIPELINE_CCB_FLAGS;
int32 g_TRACK_RENDER_PROFILE_VALUES[TRACK_RENDER_PROFILE_COUNT] = { TRACK_RENDER_VALUE_ALPINE,
                                                                    TRACK_RENDER_VALUE_CITY,
                                                                    TRACK_RENDER_VALUE_COASTAL,
                                                                    TRACK_RENDER_VALUE_DEFAULT };
int32 g_TRACK_RENDER_OPTION_COUNTS[TRACK_RENDER_PROFILE_COUNT]  = {
  TRACK_RENDER_OPTION_COUNT_ALPINE,
  TRACK_RENDER_OPTION_COUNT_STANDARD,
  TRACK_RENDER_OPTION_COUNT_STANDARD,
  TRACK_RENDER_OPTION_COUNT_STANDARD
};
uint32 g_TRACK_RENDER_PROFILES[TRACK_RENDER_PROFILE_COUNT][TRACK_RENDER_PROFILE_WORD_COUNT] = {
  { TRACK_PROFILE_WORD_00000000,
    TRACK_PROFILE_WORD_00000000,
    TRACK_PROFILE_WORD_00000000,
    TRACK_PROFILE_WORD_00000101,
    TRACK_PROFILE_WORD_01010202,
    TRACK_PROFILE_WORD_02020303,
    TRACK_PROFILE_WORD_03030303,
    TRACK_PROFILE_WORD_03030303 },
  { TRACK_PROFILE_WORD_00000000,
    TRACK_PROFILE_WORD_00000000,
    TRACK_PROFILE_WORD_01010101,
    TRACK_PROFILE_WORD_02020202,
    TRACK_PROFILE_WORD_03030303,
    TRACK_PROFILE_WORD_03030303,
    TRACK_PROFILE_WORD_03030303,
    TRACK_PROFILE_WORD_03030303 },
  { TRACK_PROFILE_WORD_00000000,
    TRACK_PROFILE_WORD_00000000,
    TRACK_PROFILE_WORD_01010101,
    TRACK_PROFILE_WORD_02020202,
    TRACK_PROFILE_WORD_03030303,
    TRACK_PROFILE_WORD_03030303,
    TRACK_PROFILE_WORD_03030303,
    TRACK_PROFILE_WORD_03030303 },
  { TRACK_PROFILE_WORD_00000000,
    TRACK_PROFILE_WORD_00000000,
    TRACK_PROFILE_WORD_01010101,
    TRACK_PROFILE_WORD_02020202,
    TRACK_PROFILE_WORD_03030303,
    TRACK_PROFILE_WORD_03030303,
    TRACK_PROFILE_WORD_03030303,
    TRACK_PROFILE_WORD_03030303 }
};
int32 g_TRACK_RENDER_LIMITS[TRACK_RENDER_PROFILE_COUNT] = { TRACK_RENDER_LIMIT_LARGE,
                                                            TRACK_RENDER_LIMIT_LARGE,
                                                            TRACK_RENDER_LIMIT_SMALL,
                                                            TRACK_RENDER_LIMIT_SMALL };

RenderThreadResourceStorage g_RENDER_THREAD_STATE_RESOURCE_STORAGE = {
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 3, 3, 3, 3, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 }
};

void
initialize_render_pipeline(void)
{
  g_FRONTEND_RENDER_STATE.menu.render_reset_pending = 1;
  g_FRONTEND_RENDER_STATE.menu.render_complete      = 0;
  g_FRONTEND_RENDER_STATE.menu.backdrop_enabled     = 0;
  g_FRONTEND_RENDER_STATE.projection.source =
    (SimulationProjectionSource *) &g_SIMULATION_RENDER_STATE;
  initialize_camera_io();
  ScavengeMem();
  open_game_session_stream(g_ROAD_RUNTIME_STATE.track_name);
  ScavengeMem();

  g_RENDER_THREAD_STATE.road_resource_packet_zero =
    acquire_game_session_resource_packet(g_GAME_SESSION_RESOURCE_BUFFER_INDEX,
                                         RENDER_PIPELINE_FIRST_RESOURCE_PACKET_INDEX);
  while(g_RENDER_THREAD_STATE.road_resource_packet_zero == NULL)
    {
      g_RENDER_THREAD_STATE.road_resource_packet_zero =
        acquire_game_session_resource_packet(g_GAME_SESSION_RESOURCE_BUFFER_INDEX,
                                             RENDER_PIPELINE_FIRST_RESOURCE_PACKET_INDEX);
    }
  g_RENDER_THREAD_STATE.road_resource_packet_one =
    acquire_game_session_resource_packet(g_GAME_SESSION_RESOURCE_BUFFER_INDEX,
                                         RENDER_PIPELINE_SECOND_RESOURCE_PACKET_INDEX);
  while(g_RENDER_THREAD_STATE.road_resource_packet_one == NULL)
    {
      g_RENDER_THREAD_STATE.road_resource_packet_one =
        acquire_game_session_resource_packet(g_GAME_SESSION_RESOURCE_BUFFER_INDEX,
                                             RENDER_PIPELINE_SECOND_RESOURCE_PACKET_INDEX);
    }

  if(g_TRACK_DISK_STREAM_STATE.active != 0)
    {
      initialize_simulation_projection_tables();
      if((g_FRONTEND_RENDER_STATE.menu.reset != 0) &&
         (g_FRONTEND_RENDER_STATE.menu.reset < SIMULATION_MODE_CONFIGURATION_COUNT))
        {
          initialize_simulation_state_from_source(
            &g_SIMULATION_RENDER_STATE,
            &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_PLAYER]->simulation_source,
            (int32) g_SIMULATION_RENDER_STATE.mode);
          g_SIMULATION_RENDER_STATE.requested_mode =
            (uint32) g_FRONTEND_RENDER_STATE.menu.saved_simulation_mode;
        }
      else
        {
          initialize_simulation_state_from_source(
            &g_SIMULATION_RENDER_STATE,
            &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_PLAYER]->simulation_source,
            RENDER_PIPELINE_SIMULATION_MODE);
        }
      g_SECONDARY_ROAD_PROJECTION_SECTION_COUNT = RENDER_PIPELINE_SECONDARY_ROAD_SECTION_COUNT;
      ScavengeMem();
      initialize_horizon_configuration(g_STARTUP_ARGUMENT);
      ScavengeMem();
      initialize_render_work_window();
      initialize_distance_slice_window();
      configure_track_render_profile();
      load_pause_game_shape();
      g_RENDER_THREAD_STATE.pipeline_initialized = 1;
      return;
    }

  while(1)
    {
    }
}

void
initialize_render_work_window(void)
{
  Window *window;

  window                                 = allocate_window(RENDER_WORK_WINDOW_WIDTH,
                                                           RENDER_WORK_WINDOW_HEIGHT,
                                                           RENDER_WORK_WINDOW_MEMORY_FLAGS);
  g_RENDER_THREAD_STATE.allocated_memory = window;
  SetClipWidth(window->bitmap_item, RENDER_WORK_WINDOW_WIDTH);
  SetClipHeight(window->bitmap_item, RENDER_WORK_WINDOW_HEIGHT);
  *g_RENDER_PIPELINE_CCB_FLAGS &= RENDER_PIPELINE_CCB_PRESERVED_FLAGS;
}

void
configure_track_render_profile(void)
{
  char  track_prefix[TRACK_RENDER_PREFIX_BYTES];
  int32 profile_index;

  copy_string(track_prefix, g_ROAD_RUNTIME_STATE.track_name);
  track_prefix[0] |= TRACK_RENDER_LOWERCASE_MASK;
  track_prefix[1] |= TRACK_RENDER_LOWERCASE_MASK;
  track_prefix[TRACK_RENDER_PREFIX_TERMINATOR_INDEX] = 0;

  profile_index = TRACK_RENDER_PROFILE_ALPINE;
  if(compare_strings((const uint8 *) track_prefix, (const uint8 *) TRACK_RENDER_ALPINE_PREFIX) != 0)
    {
      if(compare_strings((const uint8 *) track_prefix, (const uint8 *) TRACK_RENDER_CITY_PREFIX) ==
         0)
        profile_index = TRACK_RENDER_PROFILE_CITY;
      else if(compare_strings((const uint8 *) track_prefix,
                              (const uint8 *) TRACK_RENDER_COASTAL_PREFIX) == 0)
        profile_index = TRACK_RENDER_PROFILE_COASTAL;
      else
        profile_index = TRACK_RENDER_DEFAULT_PROFILE;
    }

  g_RENDER_THREAD_STATE.track_render_profile_value = g_TRACK_RENDER_PROFILE_VALUES[profile_index];
  g_RENDER_THREAD_STATE.track_render_limit         = g_TRACK_RENDER_LIMITS[profile_index];
  g_RENDER_THREAD_STATE.track_render_profile       = g_TRACK_RENDER_PROFILES[profile_index];
  g_RENDER_THREAD_STATE.track_render_option_count  = g_TRACK_RENDER_OPTION_COUNTS[profile_index];
}

void
signal_render_thread(void)
{
  SendSignal(g_RENDER_THREAD_STATE.thread_item, g_RENDER_THREAD_STATE.signal_mask);
}

Err
send_render_io_command(int32 command_)
{
  return send_io_offset_request(g_RENDER_THREAD_STATE.io_request, command_);
}

void
free_render_thread_memory(void)
{
  free_memory(g_RENDER_THREAD_STATE.allocated_memory);
}

void
run_render_thread(void)
{
  uint32 received_signals;

  ScavengeMem();
  if(g_FRONTEND_RENDER_STATE.menu.active == 0)
    (void) submit_game_session_resource_request(0, 0);
  ScavengeMem();

  g_RENDER_THREAD_STATE.signal_mask = AllocSignal(0);
  if(g_RENDER_THREAD_STATE.signal_mask == 0)
    return;

  g_RENDER_THREAD_STATE.io_request = GetVBLIOReq();
  ScavengeMem();
  open_render_thread_resources();
  ScavengeMem();

  g_RENDER_THREAD_STATE.render_phase         = 0;
  g_RENDER_THREAD_STATE.frame_complete       = 1;
  g_RENDER_THREAD_STATE.distance_slice_index = RENDER_PIPELINE_DISTANCE_SLICE_INDEX;
  initialize_render_pipeline();
  g_RENDER_THREAD_STATE.run_requested = 1;
  g_RENDER_THREAD_START_COMPLETE      = 1;

  do
    {
      received_signals = WaitSignal(g_RENDER_THREAD_STATE.signal_mask);
      if((received_signals & g_RENDER_THREAD_STATE.signal_mask) != 0)
        render_next_frame();
    }
  while(g_RENDER_THREAD_STATE.run_requested != 0);

  DeleteItem(g_RENDER_THREAD_STATE.io_request);
  ScavengeMem();
  close_render_thread_resources();
  ScavengeMem();
  g_RENDER_THREAD_STATE.shutdown_complete = 1;
  exit(0);
}

int32
start_render_thread(void)
{
  g_RENDER_THREAD_START_COMPLETE = 0;
  g_RENDER_THREAD_STATE.thread_item =
    create_task_with_allocated_stack(run_render_thread,
                                     RENDER_THREAD_STACK_BYTES,
                                     RENDER_THREAD_PRIORITY,
                                     RENDER_THREAD_NAME,
                                     &g_RENDER_THREAD_STATE.stack_allocation);
  while(g_RENDER_THREAD_START_COMPLETE == 0)
    delay_vbls(RENDER_THREAD_WAIT_DELAY_FIELDS);

  return g_RENDER_THREAD_STATE.thread_item != 0;
}

void
stop_render_thread(void)
{
  g_RENDER_THREAD_STATE.run_requested     = 0;
  g_RENDER_THREAD_STATE.shutdown_complete = 0;
  do
    {
      signal_render_thread();
      delay_vbls(RENDER_THREAD_WAIT_DELAY_FIELDS);
    }
  while(g_RENDER_THREAD_STATE.shutdown_complete == 0);

  unload_pause_game_shape();
  free_horizon_ring_resources();
  free_render_thread_memory();
  free_distance_slice_window();
  close_game_session_stream();
  (void) release_task_stack_allocation(&g_RENDER_THREAD_STATE.stack_allocation);
  g_RENDER_THREAD_STATE.pipeline_initialized = 0;
  g_RENDER_THREAD_STATE.frame_rendered       = 0;
}
