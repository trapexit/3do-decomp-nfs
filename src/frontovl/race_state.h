#pragma once

#include "types.h"

#define RACE_CONFIGURATION g_RACE_CONFIGURATION
#define RACE_PARTICIPANT_RECORDS g_RACE_PARTICIPANT_RECORDS
#define RACE_TIMING_STATE g_RACE_TIMING_STATE
#define POST_RACE_RESULT_NAME_BYTE_COUNT 13
#define RACE_CONFIGURATION_BYTE_COUNT 0x60
#define RACE_UNASSIGNED_CONTROL_COUNT 6

typedef struct PostRaceResultEntry
{
  char  name[POST_RACE_RESULT_NAME_BYTE_COUNT];
  uint8 class_index;
  uint8 track_index;
  uint8 vehicle_index;
  int32 score_divisor;
  int32 secondary_score;
  int32 tertiary_score;
} PostRaceResultEntry;

typedef struct RaceControlBindings
{
  uint32 primary_action;
  uint32 secondary_action;
  uint32 tertiary_action;
  uint32 right_shift_action;
  uint32 left_shift_action;
  uint32 up_left_action;
  uint32 left_action;
  uint32 down_left_action;
  uint32 up_right_action;
  uint32 right_action;
  uint32 down_right_action;
  uint32 start_action;
  uint32 auxiliary_action;
  uint32 unassigned[RACE_UNASSIGNED_CONTROL_COUNT];
} RaceControlBindings;

typedef struct RaceConfiguration
{
  int32               version;
  uint8               track_index;
  uint8               event_index;
  uint8               player_vehicle_index;
  uint8               opponent_vehicle_index;
  uint8               music_enabled;
  uint8               sound_effects_enabled;
  uint8               stereo_enabled;
  uint8               anti_lock_brakes_enabled;
  uint8               traction_control_enabled;
  uint8               performance_reduction_enabled;
  uint8               skill_level;
  uint8               host_commentary_enabled;
  uint8               control_preset_index;
  uint8               joystick_calibration_configured;
  uint8               reserved_before_controls[2];
  RaceControlBindings control_bindings;
} RaceConfiguration;

typedef char
  RaceConfigurationSizeCheck[sizeof(RaceConfiguration) == RACE_CONFIGURATION_BYTE_COUNT ? 1 : -1];

typedef struct RaceTimingState
{
  uint8 reserved_before_special_event[0x210];
  int32 special_event;
  int32 primary_time;
  uint8 reserved_before_alternate_time[4];
  int32 alternate_time;
  int32 progress;
  int32 secondary_time;
  uint8 reserved_before_penalty_count[0x10];
  int32 penalty_count;
  int32 completed_objective_count;
  int32 bonus_objective_count;
} RaceTimingState;

extern RaceConfiguration *g_RACE_CONFIGURATION;
extern uint8             *g_RACE_PARTICIPANT_RECORDS;
extern RaceTimingState   *g_RACE_TIMING_STATE;
