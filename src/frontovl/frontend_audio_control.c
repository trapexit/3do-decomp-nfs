#include "frontend_audio_control.h"

#include "audio_stream_resources.h"
#include "frontend_audio_state.h"
#include "frontend_frame.h"
#include "frontend_runtime_state.h"
#include "frontend_skill_menu.h"
#include "frontend_sound.h"
#include "nvram_load.h"
#include "race_state.h"
#include "random_number.h"
#include "runtime_folios.h"
#include "runtime_memory.h"
#include "sdk_veneers.h"

#include "audio.h"

#define FRONTEND_SKILL_HEADING g_FRONTEND_SKILL_HEADING
#define FRONTEND_PLAYER_NAME g_FRONTEND_PLAYER_NAME
#define FRONTEND_MUSIC_CHOICE_COUNT 3
#define FRONTEND_MUSIC_FADE_TICKS 480
#define FRONTEND_SKILL_MENU_FRAME_COUNT 60
#define FRONTEND_SKILL_HEADING_BYTE_COUNT 7
#define FRONTEND_EMPTY_NAME_BYTE_COUNT 1
#define FRONTEND_TEST_SKILL_HEADING_BYTE_COUNT 11
#define FRONTEND_TEST_TRACK_INDEX 3
#define FRONTEND_PRESENTATION_ENABLED 1
#define FRONTEND_ATTRACT_CHOICE_COUNT 3
#define FRONTEND_ATTRACT_SHAPE_COUNT 4
#define FRONTEND_ATTRACT_DELAY_TICKS 240
#define FRONTEND_TITLE_FRAME_COUNT 5
#define FRONTEND_HOST_VARIANT_BASE 4
#define FRONTEND_PRESENTATION_WIDTH 100
#define FRONTEND_PRESENTATION_HALF_WIDTH 50
#define FRONTEND_HOST_CUE_BYTE_COUNT 12
#define FRONTEND_ATTRACT_DYNAMIC_SHAPE_INDEX 2
#define FRONTEND_ATTRACT_TITLE_SHAPE_INDEX 3
#define FRONTEND_CAR_DISPLAY_CHOICE_COUNT 8
#define FRONTEND_CAR_DISPLAY_SHAPE_NAME_BYTES 8
#define FRONTEND_ATTRACT_SHAPE_NAME_BYTES 12
#define FRONTEND_CAR_SHORT_PATH_BYTES 32
#define FRONTEND_CAR_LONG_PATH_BYTES 36
#define FRONTEND_MUSIC_PATH_BYTES 20

typedef struct FrontendCarDisplayPathStorage
{
  char supra[FRONTEND_CAR_SHORT_PATH_BYTES];
  char diablo[FRONTEND_CAR_LONG_PATH_BYTES];
  char porsche911[FRONTEND_CAR_SHORT_PATH_BYTES];
  char corvette[FRONTEND_CAR_SHORT_PATH_BYTES];
  char ferrari512tr[FRONTEND_CAR_SHORT_PATH_BYTES];
  char viper[FRONTEND_CAR_SHORT_PATH_BYTES];
  char nsx[FRONTEND_CAR_SHORT_PATH_BYTES];
  char rx7[FRONTEND_CAR_SHORT_PATH_BYTES];
} FrontendCarDisplayPathStorage;

typedef struct FrontendCarTitleShapeNameStorage
{
  char names[FRONTEND_CAR_DISPLAY_CHOICE_COUNT][FRONTEND_CAR_DISPLAY_SHAPE_NAME_BYTES];
} FrontendCarTitleShapeNameStorage;

typedef struct FrontendAttractShapeNameStorage
{
  char names[FRONTEND_ATTRACT_CHOICE_COUNT][FRONTEND_ATTRACT_SHAPE_NAME_BYTES];
} FrontendAttractShapeNameStorage;

typedef struct FrontendMusicPathStorage
{
  char paths[FRONTEND_MUSIC_CHOICE_COUNT][FRONTEND_MUSIC_PATH_BYTES];
} FrontendMusicPathStorage;

extern char *FRONTEND_SKILL_HEADING;
extern char *FRONTEND_PLAYER_NAME;

static const FrontendCarDisplayPathStorage G_FRONTEND_CAR_DISPLAY_PATH_STORAGE = {
  "frontend/display/cars/supra.3sh", "frontend/display/cars/diablo.3sh",
  "frontend/display/cars/911.3sh",   "frontend/display/cars/vette.3sh",
  "frontend/display/cars/512tr.3sh", "frontend/display/cars/viper.3sh",
  "frontend/display/cars/nsx.3sh",   "frontend/display/cars/rx7.3sh"
};

static const FrontendCarTitleShapeNameStorage G_FRONTEND_CAR_TITLE_SHAPE_NAME_STORAGE = {
  { "spra", "dblo", "p911", "cvet", "f512", "vipr", "ansx", "mrx7" }
};

static const FrontendAttractShapeNameStorage G_FRONTEND_ATTRACT_SHAPE_NAME_STORAGE = {
  { "Attract1", "Attract2", "Attract3" }
};

static const FrontendMusicPathStorage G_FRONTEND_MUSIC_PATH_STORAGE = {
  { "funkinA.44k.sw.aifc", "funkinB.44k.sw.aifc", "funkinC.44k.sw.aifc" }
};

static const char G_FRONTEND_ATTRACT_HOST_SHAPE_PATH[FRONTEND_CAR_SHORT_PATH_BYTES] =
  "frontend/display/hosts/hosti";

const char *g_FRONTEND_CAR_DISPLAY_SHAPE_PATHS[FRONTEND_CAR_DISPLAY_CHOICE_COUNT] = {
  G_FRONTEND_CAR_DISPLAY_PATH_STORAGE.supra,        G_FRONTEND_CAR_DISPLAY_PATH_STORAGE.diablo,
  G_FRONTEND_CAR_DISPLAY_PATH_STORAGE.porsche911,   G_FRONTEND_CAR_DISPLAY_PATH_STORAGE.corvette,
  G_FRONTEND_CAR_DISPLAY_PATH_STORAGE.ferrari512tr, G_FRONTEND_CAR_DISPLAY_PATH_STORAGE.viper,
  G_FRONTEND_CAR_DISPLAY_PATH_STORAGE.nsx,          G_FRONTEND_CAR_DISPLAY_PATH_STORAGE.rx7
};
const char *g_FRONTEND_CAR_TITLE_SHAPE_NAMES[FRONTEND_CAR_DISPLAY_CHOICE_COUNT] = {
  G_FRONTEND_CAR_TITLE_SHAPE_NAME_STORAGE.names[0],
  G_FRONTEND_CAR_TITLE_SHAPE_NAME_STORAGE.names[1],
  G_FRONTEND_CAR_TITLE_SHAPE_NAME_STORAGE.names[2],
  G_FRONTEND_CAR_TITLE_SHAPE_NAME_STORAGE.names[3],
  G_FRONTEND_CAR_TITLE_SHAPE_NAME_STORAGE.names[4],
  G_FRONTEND_CAR_TITLE_SHAPE_NAME_STORAGE.names[5],
  G_FRONTEND_CAR_TITLE_SHAPE_NAME_STORAGE.names[6],
  G_FRONTEND_CAR_TITLE_SHAPE_NAME_STORAGE.names[7]
};
const char *g_FRONTEND_MUSIC_PATHS[FRONTEND_MUSIC_CHOICE_COUNT] = {
  G_FRONTEND_MUSIC_PATH_STORAGE.paths[0],
  G_FRONTEND_MUSIC_PATH_STORAGE.paths[1],
  G_FRONTEND_MUSIC_PATH_STORAGE.paths[2]
};
const char *g_FRONTEND_ATTRACT_SHAPE_NAMES[FRONTEND_ATTRACT_CHOICE_COUNT] = {
  G_FRONTEND_ATTRACT_SHAPE_NAME_STORAGE.names[0],
  G_FRONTEND_ATTRACT_SHAPE_NAME_STORAGE.names[1],
  G_FRONTEND_ATTRACT_SHAPE_NAME_STORAGE.names[2]
};
int32 g_FRONTEND_SCENE_AUDIO_ACTIVE;
int32 g_FRONTEND_ATTRACT_INDEX;

typedef char FrontendMusicPathsSizeCheck[sizeof(g_FRONTEND_MUSIC_PATHS) == 12 ? 1 : -1];
typedef char
  FrontendMusicPathStorageSizeCheck[sizeof(G_FRONTEND_MUSIC_PATH_STORAGE) == 60 ? 1 : -1];
typedef char
  FrontendAttractShapeNamesSizeCheck[sizeof(g_FRONTEND_ATTRACT_SHAPE_NAMES) == 12 ? 1 : -1];
typedef char
  FrontendCarDisplayShapePathsSizeCheck[sizeof(g_FRONTEND_CAR_DISPLAY_SHAPE_PATHS) == 32 ? 1 : -1];
typedef char
  FrontendCarTitleShapeNamesSizeCheck[sizeof(g_FRONTEND_CAR_TITLE_SHAPE_NAMES) == 32 ? 1 : -1];

int32
resume_frontend_audio_stream(void)
{
  FrontendMusicState *music_state;

  music_state = g_FRONTEND_MUSIC_STATE;
  if(music_state == NULL)
    {
      return 0;
    }

  g_FRONTEND_PLAYBACK_ENABLED = 1;
  return ResumeInstrument(music_state->instrument);
}

int32
pause_frontend_audio_stream(void)
{
  FrontendMusicState *music_state;

  music_state = g_FRONTEND_MUSIC_STATE;
  if(music_state == NULL)
    {
      return (int32) music_state;
    }

  g_FRONTEND_PLAYBACK_ENABLED = 0;
  return PauseInstrument(music_state->instrument);
}

int32
start_frontend_scene_audio(void)
{
  int32 music_index;

  if(g_FRONTEND_SCENE_AUDIO_ACTIVE != 0)
    {
      return g_FRONTEND_SCENE_AUDIO_ACTIVE;
    }

  initialize_frontend_audio();
  if(RACE_CONFIGURATION->stereo_enabled != 0)
    {
      configure_stereo_frontend_audio();
    }
  else
    {
      configure_mono_frontend_audio();
    }

  music_index = random_below(FRONTEND_MUSIC_CHOICE_COUNT);
  configure_frontend_audio_stream(g_FRONTEND_MUSIC_PATHS[music_index]);
  g_FRONTEND_SCENE_AUDIO_ACTIVE = 1;
  if(RACE_CONFIGURATION->music_enabled != 0)
    {
      return resume_frontend_audio_stream();
    }
  return 0;
}

void
stop_frontend_scene_audio(void)
{
  if(g_FRONTEND_SCENE_AUDIO_ACTIVE == 0)
    {
      return;
    }

  fade_out_frontend_audio_stream();
  SleepAudioTicks(FRONTEND_MUSIC_FADE_TICKS);
  shutdown_frontend_audio();
  g_FRONTEND_SCENE_AUDIO_ACTIVE = 0;
}

int32
run_frontend_skill_selection(void)
{
  start_frontend_scene_audio();
  RUNTIME_MEMCPY_VENEER(FRONTEND_SKILL_HEADING, "Skills", FRONTEND_SKILL_HEADING_BYTE_COUNT);
  RUNTIME_MEMCPY_VENEER(FRONTEND_PLAYER_NAME, "", FRONTEND_EMPTY_NAME_BYTE_COUNT);

  for(;;)
    {
      if(run_frontend_skill_menu(FRONTEND_SKILL_MENU_FRAME_COUNT) != 0)
        {
          stop_frontend_scene_audio();
          if(RACE_CONFIGURATION->track_index == FRONTEND_TEST_TRACK_INDEX)
            {
              RUNTIME_MEMCPY_VENEER(FRONTEND_SKILL_HEADING,
                                    "TestSkills",
                                    FRONTEND_TEST_SKILL_HEADING_BYTE_COUNT);
            }
          return 1;
        }

      if(run_frontend_credits(0) == 0)
        {
          stop_frontend_scene_audio();
          return 0;
        }

      suspend_frontend_music_for_scene();
      resume_frontend_music_after_scene();
    }
}

void
run_frontend_event_presentation(uint32 flags_)
{
  char        host_cue[FRONTEND_HOST_CUE_BYTE_COUNT];
  const char *shape_names[FRONTEND_ATTRACT_SHAPE_COUNT];
  int32       host_result;
  int32       title_result;
  int32       attract_index;
  int32       presentation_enabled;

  load_frontend_nvram();
  initialize_runtime_control_calibration();
  RUNTIME_MEMCPY_VENEER(FRONTEND_SKILL_HEADING, "Skills", FRONTEND_SKILL_HEADING_BYTE_COUNT);
  RACE_CONFIGURATION->event_index = 0;

  presentation_enabled = flags_ & FRONTEND_PRESENTATION_ENABLED;
  if(presentation_enabled != 0)
    {
      begin_runtime_frame();
      clear_runtime_display(0);
      finish_runtime_frame();
      present_runtime_frame();
      set_runtime_grayscale_palette();
      initialize_frontend_stamp();
    }

  display_frontend_splash("frontend/display/3do.3sh", "3do ");
  SleepAudioTicks(FRONTEND_ATTRACT_DELAY_TICKS);
  g_FRONTEND_ATTRACT_INDEX = random_below(FRONTEND_ATTRACT_CHOICE_COUNT);

  shape_names[0]                                  = "eac";
  shape_names[1]                                  = "pioneer";
  shape_names[FRONTEND_ATTRACT_TITLE_SHAPE_INDEX] = "title";

  for(;;)
    {
      if(presentation_enabled == 0)
        {
          release_frontend_display();
          return;
        }

      attract_index = g_FRONTEND_ATTRACT_INDEX;
      shape_names[FRONTEND_ATTRACT_DYNAMIC_SHAPE_INDEX] =
        g_FRONTEND_ATTRACT_SHAPE_NAMES[attract_index];
      g_FRONTEND_ATTRACT_INDEX = (attract_index + 1) % FRONTEND_ATTRACT_CHOICE_COUNT;

      present_frontend_scene(FRONTEND_ATTRACT_SHAPE_COUNT,
                             shape_names,
                             0,
                             NULL,
                             NULL,
                             g_FRONTEND_SCENE_AUDIO_ACTIVE,
                             1,
                             0);
      title_result =
        display_frontend_title("frontend/display/title.3sh", "ndtl", FRONTEND_TITLE_FRAME_COUNT);
      if(title_result == 0)
        {
          g_FRONTEND_DISPLAY_X = FRONTEND_PRESENTATION_WIDTH;
          g_FRONTEND_DISPLAY_Y = FRONTEND_PRESENTATION_HALF_WIDTH;
          runtime_format_string(host_cue,
                                "101.%d",
                                random_below(FRONTEND_ATTRACT_CHOICE_COUNT) +
                                  FRONTEND_HOST_VARIANT_BASE);
          host_result = present_frontend_host(host_cue,
                                              present_frontend_attract_frame,
                                              G_FRONTEND_ATTRACT_HOST_SHAPE_PATH,
                                              "bgnd",
                                              g_FRONTEND_SCENE_AUDIO_ACTIVE,
                                              0);
          if(host_result != 0)
            {
              continue;
            }
        }

      if(run_frontend_skill_selection() != 0)
        {
          return;
        }
    }
}

void
suspend_frontend_music_for_scene(void)
{
  fade_out_frontend_audio_stream();
  SleepAudioTicks(FRONTEND_MUSIC_FADE_TICKS);
  stop_frontend_audio_stream();
}

void
resume_frontend_music_after_scene(void)
{
  int32 music_index;

  music_index = random_below(FRONTEND_MUSIC_CHOICE_COUNT);
  start_frontend_music(g_FRONTEND_MUSIC_PATHS[music_index]);
  if(RACE_CONFIGURATION->music_enabled == 0)
    {
      pause_frontend_audio_stream();
    }
}
