#include "frontend_checkpoint.h"

#include "audio_stream_resources.h"
#include "controller_input.h"
#include "frontend_audio_control.h"
#include "frontend_frame.h"
#include "frontend_movies.h"
#include "frontend_options.h"
#include "frontend_runtime_state.h"
#include "frontend_text_layout.h"
#include "game_standings.h"
#include "nvram_save.h"
#include "post_race_flow.h"
#include "post_race_results.h"
#include "post_race_status.h"
#include "race_event_state.h"
#include "race_state.h"
#include "random_number.h"
#include "runtime_folios.h"
#include "runtime_memory.h"
#include "runtime_memory_ops.h"
#include "tracked_memory.h"

#include "audio.h"
#include "event.h"

#define FRONTEND_CHECKPOINT_SELECTION_COUNT 6
#define FRONTEND_CHECKPOINT_LAST_SELECTION 5
#define FRONTEND_CHECKPOINT_FRAME_RATE 60
#define FRONTEND_CHECKPOINT_INPUT_MASK 0xCF000000
#define FRONTEND_CHECKPOINT_CONFIRM_MASK (ControlA | ControlB | ControlC)
#define FRONTEND_CHECKPOINT_CANCEL_MASK (ControlStart | ControlX)
#define FRONTEND_CHECKPOINT_EXIT_DELAY_TICKS 0x78
#define FRONTEND_CHECKPOINT_EXIT_SELECTION 0
#define FRONTEND_CHECKPOINT_ALTERNATE_SELECTION 1
#define FRONTEND_CHECKPOINT_CONTINUE_SELECTION 2
#define FRONTEND_CHECKPOINT_OPTIONS_SELECTION 3
#define FRONTEND_CHECKPOINT_STANDINGS_SELECTION 4
#define FRONTEND_CHECKPOINT_SAVE_SELECTION 5
#define FRONTEND_CHECKPOINT_ALTERNATE_RESULT 2
#define FRONTEND_CHECKPOINT_CONTINUE_RESULT 1
#define FRONTEND_CHECKPOINT_CUE_RANDOM_GATE 4
#define FRONTEND_CHECKPOINT_CUE_METRIC_RECORD_BYTES 0xB0
#define FRONTEND_CHECKPOINT_TRACK_RECORD_BYTES 0x1C0
#define FRONTEND_CHECKPOINT_EVENT_RECORD_BYTES 0x70
#define FRONTEND_CHECKPOINT_NO_OPPONENT_MODE 8
#define FRONTEND_CHECKPOINT_CUE_BUFFER_BYTES 8
#define FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR 2
#define FRONTEND_CHECKPOINT_CUE_VARIANT_TRIPLE 3
#define FRONTEND_CHECKPOINT_CUE_EMPTY_BYTES 1
#define FRONTEND_CHECKPOINT_CUE_SHORT_BYTES 4
#define FRONTEND_CHECKPOINT_CUE_LONG_BYTES 5
#define FRONTEND_CHECKPOINT_EVENT_PRIMARY_OFFSET 0x14
#define FRONTEND_CHECKPOINT_EVENT_SECONDARY_OFFSET 0x4C
#define FRONTEND_CHECKPOINT_EVENT_NEXT_OFFSET 0x70
#define FRONTEND_CHECKPOINT_METRIC_PRIMARY_OFFSET 0x04
#define FRONTEND_CHECKPOINT_METRIC_SPEED_OFFSET 0x0C
#define FRONTEND_CHECKPOINT_METRIC_CONTROL_OFFSET 0x10
#define FRONTEND_CHECKPOINT_METRIC_24_OFFSET 0x24
#define FRONTEND_CHECKPOINT_METRIC_2C_OFFSET 0x2C
#define FRONTEND_CHECKPOINT_METRIC_38_OFFSET 0x38
#define FRONTEND_CHECKPOINT_METRIC_3C_OFFSET 0x3C
#define FRONTEND_CHECKPOINT_METRIC_48_OFFSET 0x48
#define FRONTEND_CHECKPOINT_METRIC_50_OFFSET 0x50
#define FRONTEND_CHECKPOINT_METRIC_64_OFFSET 0x64
#define FRONTEND_CHECKPOINT_METRIC_70_OFFSET 0x70
#define FRONTEND_CHECKPOINT_METRIC_74_OFFSET 0x74
#define FRONTEND_CHECKPOINT_METRIC_7C_OFFSET 0x7C
#define FRONTEND_CHECKPOINT_METRIC_80_OFFSET 0x80
#define FRONTEND_CHECKPOINT_METRIC_84_OFFSET 0x84
#define FRONTEND_CHECKPOINT_METRIC_90_OFFSET 0x90
#define FRONTEND_CHECKPOINT_METRIC_9C_OFFSET 0x9C
#define FRONTEND_CHECKPOINT_METRIC_A4_OFFSET 0xA4
#define FRONTEND_CHECKPOINT_LEAD_THRESHOLD 0x78
#define FRONTEND_CHECKPOINT_SPEED_THRESHOLD 0x00594CCC
#define FRONTEND_CHECKPOINT_CONTROL_RANGE_START 0x7200
#define FRONTEND_CHECKPOINT_CONTROL_RANGE_END 0x724A
#define FRONTEND_CHECKPOINT_LARGE_TIME_DIFFERENCE 300
#define FRONTEND_CHECKPOINT_SMALL_TIME_DIFFERENCE 0xB4
#define FRONTEND_CHECKPOINT_REMAINING_TIME_LIMIT 0x385
#define FRONTEND_CHECKPOINT_DELTA_LOW_LIMIT 5
#define FRONTEND_CHECKPOINT_DELTA_HIGH_LIMIT 6
#define FRONTEND_CHECKPOINT_COUNT_LIMIT 3
#define FRONTEND_CHECKPOINT_COUNT_LIMIT_4 4
#define FRONTEND_CHECKPOINT_COUNT_LIMIT_6 6
#define FRONTEND_CHECKPOINT_SCORE_LIMIT 0x12D
#define FRONTEND_CHECKPOINT_CONTROL_LIMIT_30 0x1E
#define FRONTEND_CHECKPOINT_CONTROL_LIMIT_31 0x1F
#define FRONTEND_CHECKPOINT_CONTROL_LIMIT_35 0x23
#define FRONTEND_CHECKPOINT_CONTROL_LIMIT_36 0x24
#define FRONTEND_CHECKPOINT_CONTROL_LIMIT_44 0x2C
#define FRONTEND_CHECKPOINT_METRIC_LIMIT_75 0x4B
#define FRONTEND_CHECKPOINT_METRIC_LIMIT_76 0x4C
#define FRONTEND_CHECKPOINT_SPECIAL_SCORE_LIMIT 0x157D
#define FRONTEND_CHECKPOINT_SPECIAL_TRACK_INDEX 2
#define FRONTEND_CHECKPOINT_SPECIAL_SKILL_LEVEL 2
#define FRONTEND_CHECKPOINT_SKILL_LEVEL_MASK 3
#define FRONTEND_CHECKPOINT_MUSIC_CHOICE_COUNT 3
#define FRONTEND_CHECKPOINT_NEW_RESULT_LIMIT 1
#define FRONTEND_CHECKPOINT_EVENT_COUNT 3
#define FRONTEND_CHECKPOINT_EVENT_DATA_OFFSET 4
#define FRONTEND_CHECKPOINT_MOVIE_INPUT_FRAMES 0xE10
#define FRONTEND_CHECKPOINT_MOVIE_INPUT_PLAYERS 1
#define FRONTEND_CHECKPOINT_MOVIE_INPUT_MASK 0x0E000000
#define FRONTEND_CHECKPOINT_MENU_TIMEOUT 0x5A0
#define FRONTEND_CHECKPOINT_MOVIE_SHAPE_PATH "frontend/display/movie.3sh"
#define FRONTEND_CHECKPOINT_HOST_SHAPE_PATH "frontend/display/hosts/hostc"
#define FRONTEND_CHECKPOINT_BACKGROUND_SHAPE "bgnd"
#define FRONTEND_CHECKPOINT_SHAPE_POSITION_WORD 3
#define FRONTEND_CHECKPOINT_SHAPE_X_SHIFT 16
#define FRONTEND_CHECKPOINT_SHAPE_PATH "frontend/display/checkpoint.3sh"
#define FRONTEND_CHECKPOINT_MENU_SHAPE "menu"
#define FRONTEND_CHECKPOINT_SELECTOR_SHAPE "lite"

/* Keep these accesses in the menu function that owns the retail instructions. */
#define release_frontend_checkpoint_shapes()       \
  do                                               \
    {                                              \
      free_primary_memory(g_FRONTEND_CHECKPOINT_SHAPES); \
      g_FRONTEND_CHECKPOINT_SHAPES = NULL;         \
    }                                              \
  while(0)

void *g_FRONTEND_CHECKPOINT_SHAPES;
int32 g_FRONTEND_CHECKPOINT_SELECTION_X[FRONTEND_CHECKPOINT_SELECTION_COUNT] = { 0x52, 0x52, 0x52,
                                                                                 0x52, 0x52, 0x52 };
int32 g_FRONTEND_CHECKPOINT_SELECTION_Y[FRONTEND_CHECKPOINT_SELECTION_COUNT] = { 0x27, 0x43, 0x60,
                                                                                 0x7B, 0x97, 0xB3 };

typedef char
  FrontendCheckpointSelectionXSizeCheck[sizeof(g_FRONTEND_CHECKPOINT_SELECTION_X) == 24 ? 1 : -1];
typedef char
  FrontendCheckpointSelectionYSizeCheck[sizeof(g_FRONTEND_CHECKPOINT_SELECTION_Y) == 24 ? 1 : -1];

static int32
_read_checkpoint_metric(const uint8 *record_, int32 offset_)
{
  return *(const int32 *) (record_ + offset_);
}

static void
_format_checkpoint_cue(char *cue_code_, const char *format_, int32 variants_)
{
  runtime_format_string(cue_code_, format_, random_below(variants_) + 1);
}

static void
_start_ranked_checkpoint_music(void)
{
  int32 music_index;

  initialize_frontend_audio();
  music_index = random_below(FRONTEND_CHECKPOINT_MUSIC_CHOICE_COUNT);
  configure_frontend_audio_stream(g_FRONTEND_MUSIC_PATHS[music_index]);
  if(RACE_CONFIGURATION->music_enabled != 0)
    {
      resume_frontend_audio_stream();
    }
}

void
select_ranked_checkpoint_commentary_cue(char *cue_code_)
{
  const uint8 *event_record;
  const uint8 *metrics;
  int32        difference;
  int32        metric10;
  int32        metric48;
  int32        metric7_c;
  int32        metric80;
  int32        metric84;
  uint8        skill_level;

  metrics      = (const uint8 *) RACE_TIMING_STATE +
                 RACE_CONFIGURATION->event_index * FRONTEND_CHECKPOINT_CUE_METRIC_RECORD_BYTES;
  event_record = RACE_PARTICIPANT_RECORDS +
                 RACE_CONFIGURATION->track_index * FRONTEND_CHECKPOINT_TRACK_RECORD_BYTES +
                 RACE_CONFIGURATION->event_index * FRONTEND_CHECKPOINT_EVENT_RECORD_BYTES;
  copy_runtime_memory(cue_code_, "", FRONTEND_CHECKPOINT_CUE_EMPTY_BYTES);

  if(RACE_CONFIGURATION->opponent_vehicle_index == FRONTEND_CHECKPOINT_NO_OPPONENT_MODE)
    {
      difference = _read_checkpoint_metric(event_record, FRONTEND_CHECKPOINT_EVENT_PRIMARY_OFFSET) -
                   _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_PRIMARY_OFFSET);
      if(difference > FRONTEND_CHECKPOINT_LEAD_THRESHOLD)
        {
          _format_checkpoint_cue(cue_code_, "1.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_TRIPLE);
          return;
        }
      if(difference > 0)
        {
          _format_checkpoint_cue(cue_code_, "2.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR);
          return;
        }
      if(_read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_SPEED_OFFSET) >=
         FRONTEND_CHECKPOINT_SPEED_THRESHOLD)
        {
          copy_runtime_memory(cue_code_, "6.3", FRONTEND_CHECKPOINT_CUE_SHORT_BYTES);
          return;
        }
      if(_read_checkpoint_metric(event_record, FRONTEND_CHECKPOINT_EVENT_SECONDARY_OFFSET) >
         _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_PRIMARY_OFFSET))
        {
          copy_runtime_memory(cue_code_, "2.3", FRONTEND_CHECKPOINT_CUE_SHORT_BYTES);
          return;
        }
      if(_read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_SPEED_OFFSET) >
         _read_checkpoint_metric(event_record, FRONTEND_CHECKPOINT_EVENT_NEXT_OFFSET))
        {
          _format_checkpoint_cue(cue_code_, "3.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR);
          return;
        }
      metric10 = _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_CONTROL_OFFSET);
      if(metric10 >= FRONTEND_CHECKPOINT_CONTROL_RANGE_START &&
         metric10 < FRONTEND_CHECKPOINT_CONTROL_RANGE_END)
        {
          _format_checkpoint_cue(cue_code_, "5.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_TRIPLE);
          if(random_below(FRONTEND_CHECKPOINT_CUE_RANDOM_GATE) != 0)
            {
              return;
            }
        }
    }
  else
    {
      difference = _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_9C_OFFSET) -
                   _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_PRIMARY_OFFSET);
      if(difference > FRONTEND_CHECKPOINT_LARGE_TIME_DIFFERENCE)
        {
          _format_checkpoint_cue(cue_code_, "7.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_TRIPLE);
          return;
        }
      if(difference > 0 && difference < FRONTEND_CHECKPOINT_SMALL_TIME_DIFFERENCE)
        {
          _format_checkpoint_cue(cue_code_, "8.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR);
          return;
        }
      if(_read_checkpoint_metric(event_record, FRONTEND_CHECKPOINT_EVENT_PRIMARY_OFFSET) >
         _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_PRIMARY_OFFSET))
        {
          _format_checkpoint_cue(cue_code_, "2.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR);
          return;
        }
      if(_read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_SPEED_OFFSET) >=
         FRONTEND_CHECKPOINT_SPEED_THRESHOLD)
        {
          copy_runtime_memory(cue_code_, "6.3", FRONTEND_CHECKPOINT_CUE_SHORT_BYTES);
          return;
        }
      if(_read_checkpoint_metric(event_record, FRONTEND_CHECKPOINT_EVENT_SECONDARY_OFFSET) >
         _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_PRIMARY_OFFSET))
        {
          copy_runtime_memory(cue_code_, "2.3", FRONTEND_CHECKPOINT_CUE_SHORT_BYTES);
          return;
        }
      if(_read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_PRIMARY_OFFSET) -
           _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_9C_OFFSET) <
         FRONTEND_CHECKPOINT_REMAINING_TIME_LIMIT)
        {
          difference = _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_SPEED_OFFSET) -
                       _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_A4_OFFSET);
          if(difference >= FRONTEND_CHECKPOINT_DELTA_HIGH_LIMIT &&
             _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_9C_OFFSET) >
               _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_PRIMARY_OFFSET))
            {
              _format_checkpoint_cue(cue_code_, "12.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_TRIPLE);
              return;
            }
          if(_read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_3C_OFFSET) <
             FRONTEND_CHECKPOINT_SCORE_LIMIT)
            {
              if(_read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_24_OFFSET) <
                 FRONTEND_CHECKPOINT_COUNT_LIMIT_6)
                {
                  if(_read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_2C_OFFSET) <
                     FRONTEND_CHECKPOINT_COUNT_LIMIT)
                    {
                      goto select_secondary_cue;
                    }
                  _format_checkpoint_cue(cue_code_,
                                         "14.%d",
                                         FRONTEND_CHECKPOINT_CUE_VARIANT_TRIPLE);
                }
              else
                {
                  _format_checkpoint_cue(cue_code_,
                                         "13.%d",
                                         FRONTEND_CHECKPOINT_CUE_VARIANT_TRIPLE);
                }
            }
          else
            {
              copy_runtime_memory(cue_code_, "15.1", FRONTEND_CHECKPOINT_CUE_LONG_BYTES);
            }
        }
      else
        {
          _format_checkpoint_cue(cue_code_, "11.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_TRIPLE);
        }
      if(random_below(FRONTEND_CHECKPOINT_CUE_RANDOM_GATE) != 0)
        {
          return;
        }
    }

select_secondary_cue:
  metric10 = _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_CONTROL_OFFSET);
  if(_read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_90_OFFSET) >=
     FRONTEND_CHECKPOINT_COUNT_LIMIT)
    {
      _format_checkpoint_cue(cue_code_, "34.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_TRIPLE);
      return;
    }
  metric84 = _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_84_OFFSET);
  if(metric84 > 0 && metric10 > FRONTEND_CHECKPOINT_CONTROL_LIMIT_36)
    {
      _format_checkpoint_cue(cue_code_, "32.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR);
      return;
    }
  metric80 = _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_80_OFFSET);
  if(!(metric80 < FRONTEND_CHECKPOINT_METRIC_LIMIT_76 ||
       metric10 > FRONTEND_CHECKPOINT_CONTROL_LIMIT_36))
    {
      _format_checkpoint_cue(cue_code_, "28.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_TRIPLE);
      return;
    }
  if(!(metric84 < FRONTEND_CHECKPOINT_CUE_EMPTY_BYTES ||
       metric10 > FRONTEND_CHECKPOINT_CONTROL_LIMIT_36))
    {
      _format_checkpoint_cue(cue_code_, "33.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_TRIPLE);
      return;
    }
  if(_read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_38_OFFSET) >=
     FRONTEND_CHECKPOINT_COUNT_LIMIT_4)
    {
      _format_checkpoint_cue(cue_code_, "23.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR);
      return;
    }
  if(metric80 > FRONTEND_CHECKPOINT_METRIC_LIMIT_75 &&
     metric10 > FRONTEND_CHECKPOINT_CONTROL_LIMIT_36)
    {
      _format_checkpoint_cue(cue_code_, "27.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR);
      return;
    }
  if(!(_read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_70_OFFSET) <
         FRONTEND_CHECKPOINT_SCORE_LIMIT ||
       metric10 > FRONTEND_CHECKPOINT_CONTROL_LIMIT_35))
    {
      _format_checkpoint_cue(cue_code_, "24.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_TRIPLE);
      return;
    }
  metric7_c = _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_7C_OFFSET);
  if(metric7_c >= FRONTEND_CHECKPOINT_COUNT_LIMIT)
    {
      _format_checkpoint_cue(cue_code_, "30.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_TRIPLE);
      return;
    }
  if(RACE_CONFIGURATION->track_index != FRONTEND_CHECKPOINT_SPECIAL_TRACK_INDEX)
    {
      metric48 = _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_48_OFFSET);
      if(metric48 > FRONTEND_CHECKPOINT_CONTROL_LIMIT_30)
        {
          metric48 = metric10;
        }
      if(metric48 > FRONTEND_CHECKPOINT_CONTROL_LIMIT_30)
        {
          _format_checkpoint_cue(cue_code_, "16.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR);
          return;
        }
    }
  metric48 = _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_48_OFFSET);
  if(!(metric48 < FRONTEND_CHECKPOINT_CONTROL_LIMIT_31 ||
       metric10 > FRONTEND_CHECKPOINT_CONTROL_LIMIT_30))
    {
      _format_checkpoint_cue(cue_code_, "17.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR);
      return;
    }
  if(metric48 < FRONTEND_CHECKPOINT_CONTROL_LIMIT_31)
    {
      metric48 = metric10;
    }
  if(metric48 < FRONTEND_CHECKPOINT_CONTROL_LIMIT_31)
    {
      _format_checkpoint_cue(cue_code_, "18.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR);
      return;
    }
  if(!(_read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_74_OFFSET) <
         FRONTEND_CHECKPOINT_METRIC_LIMIT_76 ||
       metric10 > FRONTEND_CHECKPOINT_CONTROL_LIMIT_36))
    {
      _format_checkpoint_cue(cue_code_, "26.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR);
      return;
    }
  if(!(_read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_50_OFFSET) <
         FRONTEND_CHECKPOINT_SCORE_LIMIT ||
       metric10 > FRONTEND_CHECKPOINT_CONTROL_LIMIT_36))
    {
      _format_checkpoint_cue(cue_code_, "20.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR);
      return;
    }
  skill_level = RACE_CONFIGURATION->skill_level & FRONTEND_CHECKPOINT_SKILL_LEVEL_MASK;
  if(skill_level == FRONTEND_CHECKPOINT_SPECIAL_SKILL_LEVEL &&
     _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_64_OFFSET) <
       FRONTEND_CHECKPOINT_SPECIAL_SCORE_LIMIT &&
     metric10 < FRONTEND_CHECKPOINT_CONTROL_LIMIT_36)
    {
      _format_checkpoint_cue(cue_code_, "22.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR);
      return;
    }
  if(skill_level == FRONTEND_CHECKPOINT_SPECIAL_SKILL_LEVEL &&
     _read_checkpoint_metric(metrics, FRONTEND_CHECKPOINT_METRIC_64_OFFSET) >=
       FRONTEND_CHECKPOINT_SPECIAL_SCORE_LIMIT &&
     metric10 <= FRONTEND_CHECKPOINT_CONTROL_LIMIT_44)
    {
      _format_checkpoint_cue(cue_code_, "21.%d", FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR);
      return;
    }
  if(metric7_c < FRONTEND_CHECKPOINT_CUE_VARIANT_PAIR)
    {
      copy_runtime_memory(cue_code_, "20.2", FRONTEND_CHECKPOINT_CUE_LONG_BYTES);
    }
  else
    {
      copy_runtime_memory(cue_code_, "31.1", FRONTEND_CHECKPOINT_CUE_LONG_BYTES);
    }
}

int32
run_ranked_checkpoint_flow(void)
{
  char  cue_code[FRONTEND_CHECKPOINT_CUE_BUFFER_BYTES];
  void *display_shapes;
  int32 initial_result;
  int32 menu_result;
  int32 music_started;

  music_started = 0;
  begin_runtime_frame();
  synchronize_runtime_frame();
  finish_runtime_frame();
  clear_runtime_display(0);
  finish_runtime_frame();
  initialize_frontend_stamp();
  initialize_frontend_display();
  configure_compact_movie_stream_buffers();
  format_ranked_post_race_status(g_FRONTEND_OVERLAY_TEXT_A,
                                 g_FRONTEND_OVERLAY_TEXT_B,
                                 g_FRONTEND_OVERLAY_TEXT_D,
                                 g_FRONTEND_OVERLAY_TEXT_C);

  initial_result = *g_POST_RACE_RESULT_SLOT;
  if(initial_result < FRONTEND_CHECKPOINT_NEW_RESULT_LIMIT)
    {
      if(RACE_CONFIGURATION->host_commentary_enabled != 0)
        {
          select_ranked_checkpoint_commentary_cue(cue_code);
          if(cue_code[0] != 0)
            {
              present_frontend_host(cue_code,
                                    present_frontend_host_frame,
                                    FRONTEND_CHECKPOINT_HOST_SHAPE_PATH,
                                    FRONTEND_CHECKPOINT_BACKGROUND_SHAPE,
                                    0,
                                    0);
            }
        }
      else
        {
          _start_ranked_checkpoint_music();
          music_started  = 1;
          display_shapes = display_frontend_shape(FRONTEND_CHECKPOINT_MOVIE_SHAPE_PATH,
                                                  FRONTEND_CHECKPOINT_BACKGROUND_SHAPE);
          draw_frontend_movie_overlay_text();
          finish_runtime_frame();
          present_runtime_frame();
          free_primary_memory(display_shapes);
          poll_controller_input(FRONTEND_CHECKPOINT_MOVIE_INPUT_FRAMES,
                                FRONTEND_CHECKPOINT_MOVIE_INPUT_PLAYERS,
                                FRONTEND_CHECKPOINT_MOVIE_INPUT_MASK);
        }
    }

  if(is_late_event_or_novice_skill() != 0)
    {
      if(music_started != 0)
        {
          shutdown_frontend_audio();
        }
      return run_post_race_flow();
    }

  if(music_started == 0)
    {
      _start_ranked_checkpoint_music();
    }

  if(g_POST_RACE_SAVE_ENABLED != 0 &&
     *g_POST_RACE_RESULT_SLOT < FRONTEND_CHECKPOINT_NEW_RESULT_LIMIT)
    {
      update_post_race_records(
        RACE_PARTICIPANT_RECORDS +
        RACE_CONFIGURATION->track_index * FRONTEND_CHECKPOINT_TRACK_RECORD_BYTES +
        RACE_CONFIGURATION->event_index * FRONTEND_CHECKPOINT_EVENT_RECORD_BYTES +
        FRONTEND_CHECKPOINT_EVENT_DATA_OFFSET);
      save_frontend_nvram();
    }

  menu_result = run_frontend_checkpoint_menu(FRONTEND_CHECKPOINT_MENU_TIMEOUT);
  shutdown_frontend_audio();
  if(*g_POST_RACE_RESULT_SLOT < FRONTEND_CHECKPOINT_NEW_RESULT_LIMIT)
    {
      RACE_CONFIGURATION->event_index =
        (RACE_CONFIGURATION->event_index + 1) % FRONTEND_CHECKPOINT_EVENT_COUNT;
    }
  release_frontend_display();
  return menu_result;
}

int32
draw_frontend_checkpoint_menu(int32 selection_index_)
{
  void *shape;
  int32 packed_position;

  begin_runtime_frame();
  synchronize_runtime_frame();
  clear_runtime_display(0);
  if(g_FRONTEND_CHECKPOINT_SHAPES == NULL)
    {
      g_FRONTEND_CHECKPOINT_SHAPES = load_runtime_file_data(FRONTEND_CHECKPOINT_SHAPE_PATH, 0);
      track_primary_allocation(g_FRONTEND_CHECKPOINT_SHAPES);
    }

  shape           = locate_runtime_shape(g_FRONTEND_CHECKPOINT_SHAPES,
                                         (const uint32 *) FRONTEND_CHECKPOINT_MENU_SHAPE);
  packed_position = ((int32 *) shape)[FRONTEND_CHECKPOINT_SHAPE_POSITION_WORD];
  blit_runtime_shape_at(shape,
                        packed_position >> FRONTEND_CHECKPOINT_SHAPE_X_SHIFT,
                        (int16) packed_position);

  shape = locate_runtime_shape(g_FRONTEND_CHECKPOINT_SHAPES,
                               (const uint32 *) FRONTEND_CHECKPOINT_SELECTOR_SHAPE);
  draw_runtime_shape_at(shape,
                        g_FRONTEND_CHECKPOINT_SELECTION_X[selection_index_],
                        g_FRONTEND_CHECKPOINT_SELECTION_Y[selection_index_]);

  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();
  return 1;
}

int32
run_frontend_checkpoint_menu(int32 timeout_seconds_)
{
  uint32 control_bits;
  int32  selection_index;
  int32  timeout_frames;

  g_FRONTEND_CHECKPOINT_SHAPES = NULL;
  *g_POST_RACE_RESULT_SLOT     = 0;
  begin_runtime_frame();
  synchronize_runtime_frame();
  clear_runtime_display(0);
  finish_runtime_frame();
  present_runtime_frame();

  selection_index = 0;
  draw_frontend_checkpoint_menu(selection_index);
  timeout_frames = timeout_seconds_ * FRONTEND_CHECKPOINT_FRAME_RATE;

  for(;;)
    {
      control_bits = poll_controller_input(timeout_frames, 1, FRONTEND_CHECKPOINT_INPUT_MASK);
      if(control_bits == 0)
        {
          release_frontend_checkpoint_shapes();
          return 1;
        }

      if((control_bits & ControlDown) != 0)
        {
          selection_index = (selection_index + 1) % FRONTEND_CHECKPOINT_SELECTION_COUNT;
        }
      if((control_bits & ControlUp) != 0)
        {
          if(selection_index != 0)
            {
              --selection_index;
            }
          else
            {
              selection_index = FRONTEND_CHECKPOINT_LAST_SELECTION;
            }
        }

      if((control_bits & FRONTEND_CHECKPOINT_CONFIRM_MASK) != 0)
        {
          switch(selection_index)
            {
              case FRONTEND_CHECKPOINT_EXIT_SELECTION:
                release_frontend_checkpoint_shapes();
                SleepAudioTicks(FRONTEND_CHECKPOINT_EXIT_DELAY_TICKS);
                return 1;

              case FRONTEND_CHECKPOINT_ALTERNATE_SELECTION:
                *g_POST_RACE_RESULT_SLOT = FRONTEND_CHECKPOINT_ALTERNATE_RESULT;
                release_frontend_checkpoint_shapes();
                return 1;

              case FRONTEND_CHECKPOINT_CONTINUE_SELECTION:
                *g_POST_RACE_RESULT_SLOT = FRONTEND_CHECKPOINT_CONTINUE_RESULT;
                release_frontend_checkpoint_shapes();
                return 1;

              case FRONTEND_CHECKPOINT_OPTIONS_SELECTION:
                run_frontend_options();
                control_bits = 0;
                break;

              case FRONTEND_CHECKPOINT_STANDINGS_SELECTION:
                release_frontend_checkpoint_shapes();
                run_game_standings();
                control_bits = 0;
                break;

              case FRONTEND_CHECKPOINT_SAVE_SELECTION:
                if(g_POST_RACE_SAVE_ENABLED != 0)
                  {
                    update_post_race_records(RACE_PARTICIPANT_RECORDS);
                    save_frontend_nvram();
                  }
                release_frontend_checkpoint_shapes();
                return 0;
            }
        }

      if((control_bits & FRONTEND_CHECKPOINT_CANCEL_MASK) != 0)
        {
          release_frontend_checkpoint_shapes();
          SleepAudioTicks(FRONTEND_CHECKPOINT_EXIT_DELAY_TICKS);
          return 1;
        }
      draw_frontend_checkpoint_menu(selection_index);
    }
}
