#include "controller_input.h"
#include "frontend_frame.h"
#include "frontend_race_options.h"
#include "frontend_tween_registry.h"
#include "runtime_memory.h"
#include "tracked_memory.h"

#include "event.h"

#define FRONTEND_CONTROL_SHAPE_PATH "frontend/display/controler.3sh"
#define FRONTEND_CONTROL_OPTION_SHAPE_FORMAT "opt%d"
#define FRONTEND_CONTROL_PRESET_SHAPE_FORMAT "%d   "
#define FRONTEND_CONTROL_SHAPE_COUNT FRONTEND_RACE_OPTIONS_JOYSTICK_NAME_COUNT
#define FRONTEND_CONTROL_TWEEN_DURATION 0x28
#define FRONTEND_CONTROL_FORMAT_BUFFER_SIZE 12
#define FRONTEND_CONTROL_OPTION_SHAPE_INDEX 1
#define FRONTEND_CONTROL_POSITION_WORD 3
#define FRONTEND_CONTROL_POSITION_X_SHIFT 16
#define FRONTEND_CONTROL_NO_PREVIOUS_PRESET (-1)
#define FRONTEND_CONTROL_FIRST_PRESET 0
#define FRONTEND_CONTROL_PRESET_DISPLAY_BASE 1
#define FRONTEND_CONTROL_TWEEN_FIRST_SLOT 1U
#define FRONTEND_CONTROL_FORWARD_INPUT_MASK (ControlDown | ControlRight | ControlRightShift)
#define FRONTEND_CONTROL_BACK_INPUT_MASK (ControlUp | ControlLeft | ControlLeftShift)
#define FRONTEND_CONTROL_CONFIRM_INPUT_MASK (ControlA | ControlB | ControlC | ControlStart)
#define FRONTEND_CONTROL_POLL_INPUT_MASK 0xFFE00000

FrontendControlPosition g_FRONTEND_CONTROL_OFFSCREEN_POSITIONS[FRONTEND_CONTROL_SHAPE_COUNT] = {
  { 0, 0 },      { 20, 500 },  { 400, -20 }, { -20, 300 }, { -20, -20 }, { 500, 400 },
  { -100, 400 }, { 100, -50 }, { 500, 10 },  { 400, 500 }, { 0, 0 }
};

FrontendControlPosition
  g_FRONTEND_CONTROL_PRESET_POSITIONS[FRONTEND_RACE_OPTIONS_GAMEPAD_PRESET_COUNT]
                                     [FRONTEND_CONTROL_SHAPE_COUNT] = { { { 0, 0 },
                                                                          { 68, 78 },
                                                                          { 186, 183 },
                                                                          { 237, 148 },
                                                                          { 241, 91 },
                                                                          { 94, 39 },
                                                                          { 190, 42 },
                                                                          { 97, 175 },
                                                                          { 19, 166 },
                                                                          { 8, 47 },
                                                                          { 0, 0 } },
                                                                        { { 0, 0 },
                                                                          { 68, 78 },
                                                                          { 237, 148 },
                                                                          { 186, 183 },
                                                                          { 241, 91 },
                                                                          { 94, 39 },
                                                                          { 190, 42 },
                                                                          { 97, 175 },
                                                                          { 19, 166 },
                                                                          { 8, 47 },
                                                                          { 0, 0 } },
                                                                        { { 0, 0 },
                                                                          { 68, 78 },
                                                                          { 6, 76 },
                                                                          { 20, 134 },
                                                                          { 241, 91 },
                                                                          { 213, 154 },
                                                                          { 186, 181 },
                                                                          { 97, 175 },
                                                                          { 56, 40 },
                                                                          { 162, 46 },
                                                                          { 0, 0 } },
                                                                        { { 0, 0 },
                                                                          { 68, 78 },
                                                                          { 186, 183 },
                                                                          { 237, 148 },
                                                                          { 241, 91 },
                                                                          { 7, 144 },
                                                                          { 8, 76 },
                                                                          { 97, 175 },
                                                                          { 87, 38 },
                                                                          { 180, 43 },
                                                                          { 0, 0 } },
                                                                        { { 0, 0 },
                                                                          { 68, 78 },
                                                                          { 186, 183 },
                                                                          { 237, 148 },
                                                                          { 241, 91 },
                                                                          { 9, 159 },
                                                                          { 52, 53 },
                                                                          { 97, 175 },
                                                                          { 6, 103 },
                                                                          { 130, 63 },
                                                                          { 0, 0 } } };

void *g_FRONTEND_CONTROL_SELECTION_SHAPES;

typedef char FrontendControlOffscreenPositionsSizeCheck
  [sizeof(g_FRONTEND_CONTROL_OFFSCREEN_POSITIONS) == 0x58 ? 1 : -1];
typedef char FrontendControlPresetPositionsSizeCheck
  [sizeof(g_FRONTEND_CONTROL_PRESET_POSITIONS) == 0x1B8 ? 1 : -1];

static void
_draw_frontend_control_tween(int32 x_, int32 y_, void *shape_collection_, void *shape_name_)
{
  blit_runtime_named_shape_at(x_, y_, shape_collection_, (const uint32 *) shape_name_);
}

int32
draw_frontend_control_preset(int32 previous_preset_index_, int32 preset_index_)
{
  char                     shape_name[FRONTEND_CONTROL_FORMAT_BUFFER_SIZE];
  void                    *option_shape;
  void                    *preset_shape;
  void                    *shape;
  FrontendControlPosition *initial_positions;
  FrontendControlPosition *positions;
  FrontendControlPosition *target_positions;
  int32                    tween_slots[FRONTEND_CONTROL_SHAPE_COUNT];
  int32                    display_preset_index;
  int32                    shape_index;
  uint32                   tween_mask;

  initialize_frontend_display();
  if(g_FRONTEND_CONTROL_SELECTION_SHAPES == NULL)
    {
      g_FRONTEND_CONTROL_SELECTION_SHAPES = load_runtime_file_data(FRONTEND_CONTROL_SHAPE_PATH, 0);
      track_primary_allocation(g_FRONTEND_CONTROL_SELECTION_SHAPES);
    }

  target_positions = g_FRONTEND_CONTROL_PRESET_POSITIONS[preset_index_];
  shape_index      = 0;
  if(previous_preset_index_ == FRONTEND_CONTROL_NO_PREVIOUS_PRESET)
    {
      do
        {
          tween_slots[shape_index] =
            register_frontend_tween(g_FRONTEND_CONTROL_OFFSCREEN_POSITIONS[shape_index].x,
                                    target_positions[shape_index].x,
                                    g_FRONTEND_CONTROL_OFFSCREEN_POSITIONS[shape_index].y,
                                    target_positions[shape_index].y,
                                    FRONTEND_CONTROL_TWEEN_DURATION,
                                    g_FRONTEND_CONTROL_SELECTION_SHAPES,
                                    (void *) g_FRONTEND_CONTROL_PRESETS.joystick[shape_index],
                                    _draw_frontend_control_tween);
          shape_index += 1;
        }
      while(shape_index < FRONTEND_CONTROL_SHAPE_COUNT);
    }
  else
    {
      initial_positions = g_FRONTEND_CONTROL_PRESET_POSITIONS[previous_preset_index_];
      do
        {
          tween_slots[shape_index] =
            register_frontend_tween(initial_positions[shape_index].x,
                                    target_positions[shape_index].x,
                                    initial_positions[shape_index].y,
                                    target_positions[shape_index].y,
                                    FRONTEND_CONTROL_TWEEN_DURATION,
                                    g_FRONTEND_CONTROL_SELECTION_SHAPES,
                                    (void *) g_FRONTEND_CONTROL_PRESETS.joystick[shape_index],
                                    _draw_frontend_control_tween);
          shape_index += 1;
        }
      while(shape_index < FRONTEND_CONTROL_SHAPE_COUNT);
    }

  tween_mask = FRONTEND_CONTROL_TWEEN_FIRST_SLOT;
  tween_mask = (tween_mask << shape_index) - 1;
  complete_frontend_tweens(tween_mask);
  do
    {
      synchronize_runtime_frame();
    }
  while(update_frontend_tweens() != 0);

  shape_index = 0;
  do
    {
      release_frontend_tween(tween_slots[shape_index]);
      shape_index += 1;
    }
  while(shape_index < FRONTEND_CONTROL_SHAPE_COUNT);

  begin_runtime_frame();
  synchronize_runtime_frame();
  display_preset_index = preset_index_ + FRONTEND_CONTROL_PRESET_DISPLAY_BASE;
  runtime_format_string(shape_name, FRONTEND_CONTROL_OPTION_SHAPE_FORMAT, display_preset_index);
  option_shape =
    locate_runtime_shape(g_FRONTEND_CONTROL_SELECTION_SHAPES, (const uint32 *) shape_name);
  positions   = g_FRONTEND_CONTROL_PRESET_POSITIONS[preset_index_];
  shape_index = 0;
  do
    {
      shape =
        locate_runtime_shape(g_FRONTEND_CONTROL_SELECTION_SHAPES,
                             (const uint32 *) g_FRONTEND_CONTROL_PRESETS.joystick[shape_index]);
      blit_runtime_shape_at(shape, positions[shape_index].x, positions[shape_index].y);
      if(shape_index == FRONTEND_CONTROL_OPTION_SHAPE_INDEX)
        {
          draw_runtime_shape_at(option_shape,
                                ((int32 *) option_shape)[FRONTEND_CONTROL_POSITION_WORD] >>
                                  FRONTEND_CONTROL_POSITION_X_SHIFT,
                                (int16) ((int32 *) option_shape)[FRONTEND_CONTROL_POSITION_WORD]);
        }
      shape_index += 1;
    }
  while(shape_index < FRONTEND_CONTROL_SHAPE_COUNT);

  runtime_format_string(shape_name, FRONTEND_CONTROL_PRESET_SHAPE_FORMAT, display_preset_index);
  preset_shape =
    locate_runtime_shape(g_FRONTEND_CONTROL_SELECTION_SHAPES, (const uint32 *) shape_name);
  draw_runtime_shape_at(preset_shape,
                        ((int32 *) preset_shape)[FRONTEND_CONTROL_POSITION_WORD] >>
                          FRONTEND_CONTROL_POSITION_X_SHIFT,
                        (int16) ((int32 *) preset_shape)[FRONTEND_CONTROL_POSITION_WORD]);
  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();
  return 1;
}

int32
select_frontend_control_preset(int32 frame_count_, RaceConfiguration *configuration_)
{
  uint32 control_bits;
  int32  original_preset_index;
  int32  previous_preset_index;
  int32  preset_index;

  original_preset_index = configuration_->control_preset_index;
  previous_preset_index = FRONTEND_CONTROL_NO_PREVIOUS_PRESET;
  preset_index          = original_preset_index;
  draw_frontend_control_preset(previous_preset_index, preset_index);

  for(;;)
    {
      control_bits = poll_controller_input(frame_count_ * FRONTEND_RACE_OPTIONS_FRAME_RATE,
                                           1,
                                           FRONTEND_CONTROL_POLL_INPUT_MASK);
      if(control_bits == 0)
        {
          free_primary_memory(g_FRONTEND_CONTROL_SELECTION_SHAPES);
          g_FRONTEND_CONTROL_SELECTION_SHAPES = NULL;
          return original_preset_index;
        }
      if((control_bits & FRONTEND_CONTROL_FORWARD_INPUT_MASK) != 0)
        {
          previous_preset_index = preset_index;
          preset_index          = (preset_index + 1) % FRONTEND_RACE_OPTIONS_GAMEPAD_PRESET_COUNT;
        }
      if((control_bits & FRONTEND_CONTROL_BACK_INPUT_MASK) != 0)
        {
          previous_preset_index = preset_index;
          if(preset_index == FRONTEND_CONTROL_FIRST_PRESET)
            {
              preset_index = FRONTEND_RACE_OPTIONS_GAMEPAD_PRESET_COUNT - 1;
            }
          else
            {
              preset_index -= 1;
            }
        }
      if((control_bits & FRONTEND_CONTROL_CONFIRM_INPUT_MASK) != 0)
        {
          free_primary_memory(g_FRONTEND_CONTROL_SELECTION_SHAPES);
          g_FRONTEND_CONTROL_SELECTION_SHAPES = NULL;
          return preset_index;
        }
      if((control_bits & ControlX) != 0)
        {
          free_primary_memory(g_FRONTEND_CONTROL_SELECTION_SHAPES);
          g_FRONTEND_CONTROL_SELECTION_SHAPES = NULL;
          return original_preset_index;
        }
      draw_frontend_control_preset(previous_preset_index, preset_index);
    }
}
