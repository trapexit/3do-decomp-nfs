#pragma once

#include "race_state.h"

#define FRONTEND_RACE_OPTIONS_GAMEPAD_PRESET_COUNT 5
#define FRONTEND_RACE_OPTIONS_PRESET_WORD_COUNT 13
#define FRONTEND_RACE_OPTIONS_JOYSTICK_NAME_COUNT 11
#define FRONTEND_RACE_OPTIONS_FRAME_RATE 60

typedef struct FrontendControlPresetTable
{
  uint32      gamepad[FRONTEND_RACE_OPTIONS_GAMEPAD_PRESET_COUNT]
                     [FRONTEND_RACE_OPTIONS_PRESET_WORD_COUNT];
  const char *joystick[FRONTEND_RACE_OPTIONS_JOYSTICK_NAME_COUNT];
} FrontendControlPresetTable;

typedef struct FrontendControlPosition
{
  int32 x;
  int32 y;
} FrontendControlPosition;

typedef struct FrontendJoystickCalibrationState
{
  uint32 primary_action;
  uint32 secondary_action;
  uint32 tertiary_action;
  uint32 reserved_before_shift_actions[2];
  uint32 right_shift_action;
  uint32 left_shift_action;
  uint32 down_left_action;
  uint32 reserved_before_cursor[2];
  int32  cursor_x;
  int32  cursor_y;
  uint32 reserved_before_active;
  int32  active;
} FrontendJoystickCalibrationState;

extern FrontendControlPresetTable g_FRONTEND_CONTROL_PRESETS;
extern uint8                      g_FRONTEND_CALIBRATION_SUCCEEDED;
extern const char                *g_FRONTEND_RACE_SKILL_LEVEL_SHAPE_NAMES[6];
extern const char                *g_FRONTEND_RACE_TOGGLE_SHAPE_NAMES[2];
extern const char                *g_FRONTEND_RACE_CONTROL_PRESET_SHAPE_NAMES[6];
extern const char                *g_FRONTEND_RACE_STEREO_SHAPE_NAMES[2];
extern int32                      g_FRONTEND_VEHICLE_TRACTION_CONTROL_AVAILABILITY[8];
extern int32                      g_FRONTEND_RACE_OPTION_X_POSITIONS[8];
extern int32                      g_FRONTEND_RACE_OPTION_Y_POSITIONS[8];
extern void                      *g_FRONTEND_RACE_OPTIONS_SHAPES;
extern void                      *g_FRONTEND_CALIBRATION_SHAPES;
extern FrontendControlPosition
  g_FRONTEND_CONTROL_OFFSCREEN_POSITIONS[FRONTEND_RACE_OPTIONS_JOYSTICK_NAME_COUNT];
extern FrontendControlPosition
             g_FRONTEND_CONTROL_PRESET_POSITIONS[5][FRONTEND_RACE_OPTIONS_JOYSTICK_NAME_COUNT];
extern void *g_FRONTEND_CONTROL_SELECTION_SHAPES;

int32
draw_frontend_race_options(int32 selection_index_, RaceConfiguration *configuration_);
void
draw_frontend_calibration(void);
int32
calibrate_frontend_controls(void);
int32
configure_frontend_race_options(int32 frame_count_, RaceConfiguration *configuration_);
int32
select_frontend_control_preset(int32 frame_count_, RaceConfiguration *configuration_);
int32
draw_frontend_control_preset(int32 previous_preset_index_, int32 preset_index_);
