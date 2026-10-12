#include "frontend_options.h"

#include "controller_input.h"
#include "frontend_audio_control.h"
#include "frontend_frame.h"
#include "frontend_sound.h"
#include "nvram_save.h"
#include "race_state.h"
#include "runtime_memory.h"
#include "runtime_memory_ops.h"
#include "tracked_memory.h"

#include "event.h"

#define FRONTEND_OPTIONS_SHAPE_PATH "frontend/display/checkopts.3sh"
#define FRONTEND_OPTIONS_BACKGROUND_SHAPE "bgnd"
#define FRONTEND_OPTIONS_MONO_SHAPE "mono"
#define FRONTEND_OPTIONS_STEREO_SHAPE "ster"
#define FRONTEND_OPTIONS_DISABLED_SHAPE "off "
#define FRONTEND_OPTIONS_ENABLED_SHAPE "on  "
#define FRONTEND_OPTIONS_SELECTOR_SHAPE "lite"
#define FRONTEND_OPTIONS_SHAPE_POSITION_WORD 3
#define FRONTEND_OPTIONS_SHAPE_X_SHIFT 16
#define FRONTEND_OPTIONS_AUDIO_MODE_COUNT 4
#define FRONTEND_OPTIONS_SHAPE_NAME_BYTE_COUNT 8
#define FRONTEND_OPTIONS_SELECTION_COUNT 3
#define FRONTEND_OPTIONS_AUDIO_SELECTION 0
#define FRONTEND_OPTIONS_STEREO_SELECTION 1
#define FRONTEND_OPTIONS_HOST_SELECTION 2
#define FRONTEND_OPTIONS_INPUT_FRAME_LIMIT 900
#define FRONTEND_OPTIONS_INPUT_MASK 0xCF600000

static const char
  G_FRONTEND_AUDIO_MODE_SHAPE_NAME_STORAGE[FRONTEND_OPTIONS_AUDIO_MODE_COUNT]
                                          [FRONTEND_OPTIONS_SHAPE_NAME_BYTE_COUNT] = { "none",
                                                                                       "musc",
                                                                                       "sfx ",
                                                                                       "all " };

const char *g_FRONTEND_AUDIO_MODE_SHAPE_NAMES[FRONTEND_OPTIONS_AUDIO_MODE_COUNT] = {
  G_FRONTEND_AUDIO_MODE_SHAPE_NAME_STORAGE[0],
  G_FRONTEND_AUDIO_MODE_SHAPE_NAME_STORAGE[1],
  G_FRONTEND_AUDIO_MODE_SHAPE_NAME_STORAGE[2],
  G_FRONTEND_AUDIO_MODE_SHAPE_NAME_STORAGE[3]
};
void             *g_FRONTEND_OPTIONS_SHAPES;
RaceConfiguration g_FRONTEND_OPTIONS_CONFIGURATION;
int32             g_FRONTEND_OPTIONS_SELECTION_Y[FRONTEND_OPTIONS_SELECTION_COUNT] = { 60, 102, 145 };
int32 g_FRONTEND_OPTIONS_SELECTION_X[FRONTEND_OPTIONS_SELECTION_COUNT] = { 38, 39, 39 };

typedef char
  FrontendOptionsSelectionYSizeCheck[sizeof(g_FRONTEND_OPTIONS_SELECTION_Y) == 12 ? 1 : -1];
typedef char
  FrontendOptionsSelectionXSizeCheck[sizeof(g_FRONTEND_OPTIONS_SELECTION_X) == 12 ? 1 : -1];
typedef char FrontendAudioModeShapeNameStorageSizeCheck
  [sizeof(G_FRONTEND_AUDIO_MODE_SHAPE_NAME_STORAGE) == 32 ? 1 : -1];
typedef char
  FrontendAudioModeShapeNamesSizeCheck[sizeof(g_FRONTEND_AUDIO_MODE_SHAPE_NAMES) == 16 ? 1 : -1];

static void
_draw_positioned_option_shape(const char *shape_name_)
{
  void *shape;
  int32 position;

  shape    = locate_runtime_shape(g_FRONTEND_OPTIONS_SHAPES, (const uint32 *) shape_name_);
  position = ((int32 *) shape)[FRONTEND_OPTIONS_SHAPE_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_OPTIONS_SHAPE_X_SHIFT, (int16) position);
}

static void
_apply_frontend_music_option(void)
{
  if(g_FRONTEND_OPTIONS_CONFIGURATION.music_enabled == 0)
    {
      pause_frontend_audio_stream();
    }
  else
    {
      resume_frontend_audio_stream();
    }
}

static void
_apply_frontend_stereo_option(void)
{
  if(g_FRONTEND_OPTIONS_CONFIGURATION.stereo_enabled == 0)
    {
      configure_mono_frontend_audio();
    }
  else
    {
      configure_stereo_frontend_audio();
    }
}

int32
draw_frontend_options(int32 selection_index_)
{
  void *background_shape;
  void *selector_shape;
  int32 audio_mode;
  int32 position;

  begin_runtime_frame();
  synchronize_runtime_frame();
  clear_runtime_display(0);
  if(g_FRONTEND_OPTIONS_SHAPES == NULL)
    {
      g_FRONTEND_OPTIONS_SHAPES = load_runtime_file_data(FRONTEND_OPTIONS_SHAPE_PATH, 0);
      track_primary_allocation(g_FRONTEND_OPTIONS_SHAPES);
    }

  background_shape = locate_runtime_shape(g_FRONTEND_OPTIONS_SHAPES,
                                          (const uint32 *) FRONTEND_OPTIONS_BACKGROUND_SHAPE);
  position         = ((int32 *) background_shape)[FRONTEND_OPTIONS_SHAPE_POSITION_WORD];
  blit_runtime_shape_at(background_shape,
                        position >> FRONTEND_OPTIONS_SHAPE_X_SHIFT,
                        (int16) position);

  audio_mode = (int32) g_FRONTEND_OPTIONS_CONFIGURATION.music_enabled |
               ((int32) g_FRONTEND_OPTIONS_CONFIGURATION.sound_effects_enabled << 1);
  _draw_positioned_option_shape(g_FRONTEND_AUDIO_MODE_SHAPE_NAMES[audio_mode]);
  if(g_FRONTEND_OPTIONS_CONFIGURATION.stereo_enabled == 0)
    {
      _draw_positioned_option_shape(FRONTEND_OPTIONS_MONO_SHAPE);
    }
  else
    {
      _draw_positioned_option_shape(FRONTEND_OPTIONS_STEREO_SHAPE);
    }
  if(g_FRONTEND_OPTIONS_CONFIGURATION.host_commentary_enabled == 0)
    {
      _draw_positioned_option_shape(FRONTEND_OPTIONS_DISABLED_SHAPE);
    }
  else
    {
      _draw_positioned_option_shape(FRONTEND_OPTIONS_ENABLED_SHAPE);
    }

  selector_shape = locate_runtime_shape(g_FRONTEND_OPTIONS_SHAPES,
                                        (const uint32 *) FRONTEND_OPTIONS_SELECTOR_SHAPE);
  draw_runtime_shape_at(selector_shape,
                        g_FRONTEND_OPTIONS_SELECTION_X[selection_index_],
                        g_FRONTEND_OPTIONS_SELECTION_Y[selection_index_]);
  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();
  return 1;
}

static void
_advance_frontend_option(int32 selection_index_)
{
  int32 audio_mode;

  if(selection_index_ == FRONTEND_OPTIONS_AUDIO_SELECTION)
    {
      audio_mode = (int32) g_FRONTEND_OPTIONS_CONFIGURATION.music_enabled |
                   ((int32) g_FRONTEND_OPTIONS_CONFIGURATION.sound_effects_enabled << 1);
      audio_mode = (audio_mode + 1) % FRONTEND_OPTIONS_AUDIO_MODE_COUNT;
      g_FRONTEND_OPTIONS_CONFIGURATION.music_enabled         = (uint8) (audio_mode & 1);
      g_FRONTEND_OPTIONS_CONFIGURATION.sound_effects_enabled = (uint8) ((audio_mode >> 1) & 1);
      _apply_frontend_music_option();
    }
  else if(selection_index_ == FRONTEND_OPTIONS_STEREO_SELECTION)
    {
      g_FRONTEND_OPTIONS_CONFIGURATION.stereo_enabled ^= 1;
      _apply_frontend_stereo_option();
    }
  else if(selection_index_ == FRONTEND_OPTIONS_HOST_SELECTION)
    {
      g_FRONTEND_OPTIONS_CONFIGURATION.host_commentary_enabled ^= 1;
    }
}

static void
_retreat_frontend_option(int32 selection_index_)
{
  int32 audio_mode;

  if(selection_index_ == FRONTEND_OPTIONS_AUDIO_SELECTION)
    {
      audio_mode = (int32) g_FRONTEND_OPTIONS_CONFIGURATION.music_enabled |
                   ((int32) g_FRONTEND_OPTIONS_CONFIGURATION.sound_effects_enabled << 1);
      if(audio_mode == 0)
        {
          audio_mode = FRONTEND_OPTIONS_AUDIO_MODE_COUNT - 1;
        }
      else
        {
          audio_mode -= 1;
        }
      g_FRONTEND_OPTIONS_CONFIGURATION.music_enabled         = (uint8) (audio_mode & 1);
      g_FRONTEND_OPTIONS_CONFIGURATION.sound_effects_enabled = (uint8) ((audio_mode >> 1) & 1);
      _apply_frontend_music_option();
    }
  else if(selection_index_ == FRONTEND_OPTIONS_STEREO_SELECTION)
    {
      g_FRONTEND_OPTIONS_CONFIGURATION.stereo_enabled ^= 1;
      _apply_frontend_stereo_option();
    }
  else if(selection_index_ == FRONTEND_OPTIONS_HOST_SELECTION)
    {
      g_FRONTEND_OPTIONS_CONFIGURATION.host_commentary_enabled ^= 1;
    }
}

static int32
_finish_frontend_options(int32 accepted_)
{
  if(accepted_ != 0)
    {
      copy_runtime_memory(RACE_CONFIGURATION,
                          &g_FRONTEND_OPTIONS_CONFIGURATION,
                          RACE_CONFIGURATION_BYTE_COUNT);
      save_frontend_nvram();
    }
  free_primary_memory(g_FRONTEND_OPTIONS_SHAPES);
  g_FRONTEND_OPTIONS_SHAPES = NULL;
  if(RACE_CONFIGURATION->music_enabled == 0)
    {
      pause_frontend_audio_stream();
    }
  else
    {
      resume_frontend_audio_stream();
    }
  return accepted_;
}

int32
run_frontend_options(void)
{
  uint32 control_bits;
  int32  selection_index;

  copy_runtime_memory(&g_FRONTEND_OPTIONS_CONFIGURATION,
                      RACE_CONFIGURATION,
                      RACE_CONFIGURATION_BYTE_COUNT);
  selection_index = 0;
  draw_frontend_options(selection_index);

  for(;;)
    {
      control_bits =
        poll_controller_input(FRONTEND_OPTIONS_INPUT_FRAME_LIMIT, 1, FRONTEND_OPTIONS_INPUT_MASK);
      if(control_bits == 0)
        {
          return _finish_frontend_options(0);
        }
      if((control_bits & ControlRightShift) != 0)
        {
          _advance_frontend_option(selection_index);
        }
      if((control_bits & ControlLeftShift) != 0)
        {
          _retreat_frontend_option(selection_index);
        }
      if((control_bits & ControlDown) != 0)
        {
          selection_index = (selection_index + 1) % FRONTEND_OPTIONS_SELECTION_COUNT;
        }
      if((control_bits & ControlUp) != 0)
        {
          if(selection_index == 0)
            {
              selection_index = FRONTEND_OPTIONS_SELECTION_COUNT - 1;
            }
          else
            {
              selection_index -= 1;
            }
        }
      if((control_bits & ControlStart) != 0)
        {
          return _finish_frontend_options(1);
        }
      if((control_bits & ControlX) != 0)
        {
          return _finish_frontend_options(0);
        }
      draw_frontend_options(selection_index);
    }
}
