#include "frontend_vehicle_selection.h"

#include "audio_stream_resources.h"
#include "controller_input.h"
#include "frontend_car_selection.h"
#include "frontend_frame.h"
#include "frontend_sound.h"
#include "runtime_folios.h"
#include "runtime_memory.h"
#include "speech_stream_signal.h"
#include "tracked_memory.h"

#include "event.h"

#define FRONTEND_VEHICLE_COUNT 8
#define FRONTEND_VEHICLE_INDEX_MASK 7U
#define FRONTEND_CAR_LONG_NAME_BYTES 8
#define FRONTEND_CAR_SHORT_NAME_BYTES 4
#define FRONTEND_CAR_STATS_ROOT_BYTES 28
#define FRONTEND_CAR_STATS_LONG_PATH_BYTES 36
#define FRONTEND_CAR_STATS_SHORT_PATH_BYTES 32
#define FRONTEND_CAR_SPEECH_LONG_PATH_BYTES 20
#define FRONTEND_CAR_SPEECH_SHORT_PATH_BYTES 16
#define FRONTEND_VEHICLE_BACKGROUND_SHAPE "bgnd"
#define FRONTEND_VEHICLE_TEXT_SHAPE "text"
#define FRONTEND_VEHICLE_TOP_COVER_SHAPE "topc"
#define FRONTEND_VEHICLE_BOTTOM_COVER_SHAPE "botc"
#define FRONTEND_VEHICLE_UP_ARROW_SHAPE "upar"
#define FRONTEND_VEHICLE_DOWN_ARROW_SHAPE "dnar"
#define FRONTEND_VEHICLE_CAR_SHAPE "car "
#define FRONTEND_VEHICLE_TITLE_SHAPE "titl"
#define FRONTEND_VEHICLE_POSITION_WORD 3
#define FRONTEND_VEHICLE_POSITION_X_SHIFT 16
#define FRONTEND_VEHICLE_HEIGHT_WORD 1
#define FRONTEND_VEHICLE_HEIGHT_SIGN_SHIFT 20
#define FRONTEND_VEHICLE_VISIBLE_LINE_ADJUSTMENT 2
#define FRONTEND_VEHICLE_CAR_Y_ADJUSTMENT 4
#define FRONTEND_VEHICLE_FRAME_RATE 60
#define FRONTEND_VEHICLE_INITIAL_POLL_FRAMES 120
#define FRONTEND_VEHICLE_CONFIRM_INPUT_MASK (ControlA | ControlB | ControlC)
#define FRONTEND_VEHICLE_ALLOWED_INPUT_MASK                                                        \
  (ControlDown | ControlUp | FRONTEND_VEHICLE_CONFIRM_INPUT_MASK | ControlStart | ControlX |       \
   ControlRightShift | ControlLeftShift)
#define FRONTEND_VEHICLE_NONREPEAT_INPUT_MASK (~(ControlDown | ControlUp))

typedef struct FrontendCarShapeNameStorage
{
  char supra[FRONTEND_CAR_LONG_NAME_BYTES];
  char diablo[FRONTEND_CAR_LONG_NAME_BYTES];
  char porsche_911[FRONTEND_CAR_SHORT_NAME_BYTES];
  char vette[FRONTEND_CAR_LONG_NAME_BYTES];
  char ferrari_512tr[FRONTEND_CAR_LONG_NAME_BYTES];
  char viper[FRONTEND_CAR_LONG_NAME_BYTES];
  char nsx[FRONTEND_CAR_SHORT_NAME_BYTES];
  char rx7[FRONTEND_CAR_SHORT_NAME_BYTES];
} FrontendCarShapeNameStorage;

typedef struct FrontendVehicleAssetPathStorage
{
  char stats_root[FRONTEND_CAR_STATS_ROOT_BYTES];
  char supra_stats[FRONTEND_CAR_STATS_LONG_PATH_BYTES];
  char diablo_stats[FRONTEND_CAR_STATS_LONG_PATH_BYTES];
  char porsche_911_stats[FRONTEND_CAR_STATS_SHORT_PATH_BYTES];
  char vette_stats[FRONTEND_CAR_STATS_LONG_PATH_BYTES];
  char ferrari_512_stats[FRONTEND_CAR_STATS_SHORT_PATH_BYTES];
  char viper_stats[FRONTEND_CAR_STATS_LONG_PATH_BYTES];
  char nsx_stats[FRONTEND_CAR_STATS_SHORT_PATH_BYTES];
  char rx7_stats[FRONTEND_CAR_STATS_SHORT_PATH_BYTES];
  char supra_speech[FRONTEND_CAR_SPEECH_LONG_PATH_BYTES];
  char diablo_speech[FRONTEND_CAR_SPEECH_LONG_PATH_BYTES];
  char porsche_911_speech[FRONTEND_CAR_SPEECH_SHORT_PATH_BYTES];
  char corvette_speech[FRONTEND_CAR_SPEECH_SHORT_PATH_BYTES];
  char ferrari_512tr_speech[FRONTEND_CAR_SPEECH_LONG_PATH_BYTES];
  char viper_speech[FRONTEND_CAR_SPEECH_LONG_PATH_BYTES];
  char nsx_speech[FRONTEND_CAR_SPEECH_SHORT_PATH_BYTES];
  char rx7_speech[FRONTEND_CAR_SPEECH_SHORT_PATH_BYTES];
} FrontendVehicleAssetPathStorage;

static const FrontendCarShapeNameStorage G_FRONTEND_CAR_SHAPE_NAME_STORAGE = {
  "supra", "diablo", "911", "vette", "512tr", "viper", "nsx", "rx7"
};

static const FrontendVehicleAssetPathStorage G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE = {
  "frontend/display/stats.3sh",
  "frontend/display/stats/supra.3sh",
  "frontend/display/stats/diablo.3sh",
  "frontend/display/stats/911.3sh",
  "frontend/display/stats/vette.3sh",
  "frontend/display/stats/512.3sh",
  "frontend/display/stats/viper.3sh",
  "frontend/display/stats/nsx.3sh",
  "frontend/display/stats/rx7.3sh",
  "Supra.22K.mw.aif",
  "Diablo.22K.mw.aif",
  "911.22K.mw.aif",
  "ZR1.22K.mw.aif",
  "512tr.22K.mw.aif",
  "Viper.22K.mw.aif",
  "NSX.22K.mw.aif",
  "RX7.22K.mw.aif"
};

const char *g_FRONTEND_CAR_SHAPE_NAMES[FRONTEND_VEHICLE_COUNT] = {
  G_FRONTEND_CAR_SHAPE_NAME_STORAGE.supra,         G_FRONTEND_CAR_SHAPE_NAME_STORAGE.diablo,
  G_FRONTEND_CAR_SHAPE_NAME_STORAGE.porsche_911,   G_FRONTEND_CAR_SHAPE_NAME_STORAGE.vette,
  G_FRONTEND_CAR_SHAPE_NAME_STORAGE.ferrari_512tr, G_FRONTEND_CAR_SHAPE_NAME_STORAGE.viper,
  G_FRONTEND_CAR_SHAPE_NAME_STORAGE.nsx,           G_FRONTEND_CAR_SHAPE_NAME_STORAGE.rx7
};
const char *g_FRONTEND_CAR_STATS_SHAPE_PATH = G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.stats_root;
const char *g_FRONTEND_CAR_STATS_SHAPE_PATHS[FRONTEND_VEHICLE_COUNT] = {
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.supra_stats,
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.diablo_stats,
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.porsche_911_stats,
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.vette_stats,
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.ferrari_512_stats,
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.viper_stats,
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.nsx_stats,
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.rx7_stats
};
const char *g_FRONTEND_CAR_SPEECH_PATHS[FRONTEND_VEHICLE_COUNT] = {
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.supra_speech,
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.diablo_speech,
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.porsche_911_speech,
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.corvette_speech,
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.ferrari_512tr_speech,
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.viper_speech,
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.nsx_speech,
  G_FRONTEND_VEHICLE_ASSET_PATH_STORAGE.rx7_speech
};
void *g_FRONTEND_VEHICLE_DETAIL_SHAPES;
void *g_FRONTEND_VEHICLE_STATS_SHAPES;

typedef char FrontendCarShapeNamesSizeCheck[sizeof(g_FRONTEND_CAR_SHAPE_NAMES) == 32 ? 1 : -1];
typedef char
  FrontendCarStatsShapePathsSizeCheck[sizeof(g_FRONTEND_CAR_STATS_SHAPE_PATHS) == 32 ? 1 : -1];
typedef char FrontendCarSpeechPathsSizeCheck[sizeof(g_FRONTEND_CAR_SPEECH_PATHS) == 32 ? 1 : -1];

static void
_stop_frontend_vehicle_speech(int32 speech_active_)
{
  if(speech_active_ != 0)
    {
      signal_speech_stream_stop();
      stop_speech_stream();
    }
}

int32
draw_frontend_vehicle_selection(int32 vehicle_index_, int32 scroll_offset_)
{
  void *background_shape;
  void *shape;
  void *text_shape;
  int32 line_count;
  int32 position;

  begin_runtime_frame();
  clear_runtime_display(0);
  if(g_FRONTEND_VEHICLE_DETAIL_SHAPES == NULL)
    {
      g_FRONTEND_VEHICLE_DETAIL_SHAPES =
        load_runtime_shape_collection(g_FRONTEND_CAR_STATS_SHAPE_PATHS[vehicle_index_], 0);
      track_primary_allocation(g_FRONTEND_VEHICLE_DETAIL_SHAPES);
    }
  if(g_FRONTEND_VEHICLE_STATS_SHAPES == NULL)
    {
      g_FRONTEND_VEHICLE_STATS_SHAPES =
        load_runtime_shape_collection(g_FRONTEND_CAR_STATS_SHAPE_PATH, 0);
      track_primary_allocation(g_FRONTEND_VEHICLE_STATS_SHAPES);
    }

  background_shape = locate_runtime_shape(g_FRONTEND_VEHICLE_STATS_SHAPES,
                                          (const uint32 *) FRONTEND_VEHICLE_BACKGROUND_SHAPE);
  position         = ((int32 *) background_shape)[FRONTEND_VEHICLE_POSITION_WORD];
  blit_runtime_shape_at(background_shape,
                        position >> FRONTEND_VEHICLE_POSITION_X_SHIFT,
                        (int16) position);
  text_shape = locate_runtime_shape(g_FRONTEND_VEHICLE_DETAIL_SHAPES,
                                    (const uint32 *) FRONTEND_VEHICLE_TEXT_SHAPE);
  position   = ((int32 *) text_shape)[FRONTEND_VEHICLE_POSITION_WORD];
  draw_runtime_shape_at(text_shape,
                        position >> FRONTEND_VEHICLE_POSITION_X_SHIFT,
                        (int16) position - scroll_offset_);
  shape    = locate_runtime_shape(g_FRONTEND_VEHICLE_STATS_SHAPES,
                                  (const uint32 *) FRONTEND_VEHICLE_TOP_COVER_SHAPE);
  position = ((int32 *) shape)[FRONTEND_VEHICLE_POSITION_WORD];
  blit_runtime_shape_at(shape, position >> FRONTEND_VEHICLE_POSITION_X_SHIFT, (int16) position);
  shape    = locate_runtime_shape(g_FRONTEND_VEHICLE_STATS_SHAPES,
                                  (const uint32 *) FRONTEND_VEHICLE_BOTTOM_COVER_SHAPE);
  position = ((int32 *) shape)[FRONTEND_VEHICLE_POSITION_WORD];
  blit_runtime_shape_at(shape, position >> FRONTEND_VEHICLE_POSITION_X_SHIFT, (int16) position);
  if(scroll_offset_ != 0)
    {
      shape    = locate_runtime_shape(g_FRONTEND_VEHICLE_STATS_SHAPES,
                                      (const uint32 *) FRONTEND_VEHICLE_UP_ARROW_SHAPE);
      position = ((int32 *) shape)[FRONTEND_VEHICLE_POSITION_WORD];
      blit_runtime_shape_at(shape, position >> FRONTEND_VEHICLE_POSITION_X_SHIFT, (int16) position);
    }

  line_count = ((int32) (((uint32 *) text_shape)[FRONTEND_VEHICLE_HEIGHT_WORD]
                         << FRONTEND_VEHICLE_HEIGHT_SIGN_SHIFT) >>
                FRONTEND_VEHICLE_HEIGHT_SIGN_SHIFT) -
               ((int32) (((uint32 *) background_shape)[FRONTEND_VEHICLE_HEIGHT_WORD]
                         << FRONTEND_VEHICLE_HEIGHT_SIGN_SHIFT) >>
                FRONTEND_VEHICLE_HEIGHT_SIGN_SHIFT) +
               FRONTEND_VEHICLE_VISIBLE_LINE_ADJUSTMENT;
  if(scroll_offset_ < line_count)
    {
      shape    = locate_runtime_shape(g_FRONTEND_VEHICLE_STATS_SHAPES,
                                      (const uint32 *) FRONTEND_VEHICLE_DOWN_ARROW_SHAPE);
      position = ((int32 *) shape)[FRONTEND_VEHICLE_POSITION_WORD];
      blit_runtime_shape_at(shape, position >> FRONTEND_VEHICLE_POSITION_X_SHIFT, (int16) position);
    }
  shape    = locate_runtime_shape(g_FRONTEND_VEHICLE_DETAIL_SHAPES,
                                  (const uint32 *) FRONTEND_VEHICLE_CAR_SHAPE);
  position = ((int32 *) shape)[FRONTEND_VEHICLE_POSITION_WORD];
  draw_runtime_shape_at(shape,
                        position >> FRONTEND_VEHICLE_POSITION_X_SHIFT,
                        (int16) position - FRONTEND_VEHICLE_CAR_Y_ADJUSTMENT);
  shape    = locate_runtime_shape(g_FRONTEND_VEHICLE_DETAIL_SHAPES,
                                  (const uint32 *) FRONTEND_VEHICLE_TITLE_SHAPE);
  position = ((int32 *) shape)[FRONTEND_VEHICLE_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_VEHICLE_POSITION_X_SHIFT, (int16) position);
  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();
  return line_count;
}

uint8
select_frontend_vehicle(int32 frame_count_, uint8 vehicle_index_)
{
  uint32 control_bits;
  uint8  initial_vehicle_index;
  uint8  selected_vehicle_index;
  int32  elapsed_frames;
  int32  frame_limit;
  int32  line_count;
  int32  scroll_offset;
  int32  speech_active;

  initial_vehicle_index  = vehicle_index_;
  selected_vehicle_index = vehicle_index_;
  scroll_offset          = 0;
  speech_active          = 0;
  line_count             = draw_frontend_vehicle_selection(selected_vehicle_index, scroll_offset);
  elapsed_frames         = 0;
  frame_limit            = frame_count_ * FRONTEND_VEHICLE_FRAME_RATE;

  while(elapsed_frames < frame_limit)
    {
      if(elapsed_frames == 0)
        {
          control_bits = poll_controller_input(FRONTEND_VEHICLE_INITIAL_POLL_FRAMES, 0, 0);
          if((control_bits & ~FRONTEND_VEHICLE_ALLOWED_INPUT_MASK) != 0)
            {
              play_error_sound();
              elapsed_frames = -1;
              synchronize_runtime_frame();
              elapsed_frames += 1;
              continue;
            }
          if(control_bits == 0 || (control_bits & (ControlDown | ControlUp)) != 0)
            {
              if(speech_active == 0)
                {
                  initialize_speech_stream();
                  speech_active = 1;
                }
              configure_speech_stream(g_FRONTEND_CAR_SPEECH_PATHS[selected_vehicle_index]);
            }
          else
            {
              while((read_stable_controller_input() & FRONTEND_VEHICLE_NONREPEAT_INPUT_MASK) != 0)
                {
                }
            }
        }
      else
        {
          control_bits = read_stable_controller_input();
          if((control_bits & ~FRONTEND_VEHICLE_ALLOWED_INPUT_MASK) != 0)
            {
              play_error_sound();
              elapsed_frames = -1;
            }
          if((control_bits & FRONTEND_VEHICLE_NONREPEAT_INPUT_MASK) != 0)
            {
              while((read_stable_controller_input() & FRONTEND_VEHICLE_NONREPEAT_INPUT_MASK) != 0)
                {
                }
            }
        }

      if((control_bits & ControlRightShift) != 0)
        {
          elapsed_frames = -1;
          selected_vehicle_index =
            (uint8) (((uint32) selected_vehicle_index + 1U) & FRONTEND_VEHICLE_INDEX_MASK);
          play_navigation_sound();
          free_primary_memory(g_FRONTEND_VEHICLE_DETAIL_SHAPES);
          g_FRONTEND_VEHICLE_DETAIL_SHAPES = NULL;
          if(speech_active != 0)
            {
              signal_speech_stream_stop();
            }
          control_bits = 0;
        }
      if((control_bits & ControlLeftShift) != 0)
        {
          elapsed_frames = -1;
          if(selected_vehicle_index == 0)
            {
              selected_vehicle_index = FRONTEND_VEHICLE_COUNT - 1;
            }
          else
            {
              selected_vehicle_index -= 1;
            }
          play_navigation_sound();
          free_primary_memory(g_FRONTEND_VEHICLE_DETAIL_SHAPES);
          g_FRONTEND_VEHICLE_DETAIL_SHAPES = NULL;
          if(speech_active != 0)
            {
              signal_speech_stream_stop();
            }
          control_bits = 0;
        }
      if((control_bits & FRONTEND_VEHICLE_CONFIRM_INPUT_MASK) != 0)
        {
          line_count     = draw_frontend_vehicle_selection(selected_vehicle_index, scroll_offset);
          elapsed_frames = -1;
          scroll_offset  = 0;
          play_confirmation_sound();
          free_primary_memory(g_FRONTEND_VEHICLE_DETAIL_SHAPES);
          g_FRONTEND_VEHICLE_DETAIL_SHAPES = NULL;
          free_primary_memory(g_FRONTEND_VEHICLE_STATS_SHAPES);
          g_FRONTEND_VEHICLE_STATS_SHAPES = NULL;
          _stop_frontend_vehicle_speech(speech_active);
          speech_active = 0;
          display_selected_car_scene(frame_count_, selected_vehicle_index);
          control_bits = 0;
        }
      if((control_bits & ControlStart) != 0)
        {
          play_confirmation_sound();
          free_primary_memory(g_FRONTEND_VEHICLE_DETAIL_SHAPES);
          g_FRONTEND_VEHICLE_DETAIL_SHAPES = NULL;
          free_primary_memory(g_FRONTEND_VEHICLE_STATS_SHAPES);
          g_FRONTEND_VEHICLE_STATS_SHAPES = NULL;
          _stop_frontend_vehicle_speech(speech_active);
          return selected_vehicle_index;
        }
      if((control_bits & ControlX) != 0)
        {
          play_error_sound();
          free_primary_memory(g_FRONTEND_VEHICLE_DETAIL_SHAPES);
          g_FRONTEND_VEHICLE_DETAIL_SHAPES = NULL;
          free_primary_memory(g_FRONTEND_VEHICLE_STATS_SHAPES);
          g_FRONTEND_VEHICLE_STATS_SHAPES = NULL;
          _stop_frontend_vehicle_speech(speech_active);
          return initial_vehicle_index;
        }
      if((control_bits & ControlUp) != 0)
        {
          scroll_offset -= 1;
          if(scroll_offset < 0)
            {
              scroll_offset = 0;
            }
          control_bits = 0;
        }
      if((control_bits & ControlDown) != 0)
        {
          scroll_offset += 1;
          if(scroll_offset > line_count)
            {
              scroll_offset = line_count;
            }
        }
      synchronize_runtime_frame();
      line_count = draw_frontend_vehicle_selection(selected_vehicle_index, scroll_offset);

      elapsed_frames += 1;
    }

  free_primary_memory(g_FRONTEND_VEHICLE_DETAIL_SHAPES);
  g_FRONTEND_VEHICLE_DETAIL_SHAPES = NULL;
  free_primary_memory(g_FRONTEND_VEHICLE_STATS_SHAPES);
  g_FRONTEND_VEHICLE_STATS_SHAPES = NULL;
  _stop_frontend_vehicle_speech(speech_active);
  return initial_vehicle_index;
}
