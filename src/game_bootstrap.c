#include "game_bootstrap.h"

#include "bitmap_clip.h"
#include "control_input.h"
#include "control_input_poll_state.h"
#include "display_coordinate_state.h"
#include "display_fade.h"
#include "formatted_output.h"
#include "frontend_overlay.h"
#include "frontend_state.h"
#include "frontend_transition.h"
#include "game_data_path.h"
#include "game_loop.h"
#include "game_session_resources.h"
#include "render_thread_signal.h"
#include "simulation_thread_signal.h"
#include "string_utilities.h"
#include "timer_task.h"
#include "track_data_path.h"
#include "vbl_delay.h"
#include "vehicle_control_input.h"
#include "vehicle_drive_statistics.h"
#include "vehicle_dynamics_state.h"
#include "vehicle_selection_state.h"

#include "audio.h"
#include "graphics.h"
#include "kernel.h"
#include "mem.h"

#include "string.h"

#define STARTUP_TASK_PRIORITY 80
#define STARTUP_INITIAL_SELECTED_VEHICLE_INDEX 4
#define STARTUP_COMMAND_LINE_GAME_MODE 2
#define STARTUP_GAME_MODE_NONE 0
#define STARTUP_GAME_MODE_ONE 1
#define STARTUP_GAME_MODE_TWO 2
#define STARTUP_SPECIAL_SURFACE_BANK_OFFSET 3
#define DISPLAY_FADE_FRAME_COUNT 60
#define STARTUP_RUNTIME_COPY_BYTE_COUNT 1
#define STARTUP_OPTION_VALUE_INDEX 2
#define GAME_MODE_FLAG_DEFAULT 0
#define GAME_MODE_FLAG_ALTERNATE 2
#define GAME_MODE_FLAG_SPECIAL 4
#define GAME_MODE_FLAG_EXTENDED 8
#define GAME_MODE_FLAG_RESTART 16
#define DISPLAY_CONFIGURATION_DEFAULT 2
#define DISPLAY_CONFIGURATION_STANDARD 5
#define DISPLAY_CONFIGURATION_SPECIAL 6
#define DISPLAY_CONFIGURATION_EXTENDED 7

extern uint32
poll_control_input(void);

#define DEFAULT_STARTUP_CONFIGURATION_FLAGS 12
#define DEFAULT_TRACK_INDEX 2
#define DEFAULT_PLAYER_VEHICLE_INDEX 1
#define DEFAULT_OPPONENT_VEHICLE_INDEX 4
#define FRONTEND_OVERLAY_PLAYER_SKILLS_MODE 2

typedef enum StartupControlMaskIndex
{
  STARTUP_CONTROL_ACCELERATE = 0,
  STARTUP_CONTROL_BRAKE,
  STARTUP_CONTROL_AUXILIARY,
  STARTUP_CONTROL_SHIFT_RIGHT,
  STARTUP_CONTROL_SHIFT_LEFT,
  STARTUP_CONTROL_UP_LEFT,
  STARTUP_CONTROL_LEFT,
  STARTUP_CONTROL_DOWN_LEFT,
  STARTUP_CONTROL_UP_RIGHT,
  STARTUP_CONTROL_RIGHT,
  STARTUP_CONTROL_DOWN_RIGHT,
  STARTUP_CONTROL_START,
  STARTUP_CONTROL_EXTRA
} StartupControlMaskIndex;

int32 g_GAME_MODE_INITIALIZED;
int32 g_BOOTSTRAP_RUNTIME_RESERVED_064;
int32 g_BOOTSTRAP_GAME_MODE = STARTUP_GAME_MODE_ONE;

void
initialize_game_mode(int32 display_mode_)
{
  int32 vehicle_number;

  initialize_timer_task();
  g_GAME_MODE_INITIALIZED = 1;
  if(g_FRONTEND_OVERLAY_BYPASS != 0)
    {
      g_STARTUP_CONFIGURATION.configuration_flags           = DEFAULT_STARTUP_CONFIGURATION_FLAGS;
      g_STARTUP_CONFIGURATION.skill_level                   = 0;
      g_STARTUP_CONFIGURATION.track_index                   = DEFAULT_TRACK_INDEX;
      g_STARTUP_CONFIGURATION.player_vehicle_index          = DEFAULT_PLAYER_VEHICLE_INDEX;
      g_STARTUP_CONFIGURATION.opponent_vehicle_index        = DEFAULT_OPPONENT_VEHICLE_INDEX;
      g_STARTUP_CONFIGURATION.music_enabled                 = 1;
      g_STARTUP_CONFIGURATION.sound_effects_enabled         = 1;
      g_STARTUP_CONFIGURATION.stereo_enabled                = 1;
      g_STARTUP_CONFIGURATION.anti_lock_brakes_enabled      = 1;
      g_STARTUP_CONFIGURATION.traction_control_enabled      = 1;
      g_STARTUP_CONFIGURATION.performance_reduction_enabled = 1;
      g_STARTUP_CONFIGURATION.control_preset_index          = 0;
      g_STARTUP_CONFIGURATION.control_button_masks[STARTUP_CONTROL_ACCELERATE]  = ControlA;
      g_STARTUP_CONFIGURATION.control_button_masks[STARTUP_CONTROL_BRAKE]       = ControlB;
      g_STARTUP_CONFIGURATION.control_button_masks[STARTUP_CONTROL_AUXILIARY]   = ControlC;
      g_STARTUP_CONFIGURATION.control_button_masks[STARTUP_CONTROL_SHIFT_RIGHT] = ControlRightShift;
      g_STARTUP_CONFIGURATION.control_button_masks[STARTUP_CONTROL_SHIFT_LEFT]  = ControlLeftShift;
      g_STARTUP_CONFIGURATION.control_button_masks[STARTUP_CONTROL_UP_LEFT] =
        ControlUp | ControlLeft;
      g_STARTUP_CONFIGURATION.control_button_masks[STARTUP_CONTROL_LEFT] = ControlLeft;
      g_STARTUP_CONFIGURATION.control_button_masks[STARTUP_CONTROL_DOWN_LEFT] =
        ControlDown | ControlLeft;
      g_STARTUP_CONFIGURATION.control_button_masks[STARTUP_CONTROL_UP_RIGHT] =
        ControlUp | ControlRight;
      g_STARTUP_CONFIGURATION.control_button_masks[STARTUP_CONTROL_RIGHT] = ControlRight;
      g_STARTUP_CONFIGURATION.control_button_masks[STARTUP_CONTROL_DOWN_RIGHT] =
        ControlDown | ControlRight;
      g_STARTUP_CONFIGURATION.control_button_masks[STARTUP_CONTROL_START] = ControlStart;
      g_STARTUP_CONFIGURATION.control_button_masks[STARTUP_CONTROL_EXTRA] = ControlX;

      vehicle_number = g_EVENT_INDEX + 1;
      g_EVENT_INDEX  = vehicle_number;
      sprintf(g_STARTUP_ARGUMENT_BUFFER, "Cy%d", vehicle_number);
      g_STARTUP_ARGUMENT = (char *) g_STARTUP_ARGUMENT_BUFFER;
      return;
    }

  if(((uint8 *) &g_VEHICLE_ACCELERATE_BUTTON_MASK)[0] == 'p')
    display_mode_ |= FRONTEND_OVERLAY_PLAYER_SKILLS_MODE;
  run_frontend_overlay(display_mode_);
  if(g_FRONTEND_OVERLAY_RELOAD_REQUESTED != 0)
    run_frontend_overlay(display_mode_);
}

static int32
_parse_startup_arguments(int32 parse_extended_, int argc_, char **argv_)
{
  int32 argument_index;
  char *argument;

  for(argument_index = 1; argument_index < argc_; argument_index++)
    {
      argument = argv_[argument_index];
      if(argument[0] == '-')
        {
          switch(argument[1])
            {
              case 'a':
                g_ANTI_LOCK_BRAKES_ENABLED = 0;
                break;
              case 'b':
                if(parse_extended_ != 0)
                  g_STARTUP_BUTTON_MODE = 1;
                break;
              case 'c':
                g_PLAYER_VEHICLE_INDEX = argument[STARTUP_OPTION_VALUE_INDEX] - '0';
                break;
              case 'd':
                if(parse_extended_ != 0)
                  g_STARTUP_DEBUG_MODE = 1;
                break;
              case 'e':
                if(parse_extended_ != 0)
                  g_STARTUP_EVENT_MODE = 1;
                break;
              case 'h':
                if(parse_extended_ != 0)
                  g_STARTUP_HELP_MODE = 1;
                break;
              case 'r':
                g_VEHICLE_DEFAULT_SURFACE_INDEX = argument[STARTUP_OPTION_VALUE_INDEX] - '0';
                g_VEHICLE_SURFACE_INDEX         = g_VEHICLE_DEFAULT_SURFACE_INDEX;
                break;
              case 't':
                g_TRACTION_CONTROL_ENABLED = 0;
                break;
            }
        }
      else if(argument[0] == '?')
        return 0;
    }
  return 1;
}

int
main(int argc_, char **argv_)
{
  int32 game_result;

  memcpy(g_GAME_DATA_PATH, "\0", STARTUP_RUNTIME_COPY_BYTE_COUNT);
  SetItemPri(KernelBase->kb_CurrentTask->t.n_Item, STARTUP_TASK_PRIORITY);
  g_ANTI_LOCK_BRAKES_ENABLED      = 0;
  g_TRACTION_CONTROL_ENABLED      = 0;
  g_PLAYER_VEHICLE_INDEX          = STARTUP_INITIAL_SELECTED_VEHICLE_INDEX;
  g_VEHICLE_DEFAULT_SURFACE_INDEX = 0;
  g_VEHICLE_SURFACE_INDEX         = 0;
  if(_parse_startup_arguments(0, argc_, argv_) == 0)
    return 0;
  if(argc_ > 1)
    g_BOOTSTRAP_GAME_MODE = STARTUP_COMMAND_LINE_GAME_MODE;
  if(initialize_bitmap_display_state() == 0)
    {
      fade_display_to_black(&g_BITMAP_DISPLAY_STATE, DISPLAY_FADE_FRAME_COUNT);
      CloseAudioFolio();
      return 0;
    }
  g_VBL_TIMER_IO_REQUEST = GetVBLIOReq();
  initialize_control_input_system();
  for(;;)
    {
      (void) reset_control_input_poll_state();
      SetItemPri(KernelBase->kb_CurrentTask->t.n_Item, STARTUP_TASK_PRIORITY);
      enable_control_input_thread();
      ScavengeMem();
      initialize_game_mode(g_BOOTSTRAP_GAME_MODE);
      initialize_joystick_calibration_state();
      if(_parse_startup_arguments(1, argc_, argv_) == 0)
        return 0;
      ScavengeMem();
      if(argc_ > 1)
        g_STARTUP_ARGUMENT = argv_[1];
      /* The retail bootstrap mode and active-player index share one word. */
      if(g_ACTIVE_PLAYER_INDEX < STARTUP_GAME_MODE_ONE)
        {
          if(g_ACTIVE_PLAYER_INDEX == STARTUP_GAME_MODE_NONE)
            {
              ScavengeMem();
              prepare_default_game_mode();
              ScavengeMem();
            }
          g_ANTI_LOCK_BRAKES_ENABLED = g_STARTUP_CONFIGURATION.anti_lock_brakes_enabled;
          g_TRACTION_CONTROL_ENABLED = g_STARTUP_CONFIGURATION.traction_control_enabled;
          set_track_data_path(g_STARTUP_ARGUMENT);
          ScavengeMem();
          g_VEHICLE_SURFACE_BANK_OFFSET = 0;
          if((compare_strings((const uint8 *) g_STARTUP_ARGUMENT, (const uint8 *) "3la") == 0) ||
             (compare_strings((const uint8 *) g_STARTUP_ARGUMENT, (const uint8 *) "3La") == 0) ||
             (compare_strings((const uint8 *) g_STARTUP_ARGUMENT, (const uint8 *) "3LA") == 0))
            g_VEHICLE_SURFACE_BANK_OFFSET = STARTUP_SPECIAL_SURFACE_BANK_OFFSET;
          g_RENDER_THREAD_STATE.pipeline_initialized = 0;
          poll_control_input();
          initialize_game_input();
          ScavengeMem();
        }
      else
        {
          request_game_audio_initialization();
          g_GAME_RUNTIME_STATE.flags_18 = 0;
          if(g_ACTIVE_PLAYER_INDEX == STARTUP_GAME_MODE_ONE)
            configure_frontend_display(1, 1);
          if(g_ACTIVE_PLAYER_INDEX == STARTUP_GAME_MODE_TWO)
            configure_frontend_display(STARTUP_GAME_MODE_ONE, STARTUP_GAME_MODE_TWO);
        }
      initialize_game_session();
      ScavengeMem();
      set_frontend_state(0, 1, 0, 0);
      start_render_thread();
      ScavengeMem();
      if(g_ACTIVE_PLAYER_INDEX == STARTUP_GAME_MODE_ONE)
        g_STARTUP_RENDER_FLAG = 0;
      start_game_rendering();
      ScavengeMem();
      game_result = run_game_loop();
      ScavengeMem();
      stop_game_rendering();
      ScavengeMem();
      request_game_audio_shutdown();
      ScavengeMem();
      ScavengeMem();
      finish_game_session();
      ScavengeMem();
      enable_control_input_thread();
      ScavengeMem();
      finalize_game_mode(game_result);
      ScavengeMem();
    }
}

void
finalize_game_mode(int32 result_)
{
  (void) result_;
  g_FRONTEND_OVERLAY_MODE = DISPLAY_CONFIGURATION_DEFAULT;
  switch(g_GAME_RUNTIME_STATE.flags_14)
    {
      case GAME_MODE_FLAG_DEFAULT:
      case GAME_MODE_FLAG_ALTERNATE:
      case GAME_MODE_FLAG_RESTART:
        g_FRONTEND_OVERLAY_MODE = DISPLAY_CONFIGURATION_STANDARD;
        break;

      case GAME_MODE_FLAG_SPECIAL:
        g_FRONTEND_OVERLAY_MODE = DISPLAY_CONFIGURATION_SPECIAL;
        break;

      case GAME_MODE_FLAG_EXTENDED:
        g_FRONTEND_OVERLAY_MODE = DISPLAY_CONFIGURATION_EXTENDED;
        break;

      default:
        break;
    }
}
