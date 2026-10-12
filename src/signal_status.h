#pragma once

#include "types.h"

#define TRACK_SIGNAL_STATUS_COUNT_OFFSET 0x16C40

typedef struct SignalStatus
{
  uint8 value;
  uint8 reserved_01[3];
} SignalStatus;

typedef struct RuntimeState
{
  int32 linear_motion_sleep_threshold;
  int32 angular_motion_sleep_threshold;
  int32 orientation_sleep_threshold;
  int32 traffic_coordination_countdown;
  int32 collision_simulation_enabled;
  int32 road_object_seek_record_index;
  int32 road_object_seek_record_id;
  int32 road_object_activation_record_index;
  int32 road_object_activation_record_id;
  int32 collision_threshold_default;
  int32 collision_threshold_player;
  int32 negative_road_boundary_margin;
  int32 positive_road_boundary_margin;
  int32 collision_sound_countdown;
  int32 collision_sound_cooldown_time;
} RuntimeState;

extern RuntimeState g_RUNTIME_STATE;

void
initialize_signal_statuses(int32 signal_count_hint_);
void
free_signal_statuses(void);
void
reset_runtime_state(void);
