#pragma once

#include "graphics.h"
#include "types.h"

typedef struct GameResourceState
{
  uint8  reserved_00[4];
  uint8  replay_sequence_advance_pending;
  uint8  reserved_05[3];
  uint8  runtime_initialized;
  uint8  reserved_09[3];
  int32  replay_update_count;
  int32  sound_bucket_frame;
  int32  sound_request_restore_pending;
  int32  sound_requests_enabled;
  int32  replay_input_mode;
  int32  replay_view_context;
  int32  replay_pause_mode;
  int32  replay_view_variant;
  uint8  reserved_2c[4];
  int32  replay_vehicle_sprite_rotation;
  int32  replay_completion_pending;
  int32  replay_sequence_first_pending;
  int32  replay_sequence_second_pending;
  int32  replay_sequence_fourth_pending;
  int32  driving_warning_mode;
  int32  sound_effect_request_range_count;
  int32  replay_sequence_index;
  int32  replay_track_slot_pending;
  int32  replay_saved_view_mode;
  void  *driving_warning_art_family;
  int32  control_history_initialized;
  int32  traction_control_enabled;
  uint8 *control_value_history;
  uint8 *control_nibble_history;
  int32  replay_frame_limit;
  int32  replay_requested_frame;
  int32  driving_warning_status_interval;
  int32  replay_restore_pending;
  int32  player_snapshot_restore_active;
  int32  replay_control_mode;
  void  *allocation_third;
  void  *allocation_fourth;
  int32  sound_bucket_index;
  int32  replay_render_sync_pending;
  uint8  reserved_94[0x10];
  CCB   *driving_warning_art_cels[18];
  uint8  reserved_ec[8];
  uint32 replay_input_buttons;
  int32  last_sound_request_frame;
  int32  last_sound_request_id;
  uint32 replay_previous_buttons;
  int32  replay_input_repeat_count;
  uint32 replay_raw_buttons;
  uint8  replay_sequence_active;
} GameResourceState;

extern GameResourceState g_GAME_RESOURCE_STATE;

void
release_game_resource_allocations(void);
