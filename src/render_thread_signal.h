#pragma once

#include "task_stack_allocation.h"

#include "kernel.h"
#include "types.h"

struct Window;

typedef struct RenderThreadResourceStorage
{
  uint8 reserved_00[8];
  uint8 road_object_cel_frame_offsets[120];
} RenderThreadResourceStorage;

typedef struct RenderThreadState
{
  int32               road_scene_render_state;
  uint8               pipeline_initialized;
  uint8               reserved_05[0x03];
  uint8               frame_rendered;
  uint8               reserved_09[0x03];
  volatile uint8      shutdown_complete;
  uint8               reserved_0_d[0x03];
  Item                io_request;
  int32               distance_slice_index;
  uint8               reserved_18[8];
  uint32              render_flags;
  struct Window      *distance_slice_window;
  Item                thread_item;
  uint32              signal_mask;
  volatile uint8      run_requested;
  uint8               reserved_31[0x03];
  TaskStackAllocation stack_allocation;
  uint8               reserved_40[0x04];
  int32               frame_complete;
  int32               render_phase;
  uint8               reserved_4_c[0x24];
  void               *road_resource_packet_zero;
  void               *road_resource_packet_one;
  Item                vram_io_request;
  volatile int32      simulation_update_suspended;
  uint8               reserved_80[0x04];
  uint8               reserved_84[0x04];
  uint8               reserved_88[0x10];
  int32               track_render_profile_value;
  void               *allocated_memory;
  uint8               reserved_a0[0x17C];
  const uint32       *track_render_profile;
  int32               track_render_option_count;
  uint8               reserved_224[0xB0];
  int32               track_render_limit;
  int32               trailing_flag;
} RenderThreadState;

extern RenderThreadState           g_RENDER_THREAD_STATE;
extern RenderThreadResourceStorage g_RENDER_THREAD_STATE_RESOURCE_STORAGE;
extern volatile uint8              g_RENDER_THREAD_START_COMPLETE;
extern uint32                     *g_RENDER_PIPELINE_CCB_FLAGS;
extern int32                       g_TRACK_RENDER_PROFILE_VALUES[4];
extern int32                       g_TRACK_RENDER_OPTION_COUNTS[4];
extern uint32                      g_TRACK_RENDER_PROFILES[4][8];
extern int32                       g_TRACK_RENDER_LIMITS[4];

void
signal_render_thread(void);
void
initialize_render_pipeline(void);
void
initialize_render_work_window(void);
void
configure_track_render_profile(void);
Err
send_render_io_command(int32 command_);
void
free_render_thread_memory(void);
void
run_render_thread(void);
int32
start_render_thread(void);
void
stop_render_thread(void);
