#include "post_race_flow.h"

#include "audio_stream_resources.h"
#include "controller_input.h"
#include "frontend_audio_control.h"
#include "frontend_frame.h"
#include "frontend_runtime_state.h"
#include "frontend_status_text.h"
#include "frontend_text_layout.h"
#include "nvram_save.h"
#include "post_race_legacy.h"
#include "post_race_results.h"
#include "post_race_stats.h"
#include "post_race_status.h"
#include "race_commentary.h"
#include "race_state.h"
#include "random_number.h"
#include "runtime_folios.h"
#include "runtime_memory.h"
#include "sdk_veneers.h"
#include "tracked_memory.h"

#define POST_RACE_RESULT_SLOT g_POST_RACE_RESULT_SLOT
#define POST_RACE_CONTEXT_BYTE_COUNT 28
#define POST_RACE_SCENE_SHAPE_COUNT 4
#define POST_RACE_CUE_BYTE_COUNT 12
#define POST_RACE_MUSIC_CHOICE_COUNT 3
#define POST_RACE_DISABLED_RACE_MODE 8
#define POST_RACE_PRESENT_SHAPE_COUNT 1
#define POST_RACE_MOVIE_MODE 1
#define POST_RACE_SAVE_DISABLED_MODE 1
#define POST_RACE_SAVE_ENABLED_MODE 0
#define POST_RACE_INPUT_FRAME_LIMIT 3600
#define POST_RACE_INPUT_MASK 0x0E000000
#define POST_RACE_FINISH_MENU_TIMEOUT 1440
#define POST_RACE_WIN_SHAPE_PATH "frontend/display/statics/win.3sh"
#define POST_RACE_MOVIE_SHAPE_PATH "frontend/display/movie2.3sh"
#define POST_RACE_HOST_SHAPE_PATH "frontend/display/hosts/hostf"
#define POST_RACE_BACKGROUND_SHAPE "bgnd"
#define POST_RACE_FOREGROUND_SHAPE "shot"
#define POST_RACE_STATUS_SHAPE "win"
#define RANKED_POST_RACE_CUE_FORMAT "46.%d"
#define POST_RACE_MAX_CUE_VARIANTS 5
#define RANKED_POST_RACE_CUE_BYTE_COUNT 12
#define PURSUIT_POST_RACE_CUE_FORMAT "cop%d"
#define PURSUIT_POST_RACE_HOST_CUE_FORMAT "25.%d"
#define PURSUIT_POST_RACE_OPENING_VARIANTS 3
#define PURSUIT_POST_RACE_HOST_VARIANTS 6
#define PURSUIT_POST_RACE_HOST_VARIANT_BASE 2
#define FINAL_POST_RACE_SPECIAL_CUE_FORMAT "34.%d"
#define FINAL_POST_RACE_DEFAULT_CUE_FORMAT "44.%d"
#define FINAL_POST_RACE_CUE_VARIANTS 2
#define FINAL_POST_RACE_SPECIAL_VARIANT_BASE 2

typedef struct PostRaceCueVariants
{
  int32 count;
  int32 variants[POST_RACE_MAX_CUE_VARIANTS];
} PostRaceCueVariants;

typedef struct PostRaceSceneContext
{
  const char *scene_shapes[POST_RACE_SCENE_SHAPE_COUNT];
  char        cue_code[POST_RACE_CUE_BYTE_COUNT];
} PostRaceSceneContext;

extern int32 *POST_RACE_RESULT_SLOT;

PostRaceCueVariants g_POST_RACE_CUE_VARIANTS[8] = {
  { 3, { 1, 4, 6, 0, 0 } }, { 5, { 2, 3, 4, 5, 6 } }, { 3, { 3, 4, 6, 0, 0 } },
  { 4, { 1, 3, 4, 6, 0 } }, { 4, { 2, 3, 4, 6, 0 } }, { 4, { 1, 3, 4, 6, 0 } },
  { 3, { 3, 4, 6, 0, 0 } }, { 3, { 1, 4, 6, 0, 0 } }
};

typedef char PostRaceCueVariantsSizeCheck[sizeof(g_POST_RACE_CUE_VARIANTS) == 192 ? 1 : -1];
typedef char PostRaceSceneContextSizeCheck[sizeof(PostRaceSceneContext) ==
                                                   POST_RACE_CONTEXT_BYTE_COUNT
                                             ? 1
                                             : -1];

static void
_start_post_race_music(void)
{
  int32 music_index;

  initialize_frontend_audio();
  music_index = random_below(POST_RACE_MUSIC_CHOICE_COUNT);
  configure_frontend_audio_stream(g_FRONTEND_MUSIC_PATHS[music_index]);
  if(RACE_CONFIGURATION->music_enabled != 0)
    {
      resume_frontend_audio_stream();
    }
}

int32
run_post_race_flow(void)
{
  PostRaceSceneContext context;
  char                *cue_code;
  void *display_shapes;
  int32 initial_result;
  int32 menu_result;
  int32 save_mode;

  calculate_post_race_score(0);
  initialize_frontend_display();
  configure_compact_movie_stream_buffers();
  format_post_race_status(g_FRONTEND_STATUS_FIRST_TEXT,
                          g_FRONTEND_STATUS_SECOND_TEXT,
                          g_FRONTEND_STATUS_THIRD_TEXT);

  initial_result = *POST_RACE_RESULT_SLOT;
  if(initial_result < 1)
    {
      if(RACE_CONFIGURATION->opponent_vehicle_index != POST_RACE_DISABLED_RACE_MODE &&
         RACE_TIMING_STATE->primary_time < RACE_TIMING_STATE->secondary_time)
        {
          context.scene_shapes[0] = POST_RACE_STATUS_SHAPE;
          configure_compact_movie_stream_buffers();
          present_frontend_scene(POST_RACE_PRESENT_SHAPE_COUNT,
                                 context.scene_shapes,
                                 0,
                                 POST_RACE_BACKGROUND_SHAPE,
                                 POST_RACE_FOREGROUND_SHAPE,
                                 0,
                                 0,
                                 0,
                                 POST_RACE_STATUS_SHAPE);
        }

      if(RACE_CONFIGURATION->host_commentary_enabled != 0)
        {
          cue_code = context.cue_code;
          select_race_commentary_cue(cue_code);
          present_frontend_host(cue_code,
                                present_frontend_frame,
                                POST_RACE_HOST_SHAPE_PATH,
                                POST_RACE_BACKGROUND_SHAPE,
                                0,
                                0);
          _start_post_race_music();
        }
      else
        {
          _start_post_race_music();
          display_shapes =
            display_frontend_shape(POST_RACE_MOVIE_SHAPE_PATH, POST_RACE_BACKGROUND_SHAPE);
          draw_frontend_status_text();
          finish_runtime_frame();
          present_runtime_frame();
          poll_controller_input(POST_RACE_INPUT_FRAME_LIMIT, 1, POST_RACE_INPUT_MASK);
          free_primary_memory(display_shapes);
        }
    }
  else
    {
      _start_post_race_music();
    }

  if(g_POST_RACE_SAVE_ENABLED != 0 && *POST_RACE_RESULT_SLOT < 1)
    {
      if(RACE_CONFIGURATION->skill_level != 0)
        {
          save_mode = POST_RACE_SAVE_ENABLED_MODE;
        }
      else
        {
          save_mode = POST_RACE_SAVE_DISABLED_MODE;
        }
      save_post_race_results(RACE_PARTICIPANT_RECORDS, save_mode, 0);
      save_frontend_nvram();
    }

  menu_result = run_finish_menu(POST_RACE_FINISH_MENU_TIMEOUT);
  if(*POST_RACE_RESULT_SLOT < 1)
    {
      display_post_race_stats(0);
      shutdown_frontend_audio();
      RACE_CONFIGURATION->event_index = 0;
      menu_result                     = 0;
    }
  else
    {
      shutdown_frontend_audio();
    }
  release_frontend_display();
  return menu_result;
}

int32
run_ranked_post_race_flow(void)
{
  char                 cue_code[RANKED_POST_RACE_CUE_BYTE_COUNT];
  PostRaceCueVariants *cue_variants;
  void                *display_shapes;
  int32                initial_result;
  int32                menu_result;
  int32                variant_index;

  begin_runtime_frame();
  synchronize_runtime_frame();
  finish_runtime_frame();
  clear_runtime_display(0);
  finish_runtime_frame();
  configure_compact_movie_stream_buffers();
  initialize_frontend_display();
  format_ranked_post_race_status(g_FRONTEND_OVERLAY_STATUS_TEXT,
                                 g_FRONTEND_STATUS_FIRST_TEXT,
                                 g_FRONTEND_STATUS_THIRD_TEXT,
                                 g_FRONTEND_STATUS_SECOND_TEXT);

  initial_result = *POST_RACE_RESULT_SLOT;
  if(initial_result < 1)
    {
      cue_variants  = &g_POST_RACE_CUE_VARIANTS[RACE_CONFIGURATION->player_vehicle_index];
      variant_index = random_below(cue_variants->count);
      runtime_format_string(cue_code,
                            RANKED_POST_RACE_CUE_FORMAT,
                            cue_variants->variants[variant_index]);

      if(RACE_CONFIGURATION->host_commentary_enabled != 0)
        {
          present_frontend_host(cue_code,
                                present_frontend_frame,
                                POST_RACE_HOST_SHAPE_PATH,
                                POST_RACE_BACKGROUND_SHAPE,
                                0,
                                0);
          _start_post_race_music();
        }
      else
        {
          _start_post_race_music();
          display_shapes =
            display_frontend_shape(POST_RACE_MOVIE_SHAPE_PATH, POST_RACE_BACKGROUND_SHAPE);
          draw_frontend_status_text();
          finish_runtime_frame();
          present_runtime_frame();
          poll_controller_input(POST_RACE_INPUT_FRAME_LIMIT, 1, POST_RACE_INPUT_MASK);
          free_primary_memory(display_shapes);
        }
    }
  else
    {
      _start_post_race_music();
    }

  if(g_POST_RACE_SAVE_ENABLED != 0 && *POST_RACE_RESULT_SLOT < 1)
    {
      save_post_race_results(RACE_PARTICIPANT_RECORDS, 1, 1);
      save_frontend_nvram();
    }

  menu_result = run_finish_menu(POST_RACE_FINISH_MENU_TIMEOUT);
  if(*POST_RACE_RESULT_SLOT < 1)
    {
      display_post_race_stats(0);
      shutdown_frontend_audio();
      RACE_CONFIGURATION->event_index = 0;
      menu_result                     = 0;
    }
  else
    {
      shutdown_frontend_audio();
    }
  release_frontend_display();
  return menu_result;
}

int32
run_pursuit_post_race_flow(void)
{
  PostRaceSceneContext context;
  char                *cue_code;
  void *display_shapes;
  int32 initial_result;
  int32 menu_result;
  int32 variant;

  begin_runtime_frame();
  synchronize_runtime_frame();
  finish_runtime_frame();
  clear_runtime_display(0);
  finish_runtime_frame();
  configure_compact_movie_stream_buffers();
  initialize_frontend_display();
  format_ranked_post_race_status(g_FRONTEND_OVERLAY_STATUS_TEXT,
                                 g_FRONTEND_STATUS_FIRST_TEXT,
                                 g_FRONTEND_STATUS_THIRD_TEXT,
                                 g_FRONTEND_STATUS_SECOND_TEXT);

  initial_result = *POST_RACE_RESULT_SLOT;
  if(initial_result < 1)
    {
      cue_code                = context.cue_code;
      context.scene_shapes[0] = cue_code;
      variant  = random_below(PURSUIT_POST_RACE_OPENING_VARIANTS) + 1;
      runtime_format_string(cue_code, PURSUIT_POST_RACE_CUE_FORMAT, variant);
      configure_compact_movie_stream_buffers();
      present_frontend_scene(POST_RACE_PRESENT_SHAPE_COUNT,
                             context.scene_shapes,
                             0,
                             NULL,
                             NULL,
                             0,
                             POST_RACE_MOVIE_MODE,
                             0,
                             cue_code);

      if(RACE_CONFIGURATION->host_commentary_enabled != 0)
        {
          variant =
            random_below(PURSUIT_POST_RACE_HOST_VARIANTS) + PURSUIT_POST_RACE_HOST_VARIANT_BASE;
          runtime_format_string(cue_code, PURSUIT_POST_RACE_HOST_CUE_FORMAT, variant);
          present_frontend_host(cue_code,
                                present_frontend_frame,
                                POST_RACE_HOST_SHAPE_PATH,
                                POST_RACE_BACKGROUND_SHAPE,
                                0,
                                0);
          _start_post_race_music();
        }
      else
        {
          _start_post_race_music();
          display_shapes =
            display_frontend_shape(POST_RACE_MOVIE_SHAPE_PATH, POST_RACE_BACKGROUND_SHAPE);
          draw_frontend_status_text();
          finish_runtime_frame();
          present_runtime_frame();
          poll_controller_input(POST_RACE_INPUT_FRAME_LIMIT, 1, POST_RACE_INPUT_MASK);
          free_primary_memory(display_shapes);
        }
    }
  else
    {
      _start_post_race_music();
    }

  if(g_POST_RACE_SAVE_ENABLED != 0 && *POST_RACE_RESULT_SLOT < 1)
    {
      save_post_race_results(RACE_PARTICIPANT_RECORDS, 1, 1);
      save_frontend_nvram();
    }

  menu_result = run_finish_menu(POST_RACE_FINISH_MENU_TIMEOUT);
  if(*POST_RACE_RESULT_SLOT < 1)
    {
      display_post_race_stats(0);
      shutdown_frontend_audio();
      RACE_CONFIGURATION->event_index = 0;
      menu_result                     = 0;
    }
  else
    {
      shutdown_frontend_audio();
    }
  release_frontend_display();
  return menu_result;
}

int32
run_final_post_race_flow(void)
{
  char  cue_code[RANKED_POST_RACE_CUE_BYTE_COUNT];
  void *display_shapes;
  int32 variant;

  begin_runtime_frame();
  finish_runtime_frame();
  clear_runtime_display(0);
  finish_runtime_frame();
  initialize_frontend_display();
  configure_compact_movie_stream_buffers();
  format_ranked_post_race_status(g_FRONTEND_OVERLAY_STATUS_TEXT,
                                 g_FRONTEND_STATUS_FIRST_TEXT,
                                 g_FRONTEND_STATUS_THIRD_TEXT,
                                 g_FRONTEND_STATUS_SECOND_TEXT);

  if(RACE_TIMING_STATE->special_event != 0)
    {
      variant = random_below(FINAL_POST_RACE_CUE_VARIANTS) + FINAL_POST_RACE_SPECIAL_VARIANT_BASE;
      runtime_format_string(cue_code, FINAL_POST_RACE_SPECIAL_CUE_FORMAT, variant);
    }
  else
    {
      variant = random_below(FINAL_POST_RACE_CUE_VARIANTS) + 1;
      runtime_format_string(cue_code, FINAL_POST_RACE_DEFAULT_CUE_FORMAT, variant);
    }

  if(RACE_CONFIGURATION->host_commentary_enabled != 0)
    {
      present_frontend_host(cue_code,
                            present_frontend_frame,
                            POST_RACE_HOST_SHAPE_PATH,
                            POST_RACE_BACKGROUND_SHAPE,
                            0,
                            0);
      _start_post_race_music();
    }
  else
    {
      _start_post_race_music();
      display_shapes =
        display_frontend_shape(POST_RACE_MOVIE_SHAPE_PATH, POST_RACE_BACKGROUND_SHAPE);
      draw_frontend_status_text();
      finish_runtime_frame();
      present_runtime_frame();
      free_primary_memory(display_shapes);
      poll_controller_input(POST_RACE_INPUT_FRAME_LIMIT, 1, POST_RACE_INPUT_MASK);
    }

  if(g_POST_RACE_SAVE_ENABLED != 0)
    {
      save_post_race_results(RACE_PARTICIPANT_RECORDS, 1, 1);
      save_frontend_nvram();
    }
  display_post_race_stats(0);
  shutdown_frontend_audio();
  RACE_CONFIGURATION->event_index = 0;
  release_frontend_display();
  return 1;
}
