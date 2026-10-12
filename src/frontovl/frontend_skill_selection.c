#include "frontend_skill_selection.h"

#include "audio_stream_resources.h"
#include "controller_input.h"
#include "frontend_frame.h"
#include "frontend_skill_menu.h"
#include "frontend_sound.h"
#include "nvram_save.h"
#include "post_race_speed.h"
#include "post_race_stats.h"
#include "post_race_status.h"
#include "race_state.h"
#include "runtime_folios.h"
#include "runtime_memory.h"
#include "speech_stream_signal.h"
#include "tracked_memory.h"

#include "event.h"

#define FRONTEND_TRACK_ROUTE_SHAPE_PATH "frontend/display/route.3sh"
#define FRONTEND_TRACK_ROUTE_BACKGROUND_SHAPE "bgnd"
#define FRONTEND_TRACK_ROUTE_TEXT_SHAPE "text"
#define FRONTEND_TRACK_ROUTE_POSITION_WORD 3
#define FRONTEND_TRACK_ROUTE_POSITION_X_SHIFT 16
#define FRONTEND_TRACK_ROUTE_COUNT 3
#define FRONTEND_TRACK_ROUTE_SHAPE_COUNT 4
#define FRONTEND_TRACK_ROUTE_MARKER_COUNT 12
#define FRONTEND_TRACK_ROUTE_MARKERS_PER_TRACK 3
#define FRONTEND_TRACK_ROUTE_OPTION_MASK 0x7F
#define FRONTEND_TRACK_ROUTE_FRAME_RATE 60
#define FRONTEND_TRACK_ROUTE_INITIAL_POLL_FRAMES 120
#define FRONTEND_TRACK_ROUTE_RESULT_COUNT 3
#define FRONTEND_TRACK_ROUTE_RECORD_BYTE_COUNT 0x1C0
#define FRONTEND_TRACK_ROUTE_RESULTS_OFFSET 0x154
#define FRONTEND_TRACK_ROUTE_RESULT_BYTE_COUNT 0x1C
#define FRONTEND_TRACK_ROUTE_RESULT_CLASS_OFFSET 0x0E
#define FRONTEND_TRACK_ROUTE_RESULT_VEHICLE_OFFSET 0x0F
#define FRONTEND_TRACK_ROUTE_RESULT_VALUE_OFFSET 0x10
#define FRONTEND_TRACK_ROUTE_SUMMARY_VALUE_OFFSET 0x1C0
#define FRONTEND_TRACK_ROUTE_TEXT_BYTE_COUNT 80
#define FRONTEND_TRACK_ROUTE_VALUE_TEXT_BYTE_COUNT 12
#define FRONTEND_TRACK_ROUTE_SUMMARY_TEXT_X 0x87
#define FRONTEND_TRACK_ROUTE_SUMMARY_TEXT_Y 0x8D
#define FRONTEND_TRACK_ROUTE_RESULT_TEXT_X 0x25
#define FRONTEND_TRACK_ROUTE_RESULT_TEXT_Y 0xB4
#define FRONTEND_TRACK_ROUTE_RESULT_LINE_HEIGHT 0x0C
#define FRONTEND_TRACK_ROUTE_EMPTY_RESULT_FORMAT "%d."
#define FRONTEND_TRACK_ROUTE_RESULT_FORMAT "%d. %10s %6s %6s %s"
#define FRONTEND_TRACK_ROUTE_ALLOWED_INPUT_MASK                                                    \
  (ControlStart | ControlRightShift | ControlLeftShift)

/* Keep these accesses in the selector function that owns the retail instructions. */
#define release_frontend_track_route_shapes()       \
  do                                                \
    {                                               \
      free_primary_memory(g_FRONTEND_TRACK_ROUTE_SHAPES); \
      g_FRONTEND_TRACK_ROUTE_SHAPES = NULL;         \
    }                                               \
  while(0)

const char *g_FRONTEND_TRACK_SPEECH_PATHS[FRONTEND_TRACK_ROUTE_SHAPE_COUNT] = {
  "Alpine.22K.mw.aif",
  "Coastal.22K.mw.aif",
  "City.22K.mw.aif",
  "City.22K.mw.aif"
};
void       *g_FRONTEND_TRACK_ROUTE_SHAPES;
const char *g_FRONTEND_TRACK_ROUTE_TITLE_SHAPE_NAMES[FRONTEND_TRACK_ROUTE_SHAPE_COUNT] = { "alpt",
                                                                                           "cstt",
                                                                                           "ctyt",
                                                                                           "tstt" };
const char *g_FRONTEND_TRACK_ROUTE_MARKER_SHAPE_NAMES[FRONTEND_TRACK_ROUTE_MARKER_COUNT] = {
  "alp2", "alp2", "alp3", "cst2", "cst2", "cst3", "cty2", "cty2", "cty3", "tst2", "tst2", "tst3"
};

typedef char
  FrontendTrackSpeechPathsSizeCheck[sizeof(g_FRONTEND_TRACK_SPEECH_PATHS) == 16 ? 1 : -1];
typedef char FrontendTrackRouteTitleShapeNamesSizeCheck
  [sizeof(g_FRONTEND_TRACK_ROUTE_TITLE_SHAPE_NAMES) == 16 ? 1 : -1];
typedef char FrontendTrackRouteMarkerShapeNamesSizeCheck
  [sizeof(g_FRONTEND_TRACK_ROUTE_MARKER_SHAPE_NAMES) == 48 ? 1 : -1];

static void
_stop_frontend_track_route_speech(int32 speech_active_)
{
  if(speech_active_ != 0)
    {
      signal_speech_stream_stop();
      stop_speech_stream();
    }
}

int32
draw_frontend_track_selection(int32 track_index_)
{
  uint8 *track_record;
  uint8 *result;
  char   result_text[FRONTEND_TRACK_ROUTE_TEXT_BYTE_COUNT];
  char   value_text[FRONTEND_TRACK_ROUTE_VALUE_TEXT_BYTE_COUNT];
  void  *shape;
  int32  marker_index;
  int32  position;
  int32  result_index;

  initialize_frontend_display();
  begin_runtime_frame();
  clear_runtime_display(0);
  if(g_FRONTEND_TRACK_ROUTE_SHAPES == NULL)
    {
      g_FRONTEND_TRACK_ROUTE_SHAPES =
        load_runtime_shape_collection(FRONTEND_TRACK_ROUTE_SHAPE_PATH, 0);
      track_primary_allocation(g_FRONTEND_TRACK_ROUTE_SHAPES);
    }

  shape    = locate_runtime_shape(g_FRONTEND_TRACK_ROUTE_SHAPES,
                                  (const uint32 *) FRONTEND_TRACK_ROUTE_BACKGROUND_SHAPE);
  position = ((int32 *) shape)[FRONTEND_TRACK_ROUTE_POSITION_WORD];
  blit_runtime_shape_at_alias(shape,
                              (position >> FRONTEND_TRACK_ROUTE_POSITION_X_SHIFT),
                              (int16) position);
  shape    = locate_runtime_shape(g_FRONTEND_TRACK_ROUTE_SHAPES,
                                  (const uint32 *) FRONTEND_TRACK_ROUTE_TEXT_SHAPE);
  position = ((int32 *) shape)[FRONTEND_TRACK_ROUTE_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_TRACK_ROUTE_POSITION_X_SHIFT, (int16) position);
  shape = locate_runtime_shape(g_FRONTEND_TRACK_ROUTE_SHAPES,
                               (const uint32 *) g_FRONTEND_SKILL_TRACK_SHAPE_NAMES[track_index_]);
  position = ((int32 *) shape)[FRONTEND_TRACK_ROUTE_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_TRACK_ROUTE_POSITION_X_SHIFT, (int16) position);
  shape =
    locate_runtime_shape(g_FRONTEND_TRACK_ROUTE_SHAPES,
                         (const uint32 *) g_FRONTEND_TRACK_ROUTE_TITLE_SHAPE_NAMES[track_index_]);
  position = ((int32 *) shape)[FRONTEND_TRACK_ROUTE_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_TRACK_ROUTE_POSITION_X_SHIFT, (int16) position);
  marker_index = track_index_ * FRONTEND_TRACK_ROUTE_MARKERS_PER_TRACK +
                 ((int32) RACE_CONFIGURATION->skill_level & FRONTEND_TRACK_ROUTE_OPTION_MASK);
  shape =
    locate_runtime_shape(g_FRONTEND_TRACK_ROUTE_SHAPES,
                         (const uint32 *) g_FRONTEND_TRACK_ROUTE_MARKER_SHAPE_NAMES[marker_index]);
  position = ((int32 *) shape)[FRONTEND_TRACK_ROUTE_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_TRACK_ROUTE_POSITION_X_SHIFT, (int16) position);

  track_record = RACE_PARTICIPANT_RECORDS + track_index_ * FRONTEND_TRACK_ROUTE_RECORD_BYTE_COUNT;
  format_per_second_speed_mph(*(int32 *) (track_record + FRONTEND_TRACK_ROUTE_SUMMARY_VALUE_OFFSET),
                              result_text);
  draw_runtime_text(result_text,
                    FRONTEND_TRACK_ROUTE_SUMMARY_TEXT_X,
                    FRONTEND_TRACK_ROUTE_SUMMARY_TEXT_Y);

  for(result_index = 0; result_index < FRONTEND_TRACK_ROUTE_RESULT_COUNT; ++result_index)
    {
      result = track_record + FRONTEND_TRACK_ROUTE_RESULTS_OFFSET +
               result_index * FRONTEND_TRACK_ROUTE_RESULT_BYTE_COUNT;
      format_post_race_time(*(int32 *) (result + FRONTEND_TRACK_ROUTE_RESULT_VALUE_OFFSET),
                            value_text);
      if(*(int32 *) (result + FRONTEND_TRACK_ROUTE_RESULT_VALUE_OFFSET) == 0)
        {
          runtime_format_string(result_text,
                                FRONTEND_TRACK_ROUTE_EMPTY_RESULT_FORMAT,
                                result_index + 1);
        }
      else
        {
          runtime_format_string(
            result_text,
            FRONTEND_TRACK_ROUTE_RESULT_FORMAT,
            result_index + 1,
            result,
            value_text,
            g_POST_RACE_VEHICLE_NAMES[result[FRONTEND_TRACK_ROUTE_RESULT_VEHICLE_OFFSET]],
            g_POST_RACE_CLASS_NAMES[result[FRONTEND_TRACK_ROUTE_RESULT_CLASS_OFFSET]]);
        }
      draw_runtime_text(result_text,
                        FRONTEND_TRACK_ROUTE_RESULT_TEXT_X,
                        FRONTEND_TRACK_ROUTE_RESULT_TEXT_Y +
                          result_index * FRONTEND_TRACK_ROUTE_RESULT_LINE_HEIGHT);
    }

  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();
  return 1;
}

uint8
select_frontend_track(int32 frame_count_, uint8 track_index_)
{
  uint32 control_bits;
  uint8  initial_track_index;
  uint8  selected_track_index;
  int32  elapsed_frames;
  int32  frame_limit;
  int32  speech_active;

  initial_track_index  = track_index_;
  selected_track_index = track_index_;
  speech_active        = 0;
  elapsed_frames       = 0;
  frame_limit          = frame_count_ * FRONTEND_TRACK_ROUTE_FRAME_RATE;
  draw_frontend_track_selection(selected_track_index);

  while(elapsed_frames < frame_limit)
    {
      if(elapsed_frames == 0)
        {
          control_bits = poll_controller_input(FRONTEND_TRACK_ROUTE_INITIAL_POLL_FRAMES, 1, 0);
          if((control_bits & ~FRONTEND_TRACK_ROUTE_ALLOWED_INPUT_MASK) != 0)
            {
              play_error_sound();
              elapsed_frames = -1;
              synchronize_runtime_frame();
              elapsed_frames += 1;
              continue;
            }
        }
      else
        {
          control_bits = read_stable_controller_input();
        }

      if((control_bits & ~FRONTEND_TRACK_ROUTE_ALLOWED_INPUT_MASK) != 0)
        {
          play_error_sound();
          elapsed_frames = -1;
        }
      if(control_bits == 0)
        {
          if(speech_active == 0)
            {
              initialize_speech_stream();
              speech_active = 1;
              configure_speech_stream(g_FRONTEND_TRACK_SPEECH_PATHS[selected_track_index]);
            }
          synchronize_runtime_frame();
        }
      else
        {
          while(read_stable_controller_input() != 0)
            {
            }
          if((control_bits & ControlStart) != 0)
            {
              play_confirmation_sound();
              RACE_CONFIGURATION->track_index = selected_track_index;
              save_frontend_nvram();
              release_frontend_track_route_shapes();
              _stop_frontend_track_route_speech(speech_active);
              return selected_track_index;
            }
          if((control_bits & ControlX) != 0)
            {
              play_error_sound();
              release_frontend_track_route_shapes();
              _stop_frontend_track_route_speech(speech_active);
              return initial_track_index;
            }
          if((control_bits & ControlRightShift) != 0)
            {
              play_navigation_sound();
              selected_track_index =
                (uint8) ((selected_track_index + 1) % FRONTEND_TRACK_ROUTE_COUNT);
              elapsed_frames = -1;
              if(speech_active != 0)
                {
                  signal_speech_stream_stop();
                }
              control_bits = 0;
            }
          if((control_bits & ControlLeftShift) != 0)
            {
              play_navigation_sound();
              if(selected_track_index == 0)
                {
                  selected_track_index = FRONTEND_TRACK_ROUTE_COUNT - 1;
                }
              else
                {
                  selected_track_index -= 1;
                }
              elapsed_frames = -1;
              if(speech_active != 0)
                {
                  signal_speech_stream_stop();
                }
            }
          draw_frontend_track_selection(selected_track_index);
        }

      elapsed_frames += 1;
    }

  release_frontend_track_route_shapes();
  _stop_frontend_track_route_speech(speech_active);
  return initial_track_index;
}
