#include "frontend_race_options.h"

#include "controller_input.h"
#include "frontend_audio_control.h"
#include "frontend_frame.h"
#include "frontend_movies.h"
#include "frontend_options.h"
#include "frontend_runtime_state.h"
#include "frontend_skill_menu.h"
#include "frontend_sound.h"
#include "post_race_stats.h"
#include "runtime_memory.h"
#include "runtime_memory_ops.h"
#include "tracked_memory.h"

#include "event.h"

#define FRONTEND_RACE_OPTIONS_SHAPE_PATH "frontend/display/options.3sh"
#define FRONTEND_CALIBRATION_SHAPE_PATH "frontend/display/Calibration.3sh"
#define FRONTEND_RACE_OPTIONS_BACKGROUND_SHAPE "bgnd"
#define FRONTEND_RACE_OPTIONS_UNAVAILABLE_SHAPE "na  "
#define FRONTEND_RACE_OPTIONS_SELECTOR_SHAPE "lite"
#define FRONTEND_CALIBRATION_KNOB_SHAPE "knob"
#define FRONTEND_RACE_OPTIONS_POSITION_WORD 3
#define FRONTEND_RACE_OPTIONS_DIMENSION_WORD 1
#define FRONTEND_RACE_OPTIONS_X_SHIFT 16
#define FRONTEND_RACE_OPTIONS_VALUE_X_OFFSET 0xD5
#define FRONTEND_RACE_OPTIONS_VALUE_Y_OFFSET 7
#define FRONTEND_RACE_OPTIONS_SELECTION_COUNT 8
#define FRONTEND_RACE_OPTIONS_SKILL_SELECTION 0
#define FRONTEND_RACE_OPTIONS_AUDIO_SELECTION 1
#define FRONTEND_RACE_OPTIONS_STEREO_SELECTION 2
#define FRONTEND_RACE_OPTIONS_HOST_SELECTION 3
#define FRONTEND_RACE_OPTIONS_ABS_SELECTION 4
#define FRONTEND_RACE_OPTIONS_TRACTION_CONTROL_SELECTION 5
#define FRONTEND_RACE_OPTIONS_CONTROL_SELECTION 6
#define FRONTEND_RACE_OPTIONS_STATS_SELECTION 7
#define FRONTEND_RACE_OPTIONS_SKILL_LEVEL_COUNT 3
#define FRONTEND_RACE_OPTIONS_HOST_SKILL_LEVEL_LIMIT 2
#define FRONTEND_RACE_OPTIONS_AUDIO_MODE_COUNT 4
#define FRONTEND_RACE_OPTIONS_CONTROL_PRESET_COUNT 6
#define FRONTEND_RACE_OPTIONS_PRESET_BYTE_COUNT 0x34
#define FRONTEND_RACE_OPTIONS_JOYSTICK_NAME_BYTE_COUNT 0x2C
#define FRONTEND_RACE_OPTIONS_CONFIRM_MASK (ControlA | ControlB | ControlC)
#define FRONTEND_RACE_OPTIONS_INPUT_MASK 0xCFE00000
#define FRONTEND_RACE_OPTIONS_HIDDEN_STEP_ONE 0x00C00000
#define FRONTEND_RACE_OPTIONS_HIDDEN_STEP_TWO 0x00E00000
#define FRONTEND_RACE_OPTIONS_HIDDEN_STEP_THREE 0x08E00000
#define FRONTEND_RACE_OPTIONS_HIDDEN_TIMEOUT 10
#define FRONTEND_RACE_OPTIONS_HIDDEN_FLAG 0x80
#define FRONTEND_RACE_OPTIONS_SKILL_LEVEL_MASK 3
#define FRONTEND_RACE_OPTIONS_DIMENSION_X_SHIFT 16
#define FRONTEND_RACE_OPTIONS_DIMENSION_Y_SHIFT 20
#define FRONTEND_RACE_OPTIONS_DIMENSION_CENTER_DIVISOR 2
#define FRONTEND_CALIBRATION_PRIMARY_INDEX 0
#define FRONTEND_CALIBRATION_SECONDARY_INDEX 1
#define FRONTEND_CALIBRATION_TERTIARY_INDEX 2
#define FRONTEND_CALIBRATION_RIGHT_SHIFT_INDEX 3
#define FRONTEND_CALIBRATION_LEFT_SHIFT_INDEX 4
#define FRONTEND_CALIBRATION_DOWN_LEFT_INDEX 5
#define FRONTEND_CALIBRATION_CURSOR_X_CENTER 0x9C
#define FRONTEND_CALIBRATION_CURSOR_Y_CENTER 0x40
#define FRONTEND_CALIBRATION_CURSOR_X_SCALE 0x58
#define FRONTEND_CALIBRATION_CURSOR_Y_SCALE 0x74
#define FRONTEND_CALIBRATION_AXIS_SCALE 1024
#define FRONTEND_CALIBRATION_DELAY_FRAMES 2
#define FRONTEND_CALIBRATION_CONTROLLER_NUMBER 1

FrontendControlPresetTable g_FRONTEND_CONTROL_PRESETS = {
  { { ControlA,
      ControlB,
      ControlC,
      ControlRightShift,
      ControlLeftShift,
      ControlUp | ControlLeft,
      ControlLeft,
      ControlDown | ControlLeft,
      ControlUp | ControlRight,
      ControlRight,
      ControlDown | ControlRight,
      ControlStart,
      ControlX },
    { ControlB,
      ControlA,
      ControlC,
      ControlRightShift,
      ControlLeftShift,
      ControlUp | ControlLeft,
      ControlLeft,
      ControlDown | ControlLeft,
      ControlUp | ControlRight,
      ControlRight,
      ControlDown | ControlRight,
      ControlStart,
      ControlX },
    { ControlUp,
      ControlDown,
      ControlC,
      ControlA,
      ControlB,
      0,
      ControlLeftShift,
      0,
      0,
      ControlRightShift,
      0,
      ControlStart,
      ControlX },
    { ControlA,
      ControlB,
      ControlC,
      ControlUp,
      ControlDown,
      0,
      ControlLeftShift,
      0,
      0,
      ControlRightShift,
      0,
      ControlStart,
      ControlX },
    { ControlA,
      ControlB,
      ControlC,
      ControlUp,
      ControlDown,
      ControlUp | ControlLeft,
      ControlLeft,
      ControlDown | ControlLeft,
      ControlUp | ControlRight,
      ControlRight,
      ControlDown | ControlRight,
      ControlStart,
      ControlX } },
  { "bgnd", "ctrl", "accl", "brak", "camr", "dnsh", "upsh", "strt", "tnlf", "tnrt", "fram" }
};

uint8       g_FRONTEND_CALIBRATION_SUCCEEDED                    = 1;
const char *g_FRONTEND_RACE_SKILL_LEVEL_SHAPE_NAMES[6]          = { "novc", "intr", "pro ",
                                                                    "pnov", "pint", "ppro" };
const char *g_FRONTEND_RACE_TOGGLE_SHAPE_NAMES[2]               = { "off ", "on  " };
const char *g_FRONTEND_RACE_CONTROL_PRESET_SHAPE_NAMES[6]       = { "1   ", "2   ", "3   ",
                                                                    "4   ", "5   ", "joy " };
const char *g_FRONTEND_RACE_STEREO_SHAPE_NAMES[2]               = { "mono", "ster" };
int32       g_FRONTEND_VEHICLE_TRACTION_CONTROL_AVAILABILITY[8] = { 1, 0, 0, 1, 0, 0, 1, 0 };
int32       g_FRONTEND_RACE_OPTION_X_POSITIONS[FRONTEND_RACE_OPTIONS_SELECTION_COUNT] = {
  0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28
};
int32 g_FRONTEND_RACE_OPTION_Y_POSITIONS[FRONTEND_RACE_OPTIONS_SELECTION_COUNT] = {
  0x10, 0x2A, 0x46, 0x60, 0x7C, 0x97, 0xB1, 0xCD
};
void *g_FRONTEND_RACE_OPTIONS_SHAPES;
void *g_FRONTEND_CALIBRATION_SHAPES;

typedef char
  FrontendControlPresetTableSizeCheck[sizeof(g_FRONTEND_CONTROL_PRESETS) == 0x130 ? 1 : -1];
typedef char FrontendRaceSkillLevelShapeNamesSizeCheck
  [sizeof(g_FRONTEND_RACE_SKILL_LEVEL_SHAPE_NAMES) == 24 ? 1 : -1];
typedef char
  FrontendRaceToggleShapeNamesSizeCheck[sizeof(g_FRONTEND_RACE_TOGGLE_SHAPE_NAMES) == 8 ? 1 : -1];
typedef char FrontendRaceControlPresetShapeNamesSizeCheck
  [sizeof(g_FRONTEND_RACE_CONTROL_PRESET_SHAPE_NAMES) == 24 ? 1 : -1];
typedef char
  FrontendRaceStereoShapeNamesSizeCheck[sizeof(g_FRONTEND_RACE_STEREO_SHAPE_NAMES) == 8 ? 1 : -1];
typedef char FrontendVehicleTractionControlAvailabilitySizeCheck
  [sizeof(g_FRONTEND_VEHICLE_TRACTION_CONTROL_AVAILABILITY) == 32 ? 1 : -1];
typedef char
  FrontendRaceOptionXPositionsSizeCheck[sizeof(g_FRONTEND_RACE_OPTION_X_POSITIONS) == 32 ? 1 : -1];
typedef char
  FrontendRaceOptionYPositionsSizeCheck[sizeof(g_FRONTEND_RACE_OPTION_Y_POSITIONS) == 32 ? 1 : -1];

int32
draw_frontend_race_options(int32 selection_index_, RaceConfiguration *configuration_)
{
  void *background_shape;
  void *selector_shape;
  void *shape;
  int32 skill_level;
  int32 audio_mode;
  int32 position;

  skill_level = (int32) configuration_->skill_level & FRONTEND_RACE_OPTIONS_SKILL_LEVEL_MASK;
  if((configuration_->skill_level & FRONTEND_RACE_OPTIONS_HIDDEN_FLAG) != 0)
    {
      skill_level += FRONTEND_RACE_OPTIONS_SKILL_LEVEL_COUNT;
    }

  initialize_frontend_display();
  begin_runtime_frame();
  synchronize_runtime_frame();
  clear_runtime_display(0);
  if(g_FRONTEND_RACE_OPTIONS_SHAPES == NULL)
    {
      g_FRONTEND_RACE_OPTIONS_SHAPES = load_runtime_file_data(FRONTEND_RACE_OPTIONS_SHAPE_PATH, 0);
      track_primary_allocation(g_FRONTEND_RACE_OPTIONS_SHAPES);
    }

  background_shape = locate_runtime_shape(g_FRONTEND_RACE_OPTIONS_SHAPES,
                                          (const uint32 *) FRONTEND_RACE_OPTIONS_BACKGROUND_SHAPE);
  position         = ((int32 *) background_shape)[FRONTEND_RACE_OPTIONS_POSITION_WORD];
  blit_runtime_shape_at(background_shape,
                        position >> FRONTEND_RACE_OPTIONS_X_SHIFT,
                        (int16) position);

  shape =
    locate_runtime_shape(g_FRONTEND_RACE_OPTIONS_SHAPES,
                         (const uint32 *) g_FRONTEND_RACE_SKILL_LEVEL_SHAPE_NAMES[skill_level]);
  position = ((int32 *) shape)[FRONTEND_RACE_OPTIONS_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_RACE_OPTIONS_X_SHIFT, (int16) position);
  audio_mode =
    (int32) configuration_->music_enabled | ((int32) configuration_->sound_effects_enabled << 1);
  shape    = locate_runtime_shape(g_FRONTEND_RACE_OPTIONS_SHAPES,
                                  (const uint32 *) g_FRONTEND_AUDIO_MODE_SHAPE_NAMES[audio_mode]);
  position = ((int32 *) shape)[FRONTEND_RACE_OPTIONS_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_RACE_OPTIONS_X_SHIFT, (int16) position);
  shape = locate_runtime_shape(
    g_FRONTEND_RACE_OPTIONS_SHAPES,
    (const uint32 *) g_FRONTEND_RACE_STEREO_SHAPE_NAMES[configuration_->stereo_enabled]);
  position = ((int32 *) shape)[FRONTEND_RACE_OPTIONS_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_RACE_OPTIONS_X_SHIFT, (int16) position);
  shape = locate_runtime_shape(
    g_FRONTEND_RACE_OPTIONS_SHAPES,
    (const uint32 *) g_FRONTEND_RACE_TOGGLE_SHAPE_NAMES[configuration_->host_commentary_enabled]);
  draw_runtime_shape_at(shape,
                        g_FRONTEND_RACE_OPTION_X_POSITIONS[FRONTEND_RACE_OPTIONS_HOST_SELECTION] +
                          FRONTEND_RACE_OPTIONS_VALUE_X_OFFSET,
                        g_FRONTEND_RACE_OPTION_Y_POSITIONS[FRONTEND_RACE_OPTIONS_HOST_SELECTION] +
                          FRONTEND_RACE_OPTIONS_VALUE_Y_OFFSET);

  if(g_FRONTEND_VEHICLE_ABS_AVAILABILITY[configuration_->player_vehicle_index] == 0)
    {
      shape = locate_runtime_shape(g_FRONTEND_RACE_OPTIONS_SHAPES,
                                   (const uint32 *) FRONTEND_RACE_OPTIONS_UNAVAILABLE_SHAPE);
      draw_runtime_shape_at(
        shape,
        g_FRONTEND_RACE_OPTION_X_POSITIONS[FRONTEND_RACE_OPTIONS_ABS_SELECTION] +
          FRONTEND_RACE_OPTIONS_VALUE_X_OFFSET,
        g_FRONTEND_RACE_OPTION_Y_POSITIONS[FRONTEND_RACE_OPTIONS_ABS_SELECTION] +
          FRONTEND_RACE_OPTIONS_VALUE_Y_OFFSET);
      configuration_->anti_lock_brakes_enabled = 0;
    }
  else
    {
      shape = locate_runtime_shape(
        g_FRONTEND_RACE_OPTIONS_SHAPES,
        (const uint32 *)
          g_FRONTEND_RACE_TOGGLE_SHAPE_NAMES[configuration_->anti_lock_brakes_enabled]);
      draw_runtime_shape_at(
        shape,
        g_FRONTEND_RACE_OPTION_X_POSITIONS[FRONTEND_RACE_OPTIONS_ABS_SELECTION] +
          FRONTEND_RACE_OPTIONS_VALUE_X_OFFSET,
        g_FRONTEND_RACE_OPTION_Y_POSITIONS[FRONTEND_RACE_OPTIONS_ABS_SELECTION] +
          FRONTEND_RACE_OPTIONS_VALUE_Y_OFFSET);
    }

  if(g_FRONTEND_VEHICLE_TRACTION_CONTROL_AVAILABILITY[configuration_->player_vehicle_index] == 0)
    {
      shape = locate_runtime_shape(g_FRONTEND_RACE_OPTIONS_SHAPES,
                                   (const uint32 *) FRONTEND_RACE_OPTIONS_UNAVAILABLE_SHAPE);
      draw_runtime_shape_at(
        shape,
        g_FRONTEND_RACE_OPTION_X_POSITIONS[FRONTEND_RACE_OPTIONS_TRACTION_CONTROL_SELECTION] +
          FRONTEND_RACE_OPTIONS_VALUE_X_OFFSET,
        g_FRONTEND_RACE_OPTION_Y_POSITIONS[FRONTEND_RACE_OPTIONS_TRACTION_CONTROL_SELECTION] +
          FRONTEND_RACE_OPTIONS_VALUE_Y_OFFSET);
      configuration_->traction_control_enabled = 0;
    }
  else
    {
      shape = locate_runtime_shape(
        g_FRONTEND_RACE_OPTIONS_SHAPES,
        (const uint32 *)
          g_FRONTEND_RACE_TOGGLE_SHAPE_NAMES[configuration_->traction_control_enabled]);
      draw_runtime_shape_at(
        shape,
        g_FRONTEND_RACE_OPTION_X_POSITIONS[FRONTEND_RACE_OPTIONS_TRACTION_CONTROL_SELECTION] +
          FRONTEND_RACE_OPTIONS_VALUE_X_OFFSET,
        g_FRONTEND_RACE_OPTION_Y_POSITIONS[FRONTEND_RACE_OPTIONS_TRACTION_CONTROL_SELECTION] +
          FRONTEND_RACE_OPTIONS_VALUE_Y_OFFSET);
    }

  shape = locate_runtime_shape(
    g_FRONTEND_RACE_OPTIONS_SHAPES,
    (const uint32 *)
      g_FRONTEND_RACE_CONTROL_PRESET_SHAPE_NAMES[configuration_->control_preset_index]);
  position = ((int32 *) shape)[FRONTEND_RACE_OPTIONS_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_RACE_OPTIONS_X_SHIFT, (int16) position);
  selector_shape = locate_runtime_shape(g_FRONTEND_RACE_OPTIONS_SHAPES,
                                        (const uint32 *) FRONTEND_RACE_OPTIONS_SELECTOR_SHAPE);
  draw_runtime_shape_at(selector_shape,
                        g_FRONTEND_RACE_OPTION_X_POSITIONS[selection_index_],
                        g_FRONTEND_RACE_OPTION_Y_POSITIONS[selection_index_]);
  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();
  return 1;
}

void
draw_frontend_calibration(void)
{
  FrontendJoystickCalibrationState *calibration;
  void                             *background_shape;
  void                             *knob_shape;
  int32                             knob_dimensions;
  int32                             x;
  int32                             y;

  begin_runtime_frame();
  synchronize_runtime_frame();
  clear_runtime_display(0);
  background_shape = locate_runtime_shape(g_FRONTEND_CALIBRATION_SHAPES,
                                          (const uint32 *) FRONTEND_RACE_OPTIONS_BACKGROUND_SHAPE);
  blit_runtime_shape_at(background_shape, 0, 0);

  knob_shape  = locate_runtime_shape(g_FRONTEND_CALIBRATION_SHAPES,
                                     (const uint32 *) FRONTEND_CALIBRATION_KNOB_SHAPE);
  calibration = (FrontendJoystickCalibrationState *) g_FRONTEND_JOYSTICK_CALIBRATION_STATE;
  x = calibration->cursor_x * FRONTEND_CALIBRATION_CURSOR_X_SCALE / FRONTEND_CALIBRATION_AXIS_SCALE;
  y = calibration->cursor_y * FRONTEND_CALIBRATION_CURSOR_Y_SCALE / FRONTEND_CALIBRATION_AXIS_SCALE;
  knob_dimensions = ((int32 *) knob_shape)[FRONTEND_RACE_OPTIONS_DIMENSION_WORD];
  x += FRONTEND_CALIBRATION_CURSOR_X_CENTER -
       (knob_dimensions >> FRONTEND_RACE_OPTIONS_DIMENSION_X_SHIFT) /
         FRONTEND_RACE_OPTIONS_DIMENSION_CENTER_DIVISOR;
  y += FRONTEND_CALIBRATION_CURSOR_Y_CENTER -
       ((knob_dimensions << FRONTEND_RACE_OPTIONS_DIMENSION_Y_SHIFT) >>
        FRONTEND_RACE_OPTIONS_DIMENSION_Y_SHIFT) /
         FRONTEND_RACE_OPTIONS_DIMENSION_CENTER_DIVISOR;
  draw_runtime_shape_at(knob_shape, x, y);
  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();
  delay_runtime_frames(FRONTEND_CALIBRATION_DELAY_FRAMES);
}

int32
calibrate_frontend_controls(void)
{
  FrontendJoystickCalibrationState *calibration;
  uint32                            control_bits;

  if(g_FRONTEND_CALIBRATION_SHAPES == NULL)
    {
      g_FRONTEND_CALIBRATION_SHAPES = load_runtime_file_data(FRONTEND_CALIBRATION_SHAPE_PATH, 0);
      track_primary_allocation(g_FRONTEND_CALIBRATION_SHAPES);
    }
  draw_frontend_calibration();
  GetControlPad(FRONTEND_CALIBRATION_CONTROLLER_NUMBER, 0, (ControlPadEventData *) &control_bits);
  while(control_bits != 0)
    {
      delay_runtime_frames(FRONTEND_CALIBRATION_DELAY_FRAMES);
      GetControlPad(FRONTEND_CALIBRATION_CONTROLLER_NUMBER,
                    0,
                    (ControlPadEventData *) &control_bits);
    }

  calibration         = (FrontendJoystickCalibrationState *) g_FRONTEND_JOYSTICK_CALIBRATION_STATE;
  calibration->active = 1;
  do
    {
      draw_frontend_calibration();
      GetControlPad(FRONTEND_CALIBRATION_CONTROLLER_NUMBER,
                    0,
                    (ControlPadEventData *) &control_bits);
      if(control_bits != 0)
        {
          g_FRONTEND_CALIBRATION_SUCCEEDED = 0;
          calibration->active              = 0;
          break;
        }
    }
  while(calibration->active == 1);

  free_primary_memory(g_FRONTEND_CALIBRATION_SHAPES);
  g_FRONTEND_CALIBRATION_SHAPES = NULL;
  return g_FRONTEND_CALIBRATION_SUCCEEDED;
}

static void
_copy_frontend_control_preset(RaceConfiguration *configuration_, int32 preset_index_)
{
  if(preset_index_ < FRONTEND_RACE_OPTIONS_GAMEPAD_PRESET_COUNT)
    {
      copy_runtime_memory(&configuration_->control_bindings,
                          g_FRONTEND_CONTROL_PRESETS.gamepad[preset_index_],
                          FRONTEND_RACE_OPTIONS_PRESET_BYTE_COUNT);
    }
  else
    {
      copy_runtime_memory(&configuration_->control_bindings,
                          g_FRONTEND_CONTROL_PRESETS.joystick,
                          FRONTEND_RACE_OPTIONS_JOYSTICK_NAME_BYTE_COUNT);
      configuration_->control_bindings.start_action     = 0;
      configuration_->control_bindings.auxiliary_action = 0;
    }
}

int32
configure_frontend_race_options(int32 frame_count_, RaceConfiguration *configuration_)
{
  FrontendJoystickCalibrationState *calibration;
  RaceConfiguration working_configuration;
  uint32           *calibration_values;
  uint32            control_bits;
  int32             audio_mode;
  int32             frame_index;
  int32             preset_index;
  int32             selection_index;
  int32             skill_level;

  copy_runtime_memory(&working_configuration, configuration_, RACE_CONFIGURATION_BYTE_COUNT);
  selection_index = FRONTEND_RACE_OPTIONS_SKILL_SELECTION;
  draw_frontend_race_options(selection_index, &working_configuration);

  for(;;)
    {
      control_bits = poll_controller_input(frame_count_ * FRONTEND_RACE_OPTIONS_FRAME_RATE,
                                           1,
                                           FRONTEND_RACE_OPTIONS_INPUT_MASK);
      if(control_bits == 0)
        {
          free_primary_memory(g_FRONTEND_RACE_OPTIONS_SHAPES);
          g_FRONTEND_RACE_OPTIONS_SHAPES = NULL;
          return 0;
        }
      if((control_bits & ControlRightShift) != 0)
        {
          switch(selection_index)
            {
              case FRONTEND_RACE_OPTIONS_SKILL_SELECTION:
                skill_level = ((int32) working_configuration.skill_level + 1) %
                              FRONTEND_RACE_OPTIONS_SKILL_LEVEL_COUNT;
                working_configuration.skill_level = (uint8) skill_level;
                working_configuration.host_commentary_enabled =
                  (uint8) (skill_level < FRONTEND_RACE_OPTIONS_HOST_SKILL_LEVEL_LIMIT);
                break;
              case FRONTEND_RACE_OPTIONS_AUDIO_SELECTION:
                audio_mode = ((int32) working_configuration.music_enabled |
                              ((int32) working_configuration.sound_effects_enabled << 1)) +
                             1;
                audio_mode %= FRONTEND_RACE_OPTIONS_AUDIO_MODE_COUNT;
                working_configuration.music_enabled         = (uint8) (audio_mode & 1);
                working_configuration.sound_effects_enabled = (uint8) (audio_mode >> 1);
                if((audio_mode & 1) == 0)
                  {
                    pause_frontend_audio_stream();
                  }
                else
                  {
                    resume_frontend_audio_stream();
                  }
                break;
              case FRONTEND_RACE_OPTIONS_STEREO_SELECTION:
                working_configuration.stereo_enabled ^= 1;
                if(working_configuration.stereo_enabled == 0)
                  {
                    configure_mono_frontend_audio();
                  }
                else
                  {
                    configure_stereo_frontend_audio();
                  }
                break;
              case FRONTEND_RACE_OPTIONS_HOST_SELECTION:
                working_configuration.host_commentary_enabled ^= 1;
                break;
              case FRONTEND_RACE_OPTIONS_ABS_SELECTION:
                if(g_FRONTEND_VEHICLE_ABS_AVAILABILITY[working_configuration
                                                         .player_vehicle_index] != 0)
                  {
                    working_configuration.anti_lock_brakes_enabled ^= 1;
                  }
                break;
              case FRONTEND_RACE_OPTIONS_TRACTION_CONTROL_SELECTION:
                if(g_FRONTEND_VEHICLE_TRACTION_CONTROL_AVAILABILITY[working_configuration
                                                                      .player_vehicle_index] != 0)
                  {
                    working_configuration.traction_control_enabled ^= 1;
                  }
                break;
              case FRONTEND_RACE_OPTIONS_CONTROL_SELECTION:
                preset_index = ((int32) working_configuration.control_preset_index + 1) %
                               FRONTEND_RACE_OPTIONS_CONTROL_PRESET_COUNT;
                working_configuration.control_preset_index = (uint8) preset_index;
                _copy_frontend_control_preset(&working_configuration, preset_index);
                break;
            }
        }
      if((control_bits & ControlLeftShift) != 0)
        {
          switch(selection_index)
            {
              case FRONTEND_RACE_OPTIONS_SKILL_SELECTION:
                skill_level = (int32) working_configuration.skill_level;
                if(skill_level == 0)
                  {
                    skill_level = FRONTEND_RACE_OPTIONS_SKILL_LEVEL_COUNT - 1;
                  }
                else
                  {
                    skill_level = (skill_level - 1) % FRONTEND_RACE_OPTIONS_SKILL_LEVEL_COUNT;
                  }
                working_configuration.skill_level = (uint8) skill_level;
                working_configuration.host_commentary_enabled =
                  (uint8) (skill_level < FRONTEND_RACE_OPTIONS_HOST_SKILL_LEVEL_LIMIT);
                break;
              case FRONTEND_RACE_OPTIONS_AUDIO_SELECTION:
                audio_mode = (int32) working_configuration.music_enabled |
                             ((int32) working_configuration.sound_effects_enabled << 1);
                if(audio_mode == 0)
                  {
                    audio_mode = FRONTEND_RACE_OPTIONS_AUDIO_MODE_COUNT - 1;
                  }
                else
                  {
                    audio_mode -= 1;
                  }
                working_configuration.music_enabled         = (uint8) (audio_mode & 1);
                working_configuration.sound_effects_enabled = (uint8) ((audio_mode >> 1) & 1);
                if((audio_mode & 1) == 0)
                  {
                    pause_frontend_audio_stream();
                  }
                else
                  {
                    resume_frontend_audio_stream();
                  }
                break;
              case FRONTEND_RACE_OPTIONS_STEREO_SELECTION:
                working_configuration.stereo_enabled ^= 1;
                if(working_configuration.stereo_enabled == 0)
                  {
                    configure_mono_frontend_audio();
                  }
                else
                  {
                    configure_stereo_frontend_audio();
                  }
                break;
              case FRONTEND_RACE_OPTIONS_HOST_SELECTION:
                working_configuration.host_commentary_enabled ^= 1;
                break;
              case FRONTEND_RACE_OPTIONS_ABS_SELECTION:
                if(g_FRONTEND_VEHICLE_ABS_AVAILABILITY[working_configuration
                                                         .player_vehicle_index] != 0)
                  {
                    working_configuration.anti_lock_brakes_enabled ^= 1;
                  }
                break;
              case FRONTEND_RACE_OPTIONS_TRACTION_CONTROL_SELECTION:
                if(g_FRONTEND_VEHICLE_TRACTION_CONTROL_AVAILABILITY[working_configuration
                                                                      .player_vehicle_index] != 0)
                  {
                    working_configuration.traction_control_enabled ^= 1;
                  }
                break;
              case FRONTEND_RACE_OPTIONS_CONTROL_SELECTION:
                preset_index = (int32) working_configuration.control_preset_index;
                if(preset_index == 0)
                  {
                    preset_index = FRONTEND_RACE_OPTIONS_CONTROL_PRESET_COUNT - 1;
                  }
                else
                  {
                    preset_index -= 1;
                  }
                working_configuration.control_preset_index = (uint8) preset_index;
                _copy_frontend_control_preset(&working_configuration, preset_index);
                break;
            }
        }
      if((control_bits & ControlDown) != 0)
        {
          selection_index = (selection_index + 1) % FRONTEND_RACE_OPTIONS_SELECTION_COUNT;
        }
      if((control_bits & ControlUp) != 0)
        {
          if(selection_index == FRONTEND_RACE_OPTIONS_SKILL_SELECTION)
            {
              selection_index = FRONTEND_RACE_OPTIONS_SELECTION_COUNT - 1;
            }
          else
            {
              selection_index -= 1;
            }
        }
      if((control_bits & FRONTEND_RACE_OPTIONS_CONFIRM_MASK) != 0)
        {
          if(selection_index == FRONTEND_RACE_OPTIONS_CONTROL_SELECTION)
            {
              preset_index = working_configuration.control_preset_index;
              if(preset_index == FRONTEND_RACE_OPTIONS_GAMEPAD_PRESET_COUNT)
                {
                  if(calibrate_frontend_controls() == 0)
                    {
                      working_configuration.joystick_calibration_configured = 0;
                      working_configuration.control_preset_index            = 0;
                      _copy_frontend_control_preset(&working_configuration, 0);
                    }
                  else
                    {
                      working_configuration.joystick_calibration_configured = 1;
                      calibration = (FrontendJoystickCalibrationState *)
                        g_FRONTEND_JOYSTICK_CALIBRATION_STATE;
                      calibration_values =
                        (uint32 *) &working_configuration.control_bindings.unassigned;
                      calibration_values[FRONTEND_CALIBRATION_PRIMARY_INDEX] =
                        calibration->primary_action;
                      calibration_values[FRONTEND_CALIBRATION_SECONDARY_INDEX] =
                        calibration->secondary_action;
                      calibration_values[FRONTEND_CALIBRATION_TERTIARY_INDEX] =
                        calibration->tertiary_action;
                      calibration_values[FRONTEND_CALIBRATION_RIGHT_SHIFT_INDEX] =
                        calibration->right_shift_action;
                      calibration_values[FRONTEND_CALIBRATION_LEFT_SHIFT_INDEX] =
                        calibration->left_shift_action;
                      calibration_values[FRONTEND_CALIBRATION_DOWN_LEFT_INDEX] =
                        calibration->down_left_action;
                    }
                }
              else
                {
                  preset_index =
                    select_frontend_control_preset(frame_count_, &working_configuration);
                  working_configuration.control_preset_index = (uint8) preset_index;
                  _copy_frontend_control_preset(&working_configuration, preset_index);
                }
              control_bits = 0;
            }
          else if(selection_index == FRONTEND_RACE_OPTIONS_STATS_SELECTION)
            {
              RACE_CONFIGURATION->music_enabled = working_configuration.music_enabled;
              display_post_race_stats(1);
            }
        }
      if((control_bits & ControlStart) != 0)
        {
          copy_runtime_memory(configuration_,
                              &working_configuration,
                              RACE_CONFIGURATION_BYTE_COUNT);
          free_primary_memory(g_FRONTEND_RACE_OPTIONS_SHAPES);
          g_FRONTEND_RACE_OPTIONS_SHAPES = NULL;
          return 1;
        }
      if((control_bits & ControlX) != 0)
        {
          if(selection_index != FRONTEND_RACE_OPTIONS_SKILL_SELECTION &&
             selection_index != FRONTEND_RACE_OPTIONS_HOST_SELECTION)
            {
              if(RACE_CONFIGURATION->music_enabled == 0)
                {
                  pause_frontend_audio_stream();
                }
              else
                {
                  resume_frontend_audio_stream();
                }
              free_primary_memory(g_FRONTEND_RACE_OPTIONS_SHAPES);
              g_FRONTEND_RACE_OPTIONS_SHAPES = NULL;
              return 0;
            }

          if(selection_index == FRONTEND_RACE_OPTIONS_SKILL_SELECTION)
            {
              for(frame_index = 0; frame_index < FRONTEND_RACE_OPTIONS_HIDDEN_TIMEOUT;
                  frame_index += 1)
                {
                  if(read_stable_controller_input() == FRONTEND_RACE_OPTIONS_HIDDEN_STEP_ONE)
                    {
                      break;
                    }
                  synchronize_runtime_frame();
                }
              if(frame_index < FRONTEND_RACE_OPTIONS_HIDDEN_TIMEOUT)
                {
                  for(frame_index = 0; frame_index < FRONTEND_RACE_OPTIONS_HIDDEN_TIMEOUT;
                      frame_index += 1)
                    {
                      if(read_stable_controller_input() == FRONTEND_RACE_OPTIONS_HIDDEN_STEP_TWO)
                        {
                          break;
                        }
                      synchronize_runtime_frame();
                    }
                }
              if(frame_index < FRONTEND_RACE_OPTIONS_HIDDEN_TIMEOUT)
                {
                  for(frame_index = 0; frame_index < FRONTEND_RACE_OPTIONS_HIDDEN_TIMEOUT;
                      frame_index += 1)
                    {
                      if(read_stable_controller_input() == FRONTEND_RACE_OPTIONS_HIDDEN_STEP_THREE)
                        {
                          working_configuration.skill_level |= FRONTEND_RACE_OPTIONS_HIDDEN_FLAG;
                          break;
                        }
                      synchronize_runtime_frame();
                    }
                }
              if(frame_index == FRONTEND_RACE_OPTIONS_HIDDEN_TIMEOUT)
                {
                  if(RACE_CONFIGURATION->music_enabled == 0)
                    {
                      pause_frontend_audio_stream();
                    }
                  else
                    {
                      resume_frontend_audio_stream();
                    }
                  free_primary_memory(g_FRONTEND_RACE_OPTIONS_SHAPES);
                  g_FRONTEND_RACE_OPTIONS_SHAPES = NULL;
                  return 0;
                }
            }
          else
            {
              for(frame_index = 0; frame_index < FRONTEND_RACE_OPTIONS_HIDDEN_TIMEOUT;
                  frame_index += 1)
                {
                  if(read_stable_controller_input() == FRONTEND_RACE_OPTIONS_HIDDEN_STEP_ONE)
                    {
                      break;
                    }
                  synchronize_runtime_frame();
                }
              if(frame_index < FRONTEND_RACE_OPTIONS_HIDDEN_TIMEOUT)
                {
                  for(frame_index = 0; frame_index < FRONTEND_RACE_OPTIONS_HIDDEN_TIMEOUT;
                      frame_index += 1)
                    {
                      if(read_stable_controller_input() == FRONTEND_RACE_OPTIONS_HIDDEN_STEP_TWO)
                        {
                          break;
                        }
                      synchronize_runtime_frame();
                    }
                }
              if(frame_index < FRONTEND_RACE_OPTIONS_HIDDEN_TIMEOUT)
                {
                  for(frame_index = 0; frame_index < FRONTEND_RACE_OPTIONS_HIDDEN_TIMEOUT;
                      frame_index += 1)
                    {
                      if(read_stable_controller_input() == FRONTEND_RACE_OPTIONS_HIDDEN_STEP_THREE)
                        {
                          play_frontend_movie_sequence();
                          break;
                        }
                      synchronize_runtime_frame();
                    }
                }
              if(frame_index == FRONTEND_RACE_OPTIONS_HIDDEN_TIMEOUT)
                {
                  if(RACE_CONFIGURATION->music_enabled == 0)
                    {
                      pause_frontend_audio_stream();
                    }
                  else
                    {
                      resume_frontend_audio_stream();
                    }
                  free_primary_memory(g_FRONTEND_RACE_OPTIONS_SHAPES);
                  g_FRONTEND_RACE_OPTIONS_SHAPES = NULL;
                  return 0;
                }
            }
        }
      draw_frontend_race_options(selection_index, &working_configuration);
    }
}
