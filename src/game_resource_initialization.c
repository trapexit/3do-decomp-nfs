#include "game_resource_initialization.h"

#include "control_input_poll_state.h"
#include "driving_warning_art.h"
#include "driving_warning_display.h"
#include "game_resource_cleanup.h"
#include "memory.h"
#include "simulation_thread_signal.h"
#include "vehicle_simulation_state.h"

#define USER_CONTROL_BUFFER_BYTES 0x1C20
#define USER_CONTROL_SECONDARY_BUFFER_BYTES 0x0E11
#define PLAYER_MODEL_STATE_BYTES 0x26A0
#define OPPONENT_CAR_MODEL_STATE_BYTES 0xCF00
#define GAME_RESOURCE_ALLOCATION_FLAGS 0x20
#define SIMULATION_PENDING_INITIAL_VALUE 300

void
initialize_game_resource_state(void)
{
  int32 bucket_index;

  g_GAME_RESOURCE_STATE.control_value_history =
    (uint8 *) allocate_named_memory("UsrCtrlBuff",
                                    USER_CONTROL_BUFFER_BYTES,
                                    GAME_RESOURCE_ALLOCATION_FLAGS);
  g_GAME_RESOURCE_STATE.control_nibble_history =
    (uint8 *) allocate_named_memory("UsrCtrlBuff2",
                                    USER_CONTROL_SECONDARY_BUFFER_BYTES,
                                    GAME_RESOURCE_ALLOCATION_FLAGS);
  g_GAME_RESOURCE_STATE.allocation_third =
    allocate_named_memory("PlayerMS", PLAYER_MODEL_STATE_BYTES, GAME_RESOURCE_ALLOCATION_FLAGS);
  g_GAME_RESOURCE_STATE.allocation_fourth = allocate_named_memory("OppCarMS",
                                                                  OPPONENT_CAR_MODEL_STATE_BYTES,
                                                                  GAME_RESOURCE_ALLOCATION_FLAGS);

  g_DRIVING_WARNING_METER_STATE.reserved_00 = 0;
  g_DRIVING_WARNING_METER_STATE.input_value = 0;
  g_SIMULATION_PENDING_VALUE                = SIMULATION_PENDING_INITIAL_VALUE;
  for(bucket_index = 0; bucket_index < SOUND_EFFECT_REQUEST_BUCKET_COUNT; ++bucket_index)
    {
      g_SOUND_EFFECT_REQUEST_BUCKETS[bucket_index] = 0;
    }

  initialize_driving_warning_art_cels();
  g_GAME_RESOURCE_STATE.replay_vehicle_sprite_rotation = 0;
  g_GAME_RESOURCE_STATE.control_history_initialized    = 0;
  configure_frontend_display(0, 0);
  g_GAME_RESOURCE_STATE.runtime_initialized  = 0;
  g_GAME_RESOURCE_STATE.driving_warning_mode = 0;
}
