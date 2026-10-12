#include "frontend_skill_menu.h"

#include "controller_input.h"
#include "frontend_frame.h"
#include "frontend_skill_submenus.h"
#include "frontend_sound.h"
#include "nvram_save.h"
#include "random_number.h"
#include "runtime_memory.h"
#include "runtime_memory_ops.h"
#include "tracked_memory.h"

#include "event.h"

#define FRONTEND_SKILL_CONTROL_SHAPE_PATH "frontend/display/control.3sh"
#define FRONTEND_SKILL_CAR_SHAPE_PATH "frontend/display/ctrlcars.3sh"
#define FRONTEND_SKILL_HOST_SHAPE_PATH "frontend/display/hosts/hosti"
#define FRONTEND_SKILL_BACKGROUND_SHAPE "bgnd"
#define FRONTEND_SKILL_KEYS_SHAPE "keys"
#define FRONTEND_SKILL_KEYS_SELECTOR_SHAPE "keyb"
#define FRONTEND_SKILL_OPTIONS_SHAPE "optn"
#define FRONTEND_SKILL_OPTIONS_SELECTOR_SHAPE "optb"
#define FRONTEND_SKILL_TRACK_SELECTOR_SHAPE "trkb"
#define FRONTEND_SKILL_SINGLE_RACE_SHAPE "xsht"
#define FRONTEND_SKILL_TWO_PLAYER_RACE_SHAPE "xman"
#define FRONTEND_SKILL_CLOCK_RACE_SHAPE "clok"
#define FRONTEND_SKILL_OPPONENT_SELECTOR_SHAPE "oppb"
#define FRONTEND_SKILL_CLOCK_ROAD_SHAPE "rod1"
#define FRONTEND_SKILL_CLOCK_FRAME_SHAPE "frm1"
#define FRONTEND_SKILL_OPPONENT_ROAD_SHAPE "rod2"
#define FRONTEND_SKILL_OPPONENT_FRAME_SHAPE "frm2"
#define FRONTEND_SKILL_CLOCK_VEHICLE_SELECTOR_SHAPE "brd1"
#define FRONTEND_SKILL_OPPONENT_VEHICLE_SELECTOR_SHAPE "brd2"
#define FRONTEND_SKILL_HOST_BACKGROUND_SHAPE "bgnd"
#define FRONTEND_SKILL_HOST_CUE_FORMAT "101.%d"
#define FRONTEND_SKILL_HOST_CUE_BYTE_COUNT 12
#define FRONTEND_SKILL_HOST_CHOICE_COUNT 3
#define FRONTEND_SKILL_HOST_CHOICE_BASE 1
#define FRONTEND_SKILL_POSITION_WORD 3
#define FRONTEND_SKILL_POSITION_X_SHIFT 16
#define FRONTEND_SKILL_SELECTION_COUNT 5
#define FRONTEND_SKILL_ACCEPT_SELECTION 0
#define FRONTEND_SKILL_VEHICLE_SELECTION 1
#define FRONTEND_SKILL_TRACK_SELECTION 2
#define FRONTEND_SKILL_OPPONENT_SELECTION 3
#define FRONTEND_SKILL_OPTIONS_SELECTION 4
#define FRONTEND_SKILL_SELECTABLE_TRACK_COUNT 3
#define FRONTEND_SKILL_TRACK_COUNT 4
#define FRONTEND_SKILL_VEHICLE_COUNT 8
#define FRONTEND_SKILL_VEHICLE_INDEX_MASK 7U
#define FRONTEND_SKILL_OPPONENT_COUNT 9
#define FRONTEND_SKILL_NO_OPPONENT 8
#define FRONTEND_SKILL_SHAPE_NAME_BYTES 8
#define FRONTEND_SKILL_INPUT_POLL_FRAMES 60
#define FRONTEND_SKILL_CONFIRM_INPUT_MASK (ControlA | ControlB | ControlC)

typedef struct FrontendSkillShapeNameStorage
{
  char track[FRONTEND_SKILL_TRACK_COUNT][FRONTEND_SKILL_SHAPE_NAME_BYTES];
  char vehicle_title[FRONTEND_SKILL_VEHICLE_COUNT][FRONTEND_SKILL_SHAPE_NAME_BYTES];
  char vehicle[FRONTEND_SKILL_VEHICLE_COUNT][FRONTEND_SKILL_SHAPE_NAME_BYTES];
  char opponent[FRONTEND_SKILL_VEHICLE_COUNT][FRONTEND_SKILL_SHAPE_NAME_BYTES];
} FrontendSkillShapeNameStorage;

static const FrontendSkillShapeNameStorage G_FRONTEND_SKILL_SHAPE_NAME_STORAGE = {
  { "alpn", "cstl", "city", "test" },
  { "spra", "dblo", "p911", "cvet", "f512", "vipr", "ansx", "mrx7" },
  { "sprf", "dblf", "911f", "vetf", "512f", "vipf", "nsxf", "rx7f" },
  { "sprb", "dblb", "911b", "vetb", "512b", "vipb", "nsxb", "rx7b" }
};

const char *g_FRONTEND_SKILL_TRACK_SHAPE_NAMES[FRONTEND_SKILL_TRACK_COUNT] = {
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.track[0],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.track[1],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.track[2],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.track[3]
};
const char *g_FRONTEND_SKILL_VEHICLE_TITLE_SHAPE_NAMES[FRONTEND_SKILL_VEHICLE_COUNT] = {
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle_title[0],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle_title[1],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle_title[2],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle_title[3],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle_title[4],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle_title[5],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle_title[6],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle_title[7]
};
const char *g_FRONTEND_SKILL_VEHICLE_SHAPE_NAMES[FRONTEND_SKILL_VEHICLE_COUNT] = {
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle[0], G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle[1],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle[2], G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle[3],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle[4], G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle[5],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle[6], G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.vehicle[7]
};
const char *g_FRONTEND_SKILL_OPPONENT_SHAPE_NAMES[FRONTEND_SKILL_OPPONENT_COUNT] = {
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.opponent[0],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.opponent[1],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.opponent[2],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.opponent[3],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.opponent[4],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.opponent[5],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.opponent[6],
  G_FRONTEND_SKILL_SHAPE_NAME_STORAGE.opponent[7],
  NULL
};
RaceConfiguration g_FRONTEND_SKILL_CONFIGURATION;
void             *g_FRONTEND_SKILL_CONTROL_SHAPES;
void             *g_FRONTEND_SKILL_CAR_SHAPES;
int32             g_FRONTEND_SKILL_INITIAL_SELECTION                    = -1;
int32 g_FRONTEND_SKILL_DOWN_SELECTIONS[FRONTEND_SKILL_SELECTION_COUNT]  = { 1, 0, 4, 0, 2 };
int32 g_FRONTEND_SKILL_UP_SELECTIONS[FRONTEND_SKILL_SELECTION_COUNT]    = { 1, 0, 4, 1, 2 };
int32 g_FRONTEND_SKILL_LEFT_SELECTIONS[FRONTEND_SKILL_SELECTION_COUNT]  = { 3, 2, 1, 4, 0 };
int32 g_FRONTEND_SKILL_RIGHT_SELECTIONS[FRONTEND_SKILL_SELECTION_COUNT] = { 4, 2, 1, 0, 3 };
int32 g_FRONTEND_VEHICLE_ABS_AVAILABILITY[FRONTEND_SKILL_VEHICLE_COUNT] = {
  1, 0, 1, 1, 1, 0, 1, 1
};

typedef char
  FrontendSkillTrackShapeNamesSizeCheck[sizeof(g_FRONTEND_SKILL_TRACK_SHAPE_NAMES) == 16 ? 1 : -1];
typedef char FrontendSkillVehicleTitleShapeNamesSizeCheck
  [sizeof(g_FRONTEND_SKILL_VEHICLE_TITLE_SHAPE_NAMES) == 32 ? 1 : -1];
typedef char
  FrontendSkillVehicleShapeNamesSizeCheck[sizeof(g_FRONTEND_SKILL_VEHICLE_SHAPE_NAMES) == 32 ? 1 :
                                                                                               -1];
typedef char FrontendSkillOpponentShapeNamesSizeCheck
  [sizeof(g_FRONTEND_SKILL_OPPONENT_SHAPE_NAMES) == 36 ? 1 : -1];
typedef char
  FrontendSkillDownSelectionsSizeCheck[sizeof(g_FRONTEND_SKILL_DOWN_SELECTIONS) == 20 ? 1 : -1];
typedef char
  FrontendSkillUpSelectionsSizeCheck[sizeof(g_FRONTEND_SKILL_UP_SELECTIONS) == 20 ? 1 : -1];
typedef char
  FrontendSkillLeftSelectionsSizeCheck[sizeof(g_FRONTEND_SKILL_LEFT_SELECTIONS) == 20 ? 1 : -1];
typedef char
  FrontendSkillRightSelectionsSizeCheck[sizeof(g_FRONTEND_SKILL_RIGHT_SELECTIONS) == 20 ? 1 : -1];
typedef char
  FrontendVehicleAbsAvailabilitySizeCheck[sizeof(g_FRONTEND_VEHICLE_ABS_AVAILABILITY) == 32 ? 1 :
                                                                                              -1];

int32
draw_frontend_skill_menu(int32 selection_index_)
{
  const char *race_shape_name;
  const char *vehicle_selector_shape_name;
  const char *opponent_shape_name;
  void       *shape;
  int32       position;

  begin_runtime_frame();
  clear_runtime_display(0);
  if(g_FRONTEND_SKILL_CONTROL_SHAPES == NULL)
    {
      g_FRONTEND_SKILL_CONTROL_SHAPES =
        load_runtime_shape_collection(FRONTEND_SKILL_CONTROL_SHAPE_PATH, 0);
      track_primary_allocation(g_FRONTEND_SKILL_CONTROL_SHAPES);
    }
  if(g_FRONTEND_SKILL_CAR_SHAPES == NULL)
    {
      g_FRONTEND_SKILL_CAR_SHAPES = load_runtime_shape_collection(FRONTEND_SKILL_CAR_SHAPE_PATH, 0);
      track_primary_allocation(g_FRONTEND_SKILL_CAR_SHAPES);
    }

  shape    = locate_runtime_shape(g_FRONTEND_SKILL_CONTROL_SHAPES,
                                  (const uint32 *) FRONTEND_SKILL_BACKGROUND_SHAPE);
  position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
  blit_runtime_shape_at_alias(shape,
                              (position >> FRONTEND_SKILL_POSITION_X_SHIFT),
                              (int16) position);
  shape    = locate_runtime_shape(g_FRONTEND_SKILL_CONTROL_SHAPES,
                                  (const uint32 *) FRONTEND_SKILL_KEYS_SHAPE);
  position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_SKILL_POSITION_X_SHIFT, (int16) position);
  if(selection_index_ == FRONTEND_SKILL_ACCEPT_SELECTION)
    {
      shape    = locate_runtime_shape(g_FRONTEND_SKILL_CONTROL_SHAPES,
                                      (const uint32 *) FRONTEND_SKILL_KEYS_SELECTOR_SHAPE);
      position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
      draw_runtime_shape_at(shape, position >> FRONTEND_SKILL_POSITION_X_SHIFT, (int16) position);
    }
  shape    = locate_runtime_shape(g_FRONTEND_SKILL_CONTROL_SHAPES,
                                  (const uint32 *) FRONTEND_SKILL_OPTIONS_SHAPE);
  position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_SKILL_POSITION_X_SHIFT, (int16) position);
  if(selection_index_ == FRONTEND_SKILL_OPTIONS_SELECTION)
    {
      shape    = locate_runtime_shape(g_FRONTEND_SKILL_CONTROL_SHAPES,
                                      (const uint32 *) FRONTEND_SKILL_OPTIONS_SELECTOR_SHAPE);
      position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
      draw_runtime_shape_at(shape, position >> FRONTEND_SKILL_POSITION_X_SHIFT, (int16) position);
    }
  shape = locate_runtime_shape(
    g_FRONTEND_SKILL_CONTROL_SHAPES,
    (const uint32 *)
      g_FRONTEND_SKILL_TRACK_SHAPE_NAMES[g_FRONTEND_SKILL_CONFIGURATION.track_index]);
  position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_SKILL_POSITION_X_SHIFT, (int16) position);
  if(selection_index_ == FRONTEND_SKILL_TRACK_SELECTION)
    {
      shape    = locate_runtime_shape(g_FRONTEND_SKILL_CONTROL_SHAPES,
                                      (const uint32 *) FRONTEND_SKILL_TRACK_SELECTOR_SHAPE);
      position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
      draw_runtime_shape_at(shape, position >> FRONTEND_SKILL_POSITION_X_SHIFT, (int16) position);
    }

  if(g_FRONTEND_SKILL_CONFIGURATION.opponent_vehicle_index == FRONTEND_SKILL_NO_OPPONENT)
    {
      race_shape_name = FRONTEND_SKILL_CLOCK_RACE_SHAPE;
    }
  else if(g_FRONTEND_SKILL_CONFIGURATION.host_commentary_enabled == 0)
    {
      race_shape_name = FRONTEND_SKILL_SINGLE_RACE_SHAPE;
    }
  else
    {
      race_shape_name = FRONTEND_SKILL_TWO_PLAYER_RACE_SHAPE;
    }
  shape = locate_runtime_shape(g_FRONTEND_SKILL_CONTROL_SHAPES, (const uint32 *) race_shape_name);
  position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_SKILL_POSITION_X_SHIFT, (int16) position);
  if(selection_index_ == FRONTEND_SKILL_OPPONENT_SELECTION)
    {
      shape    = locate_runtime_shape(g_FRONTEND_SKILL_CONTROL_SHAPES,
                                      (const uint32 *) FRONTEND_SKILL_OPPONENT_SELECTOR_SHAPE);
      position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
      draw_runtime_shape_at(shape, position >> FRONTEND_SKILL_POSITION_X_SHIFT, (int16) position);
    }

  if(g_FRONTEND_SKILL_CONFIGURATION.opponent_vehicle_index == FRONTEND_SKILL_NO_OPPONENT)
    {
      shape    = locate_runtime_shape(g_FRONTEND_SKILL_CAR_SHAPES,
                                      (const uint32 *) FRONTEND_SKILL_CLOCK_ROAD_SHAPE);
      position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
      draw_runtime_shape_at(shape, position >> FRONTEND_SKILL_POSITION_X_SHIFT, (int16) position);
      shape    = locate_runtime_shape(g_FRONTEND_SKILL_CAR_SHAPES,
                                      (const uint32 *) FRONTEND_SKILL_CLOCK_FRAME_SHAPE);
      position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
      draw_runtime_shape_at(shape, position >> FRONTEND_SKILL_POSITION_X_SHIFT, (int16) position);
    }
  else
    {
      shape    = locate_runtime_shape(g_FRONTEND_SKILL_CAR_SHAPES,
                                      (const uint32 *) FRONTEND_SKILL_OPPONENT_ROAD_SHAPE);
      position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
      draw_runtime_shape_at(shape, position >> FRONTEND_SKILL_POSITION_X_SHIFT, (int16) position);
      shape    = locate_runtime_shape(g_FRONTEND_SKILL_CAR_SHAPES,
                                      (const uint32 *) FRONTEND_SKILL_OPPONENT_FRAME_SHAPE);
      position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
      draw_runtime_shape_at(shape, position >> FRONTEND_SKILL_POSITION_X_SHIFT, (int16) position);
    }

  opponent_shape_name =
    g_FRONTEND_SKILL_OPPONENT_SHAPE_NAMES[g_FRONTEND_SKILL_CONFIGURATION.opponent_vehicle_index];
  if(opponent_shape_name != NULL)
    {
      shape =
        locate_runtime_shape(g_FRONTEND_SKILL_CAR_SHAPES, (const uint32 *) opponent_shape_name);
      position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
      draw_runtime_shape_at(shape, position >> FRONTEND_SKILL_POSITION_X_SHIFT, (int16) position);
    }
  shape = locate_runtime_shape(
    g_FRONTEND_SKILL_CAR_SHAPES,
    (const uint32 *)
      g_FRONTEND_SKILL_VEHICLE_SHAPE_NAMES[g_FRONTEND_SKILL_CONFIGURATION.player_vehicle_index]);
  position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_SKILL_POSITION_X_SHIFT, (int16) position);
  if(selection_index_ == FRONTEND_SKILL_VEHICLE_SELECTION)
    {
      if(g_FRONTEND_SKILL_CONFIGURATION.opponent_vehicle_index == FRONTEND_SKILL_NO_OPPONENT)
        {
          vehicle_selector_shape_name = FRONTEND_SKILL_CLOCK_VEHICLE_SELECTOR_SHAPE;
        }
      else
        {
          vehicle_selector_shape_name = FRONTEND_SKILL_OPPONENT_VEHICLE_SELECTOR_SHAPE;
        }
      shape    = locate_runtime_shape(g_FRONTEND_SKILL_CAR_SHAPES,
                                      (const uint32 *) vehicle_selector_shape_name);
      position = ((int32 *) shape)[FRONTEND_SKILL_POSITION_WORD];
      draw_runtime_shape_at(shape, position >> FRONTEND_SKILL_POSITION_X_SHIFT, (int16) position);
    }

  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();
  return 1;
}

int32
run_frontend_skill_menu(int32 frame_count_)
{
  char               host_cue[FRONTEND_SKILL_HOST_CUE_BYTE_COUNT];
  RaceConfiguration *skill_configuration;
  uint32             control_bits;
  int32              elapsed_frames;
  int32              selection_index;

  skill_configuration = &g_FRONTEND_SKILL_CONFIGURATION;
  copy_runtime_memory(skill_configuration, RACE_CONFIGURATION, RACE_CONFIGURATION_BYTE_COUNT);
  if(skill_configuration->track_index >= FRONTEND_SKILL_SELECTABLE_TRACK_COUNT)
    {
      skill_configuration->track_index = FRONTEND_SKILL_SELECTABLE_TRACK_COUNT - 1;
    }
  selection_index = FRONTEND_SKILL_ACCEPT_SELECTION;
  draw_frontend_skill_menu(selection_index);

  elapsed_frames = 0;
  while(elapsed_frames < frame_count_)
    {
      control_bits = poll_controller_input(FRONTEND_SKILL_INPUT_POLL_FRAMES, 0, 0);
      if(control_bits != 0)
        {
          elapsed_frames = 0;
        }
      if((control_bits & ControlRightShift) != 0)
        {
          switch(selection_index)
            {
              case FRONTEND_SKILL_VEHICLE_SELECTION:
                play_navigation_sound();
                skill_configuration->player_vehicle_index =
                  (uint8) (((uint32) skill_configuration->player_vehicle_index + 1U) &
                           FRONTEND_SKILL_VEHICLE_INDEX_MASK);
                skill_configuration->anti_lock_brakes_enabled = (uint8)
                  g_FRONTEND_VEHICLE_ABS_AVAILABILITY[skill_configuration->player_vehicle_index];
                skill_configuration->traction_control_enabled = 0;
                break;

              case FRONTEND_SKILL_TRACK_SELECTION:
                play_navigation_sound();
                skill_configuration->track_index = (uint8) ((skill_configuration->track_index + 1) %
                                                            FRONTEND_SKILL_SELECTABLE_TRACK_COUNT);
                skill_configuration->event_index = 0;
                break;

              case FRONTEND_SKILL_OPPONENT_SELECTION:
                play_navigation_sound();
                skill_configuration->opponent_vehicle_index =
                  (uint8) ((skill_configuration->opponent_vehicle_index + 1) %
                           FRONTEND_SKILL_OPPONENT_COUNT);
                break;

              default:
                play_error_sound();
                break;
            }
        }
      if((control_bits & ControlLeftShift) != 0)
        {
          switch(selection_index)
            {
              case FRONTEND_SKILL_VEHICLE_SELECTION:
                play_navigation_sound();
                if(skill_configuration->player_vehicle_index == 0)
                  {
                    skill_configuration->player_vehicle_index = FRONTEND_SKILL_VEHICLE_COUNT - 1;
                  }
                else
                  {
                    skill_configuration->player_vehicle_index -= 1;
                  }
                skill_configuration->anti_lock_brakes_enabled = (uint8)
                  g_FRONTEND_VEHICLE_ABS_AVAILABILITY[skill_configuration->player_vehicle_index];
                skill_configuration->traction_control_enabled = 0;
                break;

              case FRONTEND_SKILL_TRACK_SELECTION:
                play_navigation_sound();
                if(skill_configuration->track_index == 0)
                  {
                    skill_configuration->track_index = FRONTEND_SKILL_SELECTABLE_TRACK_COUNT - 1;
                  }
                else
                  {
                    skill_configuration->track_index -= 1;
                  }
                skill_configuration->event_index = 0;
                break;

              case FRONTEND_SKILL_OPPONENT_SELECTION:
                play_navigation_sound();
                if(skill_configuration->opponent_vehicle_index == 0)
                  {
                    skill_configuration->opponent_vehicle_index = FRONTEND_SKILL_OPPONENT_COUNT - 1;
                  }
                else
                  {
                    skill_configuration->opponent_vehicle_index -= 1;
                  }
                break;

              default:
                play_error_sound();
                break;
            }
        }
      if((control_bits & ControlDown) != 0)
        {
          play_navigation_sound();
          selection_index = g_FRONTEND_SKILL_DOWN_SELECTIONS[selection_index];
        }
      if((control_bits & ControlUp) != 0)
        {
          play_navigation_sound();
          selection_index = g_FRONTEND_SKILL_UP_SELECTIONS[selection_index];
        }
      if((control_bits & ControlLeft) != 0)
        {
          play_navigation_sound();
          selection_index = g_FRONTEND_SKILL_LEFT_SELECTIONS[selection_index];
        }
      if((control_bits & ControlRight) != 0)
        {
          play_navigation_sound();
          selection_index = g_FRONTEND_SKILL_RIGHT_SELECTIONS[selection_index];
        }
      if((control_bits & FRONTEND_SKILL_CONFIRM_INPUT_MASK) != 0)
        {
          control_bits = 0;
          switch(selection_index)
            {
              case FRONTEND_SKILL_ACCEPT_SELECTION:
                play_confirmation_sound();
                copy_runtime_memory(RACE_CONFIGURATION,
                                    skill_configuration,
                                    RACE_CONFIGURATION_BYTE_COUNT);
                save_frontend_nvram();
                free_primary_memory(g_FRONTEND_SKILL_CONTROL_SHAPES);
                free_primary_memory(g_FRONTEND_SKILL_CAR_SHAPES);
                g_FRONTEND_SKILL_CONTROL_SHAPES = NULL;
                g_FRONTEND_SKILL_CAR_SHAPES     = NULL;
                return 1;

              case FRONTEND_SKILL_VEHICLE_SELECTION:
                play_navigation_sound();
                free_primary_memory(g_FRONTEND_SKILL_CONTROL_SHAPES);
                free_primary_memory(g_FRONTEND_SKILL_CAR_SHAPES);
                g_FRONTEND_SKILL_CONTROL_SHAPES = NULL;
                g_FRONTEND_SKILL_CAR_SHAPES     = NULL;
                skill_configuration->player_vehicle_index =
                  select_frontend_vehicle(frame_count_, skill_configuration->player_vehicle_index);
                skill_configuration->anti_lock_brakes_enabled = (uint8)
                  g_FRONTEND_VEHICLE_ABS_AVAILABILITY[skill_configuration->player_vehicle_index];
                skill_configuration->traction_control_enabled = 0;
                break;

              case FRONTEND_SKILL_TRACK_SELECTION:
                play_navigation_sound();
                free_primary_memory(g_FRONTEND_SKILL_CONTROL_SHAPES);
                free_primary_memory(g_FRONTEND_SKILL_CAR_SHAPES);
                g_FRONTEND_SKILL_CONTROL_SHAPES = NULL;
                g_FRONTEND_SKILL_CAR_SHAPES     = NULL;
                skill_configuration->track_index =
                  select_frontend_track(frame_count_, skill_configuration->track_index);
                break;

              case FRONTEND_SKILL_OPPONENT_SELECTION:
                play_navigation_sound();
                free_primary_memory(g_FRONTEND_SKILL_CONTROL_SHAPES);
                free_primary_memory(g_FRONTEND_SKILL_CAR_SHAPES);
                g_FRONTEND_SKILL_CONTROL_SHAPES = NULL;
                g_FRONTEND_SKILL_CAR_SHAPES     = NULL;
                runtime_format_string(host_cue,
                                      FRONTEND_SKILL_HOST_CUE_FORMAT,
                                      random_below(FRONTEND_SKILL_HOST_CHOICE_COUNT) +
                                        FRONTEND_SKILL_HOST_CHOICE_BASE);
                present_frontend_host(host_cue,
                                      present_frontend_attract_frame,
                                      FRONTEND_SKILL_HOST_SHAPE_PATH,
                                      FRONTEND_SKILL_HOST_BACKGROUND_SHAPE,
                                      1,
                                      0);
                break;

              case FRONTEND_SKILL_OPTIONS_SELECTION:
                play_navigation_sound();
                free_primary_memory(g_FRONTEND_SKILL_CONTROL_SHAPES);
                free_primary_memory(g_FRONTEND_SKILL_CAR_SHAPES);
                g_FRONTEND_SKILL_CONTROL_SHAPES = NULL;
                g_FRONTEND_SKILL_CAR_SHAPES     = NULL;
                configure_frontend_race_options(frame_count_, skill_configuration);
                copy_runtime_memory(RACE_CONFIGURATION,
                                    skill_configuration,
                                    RACE_CONFIGURATION_BYTE_COUNT);
                save_frontend_nvram();
                break;

              default:
                break;
            }
        }
      if((control_bits & ControlStart) != 0)
        {
          play_confirmation_sound();
          copy_runtime_memory(RACE_CONFIGURATION,
                              skill_configuration,
                              RACE_CONFIGURATION_BYTE_COUNT);
          save_frontend_nvram();
          free_primary_memory(g_FRONTEND_SKILL_CONTROL_SHAPES);
          free_primary_memory(g_FRONTEND_SKILL_CAR_SHAPES);
          g_FRONTEND_SKILL_CONTROL_SHAPES = NULL;
          g_FRONTEND_SKILL_CAR_SHAPES     = NULL;
          return 1;
        }
      if((control_bits & ControlX) != 0)
        {
          if(selection_index == FRONTEND_SKILL_VEHICLE_SELECTION)
            {
              play_frontend_sound(skill_configuration->player_vehicle_index);
              while((control_bits & ControlX) != 0)
                {
                  synchronize_runtime_frame();
                  control_bits = read_stable_controller_input();
                }
              stop_frontend_sound();
            }
          else
            {
              play_error_sound();
            }
        }
      draw_frontend_skill_menu(selection_index);
      elapsed_frames += 1;
    }

  free_primary_memory(g_FRONTEND_SKILL_CONTROL_SHAPES);
  free_primary_memory(g_FRONTEND_SKILL_CAR_SHAPES);
  g_FRONTEND_SKILL_CONTROL_SHAPES = NULL;
  g_FRONTEND_SKILL_CAR_SHAPES     = NULL;
  return 0;
}
