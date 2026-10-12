#pragma once

#include "game_resource_cleanup.h"
#include "render_thread_signal.h"
#include "task_stack_allocation.h"

#include "kernel.h"
#include "types.h"

#define VEHICLE_RENDER_TUNING_COUNT 3
#define VEHICLE_RENDER_TYPE_COUNT 8

typedef struct SimulationAudioRequestFlag
{
  volatile uint8 requested;
  uint8          reserved_01[3];
} SimulationAudioRequestFlag;

typedef struct SimulationThreadSignalState
{
  uint8               reserved_00[0x64];
  TaskStackAllocation stack_allocation;
  Item                thread_item;
  uint32              signal_mask;
  volatile uint8      run_requested;
  uint8               reserved_79[3];
  volatile uint8      start_complete;
  uint8               reserved_7_d[3];
  int32               startup_frame_count;
  uint8               frame_initialized;
  uint8               reserved_85[3];
  volatile uint8      shutdown_complete;
  uint8               reserved_89[3];
  Item                vbl_io_request;
  int32               frame_phase;
  int32               frame_phase_source;
  int32               frame_counter;
  int32               runtime_update_blocked;
  int32               render_flags;
  int32               frame_timer;
  int32               frame_timer_aux;
  uint8               reserved_ac[0x18];
  uint8               reserved_c4[4];
} SimulationThreadSignalState;

extern SimulationThreadSignalState g_SIMULATION_THREAD_SIGNAL_STATE;
extern int32                       g_ACTIVE_PLAYER_INDEX;
/* Retail stores the simulation control state in GameResourceState. */
#define G_FRONTEND_EXIT_REQUESTED (g_GAME_RESOURCE_STATE.sound_requests_enabled)
#define G_SIMULATION_UPDATE_MODE (g_GAME_RESOURCE_STATE.replay_view_context)
#define G_SIMULATION_FRAME_BUDGET (g_GAME_RESOURCE_STATE.replay_pause_mode)
extern int32          g_SIMULATION_UPDATE_REPEAT_COUNT;
extern SimulationAudioRequestFlag g_GAME_AUDIO_SHUTDOWN_REQUESTED;
extern SimulationAudioRequestFlag g_GAME_AUDIO_INITIALIZE_REQUESTED;
extern volatile uint8 g_DORMANT_SIMULATION_REQUEST;
extern uint8          g_SIMULATION_TRANSITION_ACTIVE;
extern int32          g_SIMULATION_UPDATE_COUNTDOWN;
extern int32          g_SIMULATION_PENDING_VALUE;
extern int32          g_PAUSE_DISPLAY_PROFILE_VALUES[8][3];
extern int32 g_VEHICLE_RENDER_TUNING_SCALES[VEHICLE_RENDER_TUNING_COUNT][VEHICLE_RENDER_TYPE_COUNT];

void
signal_simulation_thread(void);
void
free_simulation_common_art(void);
void
wait_for_simulation_thread_signal(void);
void
start_game_rendering(void);
void
stop_game_rendering(void);
void
request_game_audio_shutdown(void);
void
request_game_audio_initialization(void);
void
request_dormant_simulation_operation(void);
void
configure_vehicle_render_context(int32 coordinate_);
void
update_simulation_frame(void);
void
run_simulation_thread(void);
int32
initialize_game_input(void);
void
prepare_default_game_mode(void);
