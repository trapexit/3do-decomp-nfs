#include "frontend_state.h"

#include "audio_mode_names.h"
#include "ccb_manager.h"
#include "cel_render.h"
#include "display_buffer_state.h"
#include "file_load_options.h"
#include "frontend_control_input.h"
#include "frontend_input_wait.h"
#include "frontend_transition.h"
#include "memory.h"
#include "shape_blit.h"
#include "shape_lookup.h"
#include "simulation_snapshot.h"
#include "video_window_state.h"
#include "window_fill.h"

#include "event.h"

#include "stddef.h"

#define FRONTEND_OPTIONS_SHAPE_PATH "frontend/display/checkopts.3sh"
#define FRONTEND_OPTIONS_BACKGROUND_NAME "bgnd"
#define FRONTEND_OPTIONS_MONO_NAME "mono"
#define FRONTEND_OPTIONS_STEREO_NAME "ster"
#define FRONTEND_OPTIONS_OFF_NAME "off "
#define FRONTEND_OPTIONS_ON_NAME "on  "
#define FRONTEND_OPTIONS_HIGHLIGHT_NAME "lite"
#define FRONTEND_OPTIONS_SOUND_EFFECTS_SHIFT 1
#define FRONTEND_OPTIONS_INITIAL_WINDOW_VALUE 0
#define FRONTEND_SHAPE_PACKED_POSITION_WORD 3
#define FRONTEND_SHAPE_X_POSITION_SHIFT 16
#define FRONTEND_OPTIONS_POSITION_BYTES 12
#define FRONTEND_OPTIONS_POSITION_Y_FIRST 60
#define FRONTEND_OPTIONS_POSITION_Y_SECOND 102
#define FRONTEND_OPTIONS_POSITION_Y_THIRD 145
#define FRONTEND_OPTIONS_POSITION_X_FIRST 38
#define FRONTEND_OPTIONS_POSITION_X_SECOND 39
#define FRONTEND_OPTIONS_POSITION_X_THIRD 39
#define FRONTEND_OPTIONS_SELECTION_COUNT 3
#define FRONTEND_OPTIONS_AUDIO_SELECTION 0
#define FRONTEND_OPTIONS_STEREO_SELECTION 1
#define FRONTEND_OPTIONS_HOST_SELECTION 2
#define FRONTEND_OPTIONS_AUDIO_MODE_COUNT 4
#define FRONTEND_OPTIONS_AUDIO_MODE_MASK 3
#define FRONTEND_OPTIONS_SELECTOR_LOW_MASK 1
#define FRONTEND_OPTIONS_INPUT_MASK                                                                \
  (ControlDown | ControlUp | ControlStart | ControlX | ControlRightShift | ControlLeftShift)
#define FRONTEND_OPTIONS_NEXT_VALUE ControlRightShift
#define FRONTEND_OPTIONS_PREVIOUS_VALUE ControlLeftShift
#define FRONTEND_OPTIONS_NEXT_SELECTION ControlDown
#define FRONTEND_OPTIONS_PREVIOUS_SELECTION ControlUp
#define FRONTEND_OPTIONS_CONFIRM ControlStart
#define FRONTEND_OPTIONS_CANCEL ControlX
#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

typedef char SizeAssertFrontendRenderState[(sizeof(FrontendRenderState) == 0x24) ? 1 : -1];
typedef char SizeAssertFrontendMenuModeOffset[(offsetof(FrontendMenuState, mode) == 0x14) ? 1 : -1];
typedef char
  SizeAssertFrontendMenuResetOffset[(offsetof(FrontendMenuState, reset) == 0x18) ? 1 : -1];
typedef char SizeAssertFrontendMenuRenderResetPendingOffset
  [(offsetof(FrontendMenuState, render_reset_pending) == 0x20) ? 1 : -1];
typedef char
  SizeAssertFrontendMenuActiveOffset[(offsetof(FrontendMenuState, active) == 0x21) ? 1 : -1];
typedef char SizeAssertFrontendMenuRenderCompleteOffset
  [(offsetof(FrontendMenuState, render_complete) == 0x22) ? 1 : -1];
typedef char SizeAssertFrontendMenuTransitionActiveOffset
  [(offsetof(FrontendMenuState, transition_active) == 0x23) ? 1 : -1];

FrontendRenderState  g_FRONTEND_RENDER_STATE;
void                *g_FRONTEND_OPTIONS_SHAPE_RESOURCE;
StartupConfiguration g_FRONTEND_OPTIONS_CONFIGURATION;
uint8                g_FRONTEND_SAVED_SOUND_EFFECTS;
uint8                g_FRONTEND_SAVED_SOUND_EFFECTS_PADDING[3];
int32                g_FRONTEND_OPTIONS_POSITION_Y[FRONTEND_OPTIONS_POSITION_COUNT] = {
  FRONTEND_OPTIONS_POSITION_Y_FIRST,
  FRONTEND_OPTIONS_POSITION_Y_SECOND,
  FRONTEND_OPTIONS_POSITION_Y_THIRD
};
int32 g_FRONTEND_OPTIONS_POSITION_X[FRONTEND_OPTIONS_POSITION_COUNT] = {
  FRONTEND_OPTIONS_POSITION_X_FIRST,
  FRONTEND_OPTIONS_POSITION_X_SECOND,
  FRONTEND_OPTIONS_POSITION_X_THIRD
};

COMPILE_TIME_SIZE_ASSERT(frontend_options_position_y_size,
                         sizeof(g_FRONTEND_OPTIONS_POSITION_Y) == FRONTEND_OPTIONS_POSITION_BYTES);
COMPILE_TIME_SIZE_ASSERT(frontend_options_position_x_size,
                         sizeof(g_FRONTEND_OPTIONS_POSITION_X) == FRONTEND_OPTIONS_POSITION_BYTES);

static void
_draw_frontend_option_shape(const uint32 *shape_)
{
  uint32 packed_position;

  packed_position = shape_[FRONTEND_SHAPE_PACKED_POSITION_WORD];
  draw_cel_header(shape_,
                  (int32) (packed_position >> FRONTEND_SHAPE_X_POSITION_SHIFT),
                  (int32) (int16) packed_position);
}

int32
render_frontend_display_options(int32 selected_option_)
{
  const uint32 *shape;
  int32         audio_mode_index;

  prepare_pending_video_window();
  wait_for_frontend_input();
  fill_display_window(FRONTEND_OPTIONS_INITIAL_WINDOW_VALUE);
  if(g_FRONTEND_OPTIONS_SHAPE_RESOURCE == NULL)
    {
      g_FRONTEND_OPTIONS_SHAPE_RESOURCE =
        load_file_with_options_with_error(FRONTEND_OPTIONS_SHAPE_PATH, 0);
    }

  shape = (const uint32 *) require_shape(g_FRONTEND_OPTIONS_SHAPE_RESOURCE,
                                         (const uint32 *) FRONTEND_OPTIONS_BACKGROUND_NAME);
  blit_shape_at(
    shape,
    (int32) (shape[FRONTEND_SHAPE_PACKED_POSITION_WORD] >> FRONTEND_SHAPE_X_POSITION_SHIFT),
    (int32) (int16) shape[FRONTEND_SHAPE_PACKED_POSITION_WORD]);

  audio_mode_index = (int32) g_FRONTEND_OPTIONS_CONFIGURATION.music_enabled |
                     ((int32) g_FRONTEND_OPTIONS_CONFIGURATION.sound_effects_enabled
                      << FRONTEND_OPTIONS_SOUND_EFFECTS_SHIFT);
  shape =
    (const uint32 *) require_shape(g_FRONTEND_OPTIONS_SHAPE_RESOURCE,
                                   (const uint32 *) g_AUDIO_MODE_NAME_POINTERS[audio_mode_index]);
  _draw_frontend_option_shape(shape);

  if(g_FRONTEND_OPTIONS_CONFIGURATION.stereo_enabled == 0)
    {
      shape = (const uint32 *) require_shape(g_FRONTEND_OPTIONS_SHAPE_RESOURCE,
                                             (const uint32 *) FRONTEND_OPTIONS_MONO_NAME);
    }
  else
    {
      shape = (const uint32 *) require_shape(g_FRONTEND_OPTIONS_SHAPE_RESOURCE,
                                             (const uint32 *) FRONTEND_OPTIONS_STEREO_NAME);
    }
  _draw_frontend_option_shape(shape);

  if(g_FRONTEND_OPTIONS_CONFIGURATION.host_commentary_enabled == 0)
    {
      shape = (const uint32 *) require_shape(g_FRONTEND_OPTIONS_SHAPE_RESOURCE,
                                             (const uint32 *) FRONTEND_OPTIONS_OFF_NAME);
    }
  else
    {
      shape = (const uint32 *) require_shape(g_FRONTEND_OPTIONS_SHAPE_RESOURCE,
                                             (const uint32 *) FRONTEND_OPTIONS_ON_NAME);
    }
  _draw_frontend_option_shape(shape);

  shape = (const uint32 *) require_shape(g_FRONTEND_OPTIONS_SHAPE_RESOURCE,
                                         (const uint32 *) FRONTEND_OPTIONS_HIGHLIGHT_NAME);
  draw_cel_header(shape,
                  g_FRONTEND_OPTIONS_POSITION_X[selected_option_],
                  g_FRONTEND_OPTIONS_POSITION_Y[selected_option_]);
  finalize_allocated_ccb_batch();
  advance_display_buffer();
  return 1;
}

static void
_release_frontend_options_resources(void)
{
  release_frontend_input_buffer();
  free_memory(g_FRONTEND_OPTIONS_SHAPE_RESOURCE);
  g_FRONTEND_OPTIONS_SHAPE_RESOURCE = NULL;
}

static void
_update_frontend_audio_mode(int32 direction_)
{
  int32 audio_mode;

  audio_mode = (int32) g_FRONTEND_OPTIONS_CONFIGURATION.music_enabled |
               ((int32) g_FRONTEND_OPTIONS_CONFIGURATION.sound_effects_enabled
                << FRONTEND_OPTIONS_SOUND_EFFECTS_SHIFT);
  audio_mode += direction_;
  if(audio_mode < 0)
    audio_mode = FRONTEND_OPTIONS_AUDIO_MODE_COUNT - 1;
  audio_mode &= FRONTEND_OPTIONS_AUDIO_MODE_MASK;
  g_FRONTEND_OPTIONS_CONFIGURATION.music_enabled =
    (uint8) (audio_mode & FRONTEND_OPTIONS_SELECTOR_LOW_MASK);
  g_FRONTEND_OPTIONS_CONFIGURATION.sound_effects_enabled =
    (uint8) (audio_mode >> FRONTEND_OPTIONS_SOUND_EFFECTS_SHIFT);
}

int32
initialize_frontend_state_internal(void)
{
  uint32 control_input;
  int32  selected_option;

  g_FRONTEND_OPTIONS_CONFIGURATION              = g_STARTUP_CONFIGURATION;
  g_FRONTEND_SAVED_SOUND_EFFECTS                = g_STARTUP_CONFIGURATION.sound_effects_enabled;
  g_STARTUP_CONFIGURATION.sound_effects_enabled = 0;
  selected_option                               = 0;

  if(render_frontend_display_options(selected_option) == 0)
    {
      _release_frontend_options_resources();
      g_STARTUP_CONFIGURATION.sound_effects_enabled = g_FRONTEND_SAVED_SOUND_EFFECTS;
      return 0;
    }

  for(;;)
    {
      control_input = wait_for_frontend_control_press(FRONTEND_OPTIONS_INPUT_MASK);
      if(control_input == 0)
        {
          _release_frontend_options_resources();
          g_STARTUP_CONFIGURATION.sound_effects_enabled = g_FRONTEND_SAVED_SOUND_EFFECTS;
          return 0;
        }

      if((control_input & FRONTEND_OPTIONS_NEXT_VALUE) != 0)
        {
          if(selected_option == FRONTEND_OPTIONS_AUDIO_SELECTION)
            _update_frontend_audio_mode(1);
          else if(selected_option == FRONTEND_OPTIONS_STEREO_SELECTION)
            g_FRONTEND_OPTIONS_CONFIGURATION.stereo_enabled ^= 1;
          else if(selected_option == FRONTEND_OPTIONS_HOST_SELECTION)
            g_FRONTEND_OPTIONS_CONFIGURATION.host_commentary_enabled ^= 1;
        }

      if((control_input & FRONTEND_OPTIONS_PREVIOUS_VALUE) != 0)
        {
          if(selected_option == FRONTEND_OPTIONS_AUDIO_SELECTION)
            _update_frontend_audio_mode(-1);
          else if(selected_option == FRONTEND_OPTIONS_STEREO_SELECTION)
            g_FRONTEND_OPTIONS_CONFIGURATION.stereo_enabled ^= 1;
          else if(selected_option == FRONTEND_OPTIONS_HOST_SELECTION)
            g_FRONTEND_OPTIONS_CONFIGURATION.host_commentary_enabled ^= 1;
        }

      if((control_input & FRONTEND_OPTIONS_NEXT_SELECTION) != 0)
        {
          selected_option = (selected_option + 1) % FRONTEND_OPTIONS_SELECTION_COUNT;
        }
      if((control_input & FRONTEND_OPTIONS_PREVIOUS_SELECTION) != 0)
        {
          if(selected_option == 0)
            selected_option = FRONTEND_OPTIONS_SELECTION_COUNT - 1;
          else
            selected_option--;
        }

      if((control_input & FRONTEND_OPTIONS_CONFIRM) != 0)
        {
          g_STARTUP_CONFIGURATION = g_FRONTEND_OPTIONS_CONFIGURATION;
          _release_frontend_options_resources();
          do
            {
              control_input = read_frontend_control_input();
            }
          while((control_input & FRONTEND_OPTIONS_CONFIRM) != 0);
          return 1;
        }

      if((control_input & FRONTEND_OPTIONS_CANCEL) != 0)
        {
          _release_frontend_options_resources();
          g_STARTUP_CONFIGURATION.sound_effects_enabled = g_FRONTEND_SAVED_SOUND_EFFECTS;
          return 0;
        }

      if(render_frontend_display_options(selected_option) == 0)
        {
          g_STARTUP_CONFIGURATION.sound_effects_enabled = g_FRONTEND_SAVED_SOUND_EFFECTS;
          return 0;
        }
    }
}

void
initialize_frontend_state(void)
{
  (void) initialize_frontend_state_internal();
}

void
set_frontend_state(int32 active_, int32 mode_, int32 reset_, int32 transition_active_)
{
  g_FRONTEND_RENDER_STATE.menu.active                = (uint8) active_;
  g_FRONTEND_RENDER_STATE.menu.mode                  = (uint8) mode_;
  g_FRONTEND_RENDER_STATE.menu.transition_active     = (uint8) transition_active_;
  g_FRONTEND_RENDER_STATE.menu.reset                 = reset_;
  g_FRONTEND_RENDER_STATE.menu.saved_simulation_mode =
    (int32) g_SIMULATION_RENDER_STATE.requested_mode;
}
