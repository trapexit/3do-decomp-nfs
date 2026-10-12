#pragma once

#include "types.h"

typedef struct StartupConfiguration
{
  uint32 configuration_flags;
  uint8  track_index;
  uint8  event_index;
  uint8  player_vehicle_index;
  uint8  opponent_vehicle_index;
  uint8  music_enabled;
  uint8  sound_effects_enabled;
  uint8  stereo_enabled;
  uint8  anti_lock_brakes_enabled;
  uint8  traction_control_enabled;
  uint8  performance_reduction_enabled;
  uint8  skill_level;
  uint8  host_commentary_enabled;
  uint8  control_preset_index;
  uint8  joystick_calibration_configured;
  uint8  reserved_12[2];
  uint32 control_button_masks[13];
  int32  joystick_x_minimum;
  int32  joystick_x_center;
  int32  joystick_x_maximum;
  int32  joystick_y_minimum;
  int32  joystick_y_center;
  int32  joystick_y_maximum;
} StartupConfiguration;

extern int32                g_ANTI_LOCK_BRAKES_ENABLED;
extern int32                g_TRACTION_CONTROL_ENABLED;
extern int32                g_STARTUP_DEBUG_MODE;
extern int32                g_STARTUP_BUTTON_MODE;
extern int32                g_STARTUP_EVENT_MODE;
extern int32                g_STARTUP_HELP_MODE;
extern char                *g_STARTUP_ARGUMENT;
extern StartupConfiguration g_STARTUP_CONFIGURATION;
extern uint8                g_STARTUP_RENDER_FLAG;
extern int32                g_GAME_MODE_INITIALIZED;
extern int32                g_BOOTSTRAP_GAME_MODE;

int
main(int argc_, char **argv_);
void
initialize_game_mode(int32 display_mode_);
void
finalize_game_mode(int32 result_);
