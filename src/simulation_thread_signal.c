#include "simulation_thread_signal.h"

#include "bitmap_clip.h"
#include "control_input_poll_state.h"
#include "frontend_transition.h"
#include "game_audio_system.h"
#include "game_bootstrap.h"
#include "game_loop.h"
#include "game_resource_cleanup.h"
#include "memory.h"
#include "pause_game_display.h"
#include "race_state.h"
#include "render_thread_signal.h"
#include "signal_status.h"
#include "simulation_common_art.h"
#include "simulation_resource_cleanup.h"
#include "simulation_snapshot.h"
#include "tire_smoke.h"
#include "track_disk_stream.h"
#include "vbl_delay.h"
#include "vehicle_drive_initialization.h"
#include "vehicle_drive_statistics.h"
#include "vehicle_render_resource_selection.h"
#include "vehicle_selection_state.h"

#include "graphics.h"
#include "mem.h"
#include "operamath.h"
#include "task.h"

#include "stddef.h"
#include "stdlib.h"

#define SIMULATION_THREAD_STACK_BYTES 7000
#define SIMULATION_THREAD_PRIORITY 10
#define SIMULATION_THREAD_START_DELAY_FIELDS 2
#define SIMULATION_THREAD_STOP_DELAY_FIELDS 1
#define DORMANT_REQUEST_DELAY_FIELDS 2
#define SIMULATION_THREAD_NAME "Sim"
#define SIMULATION_THREAD_STARTUP_FRAME_COUNT 10
#define SIMULATION_BITMAP_CE_CONTROL_INITIAL 0xE4500000
#define SIMULATION_BITMAP_CE_CONTROL_RUNNING 0x24500000
#define SIMULATION_BITMAP_CE_CONTROL_MASK 0xFDF00000
#define SIMULATION_PAUSE_COORDINATE_OFFSET 180
#define SIMULATION_PAUSE_COORDINATE_SCALE 4
#define SIMULATION_FRONTEND_MODE_TIMED 2
#define SIMULATION_FRONTEND_MODE_BLOCKED 3
#define SIMULATION_UPDATE_MODE_DIRECT 0
#define SIMULATION_UPDATE_MODE_DIRECT_ALT 4
#define SIMULATION_UPDATE_MODE_LIMIT 4
#define SIMULATION_FRONTEND_EXIT_COMPLETE 4
#define SIMULATION_JOYSTICK_PRESET_INDEX 5
#define SIMULATION_START_DELAY_FIELDS 30
#define SIMULATION_START_SCALE 128
#define SIMULATION_RENDER_CONTEXT_OFFSET 181
#define SIMULATION_FRAME_TIMER_SENTINEL (-90)
#define SIMULATION_SOURCE_MODE_STATE_OFFSET 0x0C
#define SIMULATION_RENDER_FLAG_PRIMARY 1
#define SIMULATION_RENDER_FLAG_PERIOD_MASK 3
#define SIMULATION_RENDER_FLAG_PERIODIC 4
#define SIMULATION_RENDER_FLAGS_STARTUP 5
#define SIMULATION_REQUEST_PENDING 1
#define VEHICLE_RENDER_SECONDARY_PROFILE_OFFSET 4

typedef struct VehicleRenderContext
{
  int32 coordinate;
  int32 enabled;
  int32 reserved_08;
  int32 reserved_0c;
  int32 reserved_10;
  int32 reserved_14;
  int32 reserved_18;
  int32 primary_scale;
  int32 secondary_scale;
  int32 reserved_24;
} VehicleRenderContext;

#define G_VEHICLE_RENDER_CONTEXT (*(VehicleRenderContext *) &g_GAME_RUNTIME_STATE)

extern uint8 g_FRONTEND_ACTIVE;
extern int32 g_SIMULATION_TIMEOUT_EXPIRED;

SimulationThreadSignalState g_SIMULATION_THREAD_SIGNAL_STATE;
int32                       g_ACTIVE_PLAYER_INDEX = -1;
int32                       g_SIMULATION_UPDATE_REPEAT_COUNT;
SimulationAudioRequestFlag  g_GAME_AUDIO_SHUTDOWN_REQUESTED;
SimulationAudioRequestFlag  g_GAME_AUDIO_INITIALIZE_REQUESTED;
volatile uint8              g_DORMANT_SIMULATION_REQUEST;
uint8                       g_DORMANT_SIMULATION_REQUEST_PADDING[3];
uint8                       g_SIMULATION_TRANSITION_ACTIVE;
int32                       g_SIMULATION_UPDATE_COUNTDOWN;
int32                       g_SIMULATION_PENDING_VALUE;
int32                       g_PAUSE_DISPLAY_PROFILE_VALUES[8][3] = {
  { 0x036F, 0x057D, 0x03CA }, { 0x03FE, 0x03CF, 0x03C0 }, { 0x0384, 0x0378, 0x043D },
  { 0x0BB8, 0x0BB8, 0x0BB8 }, { 0x16A8, 0x2D5C, 0x2A67 }, { 0x192D, 0x1FBD, 0x23D2 },
  { 0x13AB, 0x1874, 0x2443 }, { 0x03E8, 0x03E8, 0x03E8 }
};
int32 g_VEHICLE_RENDER_TUNING_SCALES[VEHICLE_RENDER_TUNING_COUNT][VEHICLE_RENDER_TYPE_COUNT] = {
  { 0xC28F, 0xD999, 0xC28F, 0xCA3D, 0xD999, 0xCA3D, 0xC000, 0xBD70 },
  { 0xC28F, 0xD999, 0xC28F, 0xCA3D, 0xD999, 0xCA3D, 0xC000, 0xBD70 },
  { 0xE147, 0xF851, 0xE147, 0xE8F5, 0xF851, 0xE8F5, 0xDEB8, 0xDC28 }
};
typedef char
  SizeAssertPauseDisplayProfileValues[(sizeof(g_PAUSE_DISPLAY_PROFILE_VALUES) == 0x60) ? 1 : -1];
typedef char
  SizeAssertVehicleRenderTuningScales[(sizeof(g_VEHICLE_RENDER_TUNING_SCALES) == 0x60) ? 1 : -1];
typedef char SizeAssertVehicleRenderContext[(sizeof(VehicleRenderContext) == 0x28) ? 1 : -1];
extern void
initialize_vehicle_runtime_objects(void);
extern void
update_vehicle_runtime_objects(void);

typedef char
  SizeAssertSimulationThreadStateSize[(sizeof(SimulationThreadSignalState) == 0xC8) ? 1 : -1];
typedef char SizeAssertSimulationAudioRequestFlag
  [(sizeof(SimulationAudioRequestFlag) == 4) ? 1 : -1];
typedef char SizeAssertSimulationThreadStackAllocationOffset
  [(offsetof(SimulationThreadSignalState, stack_allocation) == 0x64) ? 1 : -1];
typedef char SizeAssertSimulationThreadItemOffset
  [(offsetof(SimulationThreadSignalState, thread_item) == 0x70) ? 1 : -1];
typedef char SizeAssertSimulationThreadSignalOffset
  [(offsetof(SimulationThreadSignalState, signal_mask) == 0x74) ? 1 : -1];
typedef char SizeAssertSimulationThreadRunRequestedOffset
  [(offsetof(SimulationThreadSignalState, run_requested) == 0x78) ? 1 : -1];
typedef char SizeAssertSimulationThreadStartCompleteOffset
  [(offsetof(SimulationThreadSignalState, start_complete) == 0x7C) ? 1 : -1];
typedef char SizeAssertSimulationThreadShutdownCompleteOffset
  [(offsetof(SimulationThreadSignalState, shutdown_complete) == 0x88) ? 1 : -1];
typedef char SizeAssertSimulationThreadFrameCounterOffset
  [(offsetof(SimulationThreadSignalState, frame_counter) == 0x98) ? 1 : -1];
typedef char SizeAssertSimulationThreadRuntimeUpdateBlockedOffset
  [(offsetof(SimulationThreadSignalState, runtime_update_blocked) == 0x9C) ? 1 : -1];
typedef char SizeAssertSimulationThreadRenderFlagsOffset
  [(offsetof(SimulationThreadSignalState, render_flags) == 0xA0) ? 1 : -1];

/* Initializes the worker-owned flags before acquiring its Portfolio signal. */
static void
_initialize_simulation_thread_state(void);

void
request_game_audio_shutdown(void)
{
  g_GAME_AUDIO_SHUTDOWN_REQUESTED.requested = SIMULATION_REQUEST_PENDING;
  while(g_GAME_AUDIO_SHUTDOWN_REQUESTED.requested == SIMULATION_REQUEST_PENDING)
    {
      signal_simulation_thread();
      delay_vbls(1);
    }
}

void
request_game_audio_initialization(void)
{
  g_GAME_AUDIO_INITIALIZE_REQUESTED.requested = SIMULATION_REQUEST_PENDING;
  while(g_GAME_AUDIO_INITIALIZE_REQUESTED.requested == SIMULATION_REQUEST_PENDING)
    {
      signal_simulation_thread();
      delay_vbls(1);
    }
}

/*
 * Preserves a dormant request path. The shipped worker has no matching
 * consumer, and this routine has no callers in the executable.
 */
void
request_dormant_simulation_operation(void)
{
  g_DORMANT_SIMULATION_REQUEST = SIMULATION_REQUEST_PENDING;
  while(g_DORMANT_SIMULATION_REQUEST == SIMULATION_REQUEST_PENDING)
    {
      signal_simulation_thread();
      delay_vbls(DORMANT_REQUEST_DELAY_FIELDS);
    }
}

void
configure_vehicle_render_context(int32 coordinate_)
{
  VehicleRenderContext *context;
  int32                 secondary_scale;

  context                = &G_VEHICLE_RENDER_CONTEXT;
  context->coordinate    = coordinate_;
  context->enabled       = 1;
  context->reserved_08   = 0;
  context->reserved_0c   = 0;
  context->reserved_10   = 0;
  context->reserved_18   = 0;
  context->reserved_14   = 0;
  context->primary_scale = g_PAUSE_DISPLAY_PROFILE_VALUES[g_TRACK_INDEX][g_EVENT_INDEX];
  secondary_scale =
    g_PAUSE_DISPLAY_PROFILE_VALUES[g_TRACK_INDEX + VEHICLE_RENDER_SECONDARY_PROFILE_OFFSET]
                                  [g_EVENT_INDEX];
  context->secondary_scale = DivSF16(
    secondary_scale,
    g_VEHICLE_RENDER_TUNING_SCALES[g_STARTUP_CONFIGURATION.skill_level][g_PLAYER_VEHICLE_INDEX]);
  context->reserved_24 = 0;
}

/* Applies one CE control value to both display bitmaps. */
static void
_configure_simulation_bitmap_controls(int32 control_);
/* Waits until the frontend has published the initial simulation state. */
static void
_wait_for_simulation_start(void);
/* Dispatches audio lifecycle requests or one simulation update. */
static void
_process_simulation_signal(void);

void
signal_simulation_thread(void)
{
  SendSignal(g_SIMULATION_THREAD_SIGNAL_STATE.thread_item,
             g_SIMULATION_THREAD_SIGNAL_STATE.signal_mask);
}

void
free_simulation_common_art(void)
{
  if(g_SIMULATION_COMMON_ART != NULL)
    free_memory(g_SIMULATION_COMMON_ART);
}

void
wait_for_simulation_thread_signal(void)
{
  (void) WaitSignal(g_SIMULATION_THREAD_SIGNAL_STATE.signal_mask);
}

void
start_game_rendering(void)
{
  g_RACE_ACTIVE = 1;
}

void
stop_game_rendering(void)
{
  g_RACE_ACTIVE = 0;
}

void
update_simulation_frame(void)
{
  int32 phase;
  int32 render_coordinate;

  while(g_RENDER_THREAD_STATE.pipeline_initialized == 0)
    {
      if(g_RENDER_THREAD_STATE.frame_complete != 0)
        g_RENDER_THREAD_STATE.frame_complete += 1;
      signal_render_thread();
      wait_for_simulation_thread_signal();
    }

  if(g_SIMULATION_THREAD_SIGNAL_STATE.frame_initialized != 0)
    {
      if(g_SIMULATION_TRANSITION_ACTIVE == 0)
        {
          g_SIMULATION_THREAD_SIGNAL_STATE.render_flags = 0;
          if((g_SIMULATION_THREAD_SIGNAL_STATE.frame_counter & 1) == 0)
            g_SIMULATION_THREAD_SIGNAL_STATE.render_flags = SIMULATION_RENDER_FLAG_PRIMARY;
          if((g_SIMULATION_THREAD_SIGNAL_STATE.frame_counter &
              SIMULATION_RENDER_FLAG_PERIOD_MASK) == 0)
            {
              g_SIMULATION_THREAD_SIGNAL_STATE.render_flags |= SIMULATION_RENDER_FLAG_PERIODIC;
            }
          update_vehicle_runtime_objects();
          if(G_FRONTEND_EXIT_REQUESTED != SIMULATION_FRONTEND_EXIT_COMPLETE)
            {
              if((G_FRONTEND_EXIT_REQUESTED != SIMULATION_FRONTEND_MODE_TIMED) ||
                 (G_SIMULATION_UPDATE_MODE != SIMULATION_UPDATE_MODE_DIRECT) ||
                 (G_SIMULATION_FRAME_BUDGET > 0))
                {
                  if(G_SIMULATION_FRAME_BUDGET > 0)
                    G_SIMULATION_FRAME_BUDGET -= 1;
                  g_SIMULATION_THREAD_SIGNAL_STATE.frame_counter += 1;
                }
            }
        }
    }
  else if((g_SIMULATION_THREAD_SIGNAL_STATE.startup_frame_count == 0) &&
          (g_SIMULATION_TRANSITION_ACTIVE == 0))
    {
      g_SIMULATION_THREAD_SIGNAL_STATE.frame_initialized = 1;
      g_SIMULATION_THREAD_SIGNAL_STATE.frame_counter     = 0;
      G_FRONTEND_EXIT_REQUESTED                          = 1;
      g_SIMULATION_PENDING_VALUE                         = 0;
      g_SIMULATION_THREAD_SIGNAL_STATE.render_flags      = -1;
    }
  else
    {
      g_SIMULATION_THREAD_SIGNAL_STATE.frame_counter = 0;
      g_SIMULATION_THREAD_SIGNAL_STATE.render_flags  = SIMULATION_RENDER_FLAGS_STARTUP;
      G_FRONTEND_EXIT_REQUESTED                      = 0;
      update_vehicle_runtime_objects();
      g_SIMULATION_THREAD_SIGNAL_STATE.startup_frame_count -= 1;
      render_coordinate = (G_TRACK_DATA->section_count * SIMULATION_PAUSE_COORDINATE_SCALE) -
                          SIMULATION_RENDER_CONTEXT_OFFSET;
      configure_vehicle_render_context(render_coordinate);
      if(g_STARTUP_CONFIGURATION.control_preset_index == SIMULATION_JOYSTICK_PRESET_INDEX)
        {
          g_CONTROL_INPUT_POLL_STATE.simulation_start_delay = SIMULATION_START_DELAY_FIELDS;
          g_CONTROL_INPUT_POLL_STATE.simulation_start_scale = SIMULATION_START_SCALE;
        }
      else
        {
          g_CONTROL_INPUT_POLL_STATE.simulation_start_delay = 0;
          g_CONTROL_INPUT_POLL_STATE.simulation_start_scale = 0;
        }
    }

  update_simulation_mode_state(
    (const struct TrackOrientationState *) ((uint8 *) G_ACTIVE_SIMULATION_SOURCE +
                                            SIMULATION_SOURCE_MODE_STATE_OFFSET));
  g_SIMULATION_THREAD_SIGNAL_STATE.frame_timer -= 1;
  if(g_SIMULATION_THREAD_SIGNAL_STATE.frame_timer != 0)
    {
      if(g_SIMULATION_THREAD_SIGNAL_STATE.frame_timer == SIMULATION_FRAME_TIMER_SENTINEL)
        g_RENDER_THREAD_STATE.render_phase = 1;
      return;
    }

  phase = (g_SIMULATION_THREAD_SIGNAL_STATE.frame_phase_source + 1);
  if(phase < 0)
    g_SIMULATION_THREAD_SIGNAL_STATE.frame_phase = -(phase & 1);
  else
    g_SIMULATION_THREAD_SIGNAL_STATE.frame_phase = (phase & 1);
}

void
run_simulation_thread(void)
{
  uint32 received_signals;

  _initialize_simulation_thread_state();
  g_SIMULATION_THREAD_SIGNAL_STATE.signal_mask = AllocSignal(0);
  if(g_SIMULATION_THREAD_SIGNAL_STATE.signal_mask == 0)
    return;

  ScavengeMem();
  open_bitmap_display_items();
  ScavengeMem();
  g_SIMULATION_THREAD_SIGNAL_STATE.vbl_io_request = GetVBLIOReq();
  initialize_game_audio_system();
  load_simulation_common_art();
  initialize_simulation_render_resources();
  ScavengeMem();
  _configure_simulation_bitmap_controls(SIMULATION_BITMAP_CE_CONTROL_INITIAL);
  load_random_vehicle_art(1);
  initialize_vehicle_runtime_objects();
  ScavengeMem();
  initialize_tire_smoke_particles(&g_TIRE_SMOKE_PARTICLES);
  ScavengeMem();
  reset_runtime_state();
  ScavengeMem();
  load_random_vehicle_art(0);
  _wait_for_simulation_start();
  ScavengeMem();
  initialize_vehicle_drive_state();
  initialize_pause_game_display((G_TRACK_DATA->section_count * SIMULATION_PAUSE_COORDINATE_SCALE) -
                                  SIMULATION_PAUSE_COORDINATE_OFFSET,
                                g_PAUSE_DISPLAY_PROFILE_VALUES[g_TRACK_INDEX][g_EVENT_INDEX]);
  ScavengeMem();
  ScavengeMem();
  _configure_simulation_bitmap_controls(SIMULATION_BITMAP_CE_CONTROL_RUNNING);

  while(g_SIMULATION_THREAD_SIGNAL_STATE.run_requested != 0)
    {
      received_signals = WaitSignal(g_SIMULATION_THREAD_SIGNAL_STATE.signal_mask);
      if((received_signals & g_SIMULATION_THREAD_SIGNAL_STATE.signal_mask) != 0)
        _process_simulation_signal();
    }

  (void) DeleteItem(g_SIMULATION_THREAD_SIGNAL_STATE.vbl_io_request);
  shutdown_simulation_resources();
  free_simulation_common_art();
  (void) FreeSignal(g_SIMULATION_THREAD_SIGNAL_STATE.signal_mask);
  g_SIMULATION_THREAD_SIGNAL_STATE.shutdown_complete = 1;
  ScavengeMem();
  exit(0);
}

int32
initialize_game_input(void)
{
  g_SIMULATION_THREAD_SIGNAL_STATE.start_complete = 0;
  g_SIMULATION_THREAD_SIGNAL_STATE.thread_item =
    create_task_with_allocated_stack(run_simulation_thread,
                                     SIMULATION_THREAD_STACK_BYTES,
                                     SIMULATION_THREAD_PRIORITY,
                                     SIMULATION_THREAD_NAME,
                                     &g_SIMULATION_THREAD_SIGNAL_STATE.stack_allocation);
  while(g_SIMULATION_THREAD_SIGNAL_STATE.start_complete == 0)
    delay_vbls(SIMULATION_THREAD_START_DELAY_FIELDS);

  return g_SIMULATION_THREAD_SIGNAL_STATE.thread_item != 0;
}

void
prepare_default_game_mode(void)
{
  g_SIMULATION_THREAD_SIGNAL_STATE.run_requested     = 0;
  g_SIMULATION_THREAD_SIGNAL_STATE.shutdown_complete = 0;
  g_RENDER_THREAD_STATE.pipeline_initialized         = 1;
  while(g_SIMULATION_THREAD_SIGNAL_STATE.shutdown_complete == 0)
    {
      signal_simulation_thread();
      delay_vbls(SIMULATION_THREAD_STOP_DELAY_FIELDS);
    }

  g_RENDER_THREAD_STATE.pipeline_initialized = 0;
  (void) release_task_stack_allocation(&g_SIMULATION_THREAD_SIGNAL_STATE.stack_allocation);
  g_ACTIVE_PLAYER_INDEX = -1;
}

static void
_initialize_simulation_thread_state(void)
{
  g_SIMULATION_THREAD_SIGNAL_STATE.startup_frame_count = SIMULATION_THREAD_STARTUP_FRAME_COUNT;
  g_SIMULATION_THREAD_SIGNAL_STATE.frame_phase         = 0;
  g_SIMULATION_THREAD_SIGNAL_STATE.frame_phase_source  = 0;
  g_SIMULATION_THREAD_SIGNAL_STATE.frame_counter       = 0;
  G_FRONTEND_EXIT_REQUESTED                            = 0;
  g_SIMULATION_MODE_TRANSITION_COOLDOWN                = 0;
  g_SIMULATION_THREAD_SIGNAL_STATE.frame_timer         = 0;
  g_SIMULATION_THREAD_SIGNAL_STATE.frame_timer_aux     = 0;
  g_SIMULATION_THREAD_SIGNAL_STATE.frame_initialized   = 0;
  g_SIMULATION_TRANSITION_ACTIVE                       = 0;
}

static void
_configure_simulation_bitmap_controls(int32 control_)
{
  (void) SetCEControl(
    g_BITMAP_DISPLAY_STATE.bitmap_items[g_BITMAP_DISPLAY_STATE.current_bitmap_index],
    control_,
    SIMULATION_BITMAP_CE_CONTROL_MASK);
  (void) SetCEControl(
    g_BITMAP_DISPLAY_STATE.bitmap_items[1 - g_BITMAP_DISPLAY_STATE.current_bitmap_index],
    control_,
    SIMULATION_BITMAP_CE_CONTROL_MASK);
}

static void
_wait_for_simulation_start(void)
{
  int32 startup_index;

  for(startup_index = 1; startup_index < SIMULATION_THREAD_STARTUP_FRAME_COUNT; startup_index += 1)
    {
      g_SIMULATION_THREAD_SIGNAL_STATE.run_requested  = 0;
      g_SIMULATION_THREAD_SIGNAL_STATE.start_complete = 0;
    }
  g_SIMULATION_THREAD_SIGNAL_STATE.run_requested  = 1;
  g_SIMULATION_THREAD_SIGNAL_STATE.start_complete = 1;
  while(g_RENDER_THREAD_STATE.pipeline_initialized == 0)
    {
      if(g_RENDER_THREAD_STATE.frame_complete != 0)
        g_RENDER_THREAD_STATE.frame_complete += 1;
      signal_render_thread();
      wait_for_simulation_thread_signal();
    }
}

static void
_process_simulation_signal(void)
{
  if(g_GAME_AUDIO_SHUTDOWN_REQUESTED.requested != 0)
    {
      shutdown_game_audio_system();
      g_GAME_AUDIO_SHUTDOWN_REQUESTED.requested = 0;
      return;
    }
  if(g_GAME_AUDIO_INITIALIZE_REQUESTED.requested != 0)
    {
      initialize_game_audio_system();
      g_GAME_AUDIO_INITIALIZE_REQUESTED.requested = 0;
      return;
    }
  if(g_RACE_ACTIVE == 0)
    return;

  if(G_FRONTEND_EXIT_REQUESTED == SIMULATION_FRONTEND_MODE_TIMED)
    {
      if((G_SIMULATION_UPDATE_MODE == SIMULATION_UPDATE_MODE_DIRECT_ALT) ||
         (G_SIMULATION_UPDATE_MODE == SIMULATION_UPDATE_MODE_DIRECT))
        {
          update_simulation_frame();
          signal_render_thread();
          g_SIMULATION_UPDATE_COUNTDOWN = 0;
        }
      else if(G_SIMULATION_UPDATE_MODE < SIMULATION_UPDATE_MODE_LIMIT)
        {
          if(g_SIMULATION_UPDATE_COUNTDOWN <= 0)
            {
              g_SIMULATION_UPDATE_COUNTDOWN = g_SIMULATION_UPDATE_REPEAT_COUNT;
              update_simulation_frame();
              signal_render_thread();
            }
          g_SIMULATION_UPDATE_COUNTDOWN -= 1;
        }
      return;
    }

  if(G_FRONTEND_EXIT_REQUESTED != SIMULATION_FRONTEND_MODE_BLOCKED)
    {
      update_simulation_frame();
      signal_render_thread();
      return;
    }

  do
    {
      update_simulation_frame();
    }
  while(G_FRONTEND_EXIT_REQUESTED == SIMULATION_FRONTEND_MODE_BLOCKED);
}
