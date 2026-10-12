#include "frontend_frame.h"

#include "allocation.h"
#include "controller_input.h"
#include "frontend_audio_control.h"
#include "frontend_movie_stream.h"
#include "frontend_movies.h"
#include "frontend_options.h"
#include "frontend_runtime_state.h"
#include "frontend_status_text.h"
#include "frontend_text_layout.h"
#include "game_standings.h"
#include "movie_audio.h"
#include "post_race_legacy.h"
#include "race_state.h"
#include "runtime_folios.h"
#include "runtime_memory.h"
#include "runtime_string.h"
#include "sdk_veneers.h"
#include "tracked_memory.h"

#include "graphics.h"

#define FRONTEND_DISPLAY_NO_CLEAR (-1)
#define FRONTEND_DISPLAY_SHARED_OVERLAY (-2)
#define FRONTEND_AUDIO_TICKS_PER_FRAME_TICK 4
#define FRONTEND_RETRY_FRAME_TICKS 1
#define FRONTEND_PACKED_COORDINATE_SHIFT 16
#define FRONTEND_ATTRACT_HEIGHT_SHIFT 20
#define FRONTEND_DISPLAY_HEIGHT 240
#define FRONTEND_VERTICAL_CENTER_DIVISOR 2
#define FRONTEND_STAMP_SHAPE_PATH "frontend/display/stamp.3sh"
#define FRONTEND_STAMP_FRAME_SHAPE "fram"
#define FRONTEND_FONT_PATH "frontend/display/trixie.3fn"
#define FRONTEND_FONT_FOREGROUND_COLOR 0x7FFF
#define FRONTEND_DISPLAY_CONTEXT ((FrontendDisplayContext *) g_FRONTEND_DISPLAY_CONTEXT)
#define FRONTEND_MOVIE_PATH_FORMAT "Movies/%s.stream"
#define FRONTEND_MOVIE_PATH_BYTE_COUNT 60
#define FRONTEND_VIDEO_WINDOW_SHAPE_WORD 11
#define FRONTEND_HOST_MOVIE_PATH_BYTE_COUNT 40
#define FRONTEND_HOST_SHAPE_PATH_BYTE_COUNT 80
#define FRONTEND_HOST_CUE_RADIX 10
#define FRONTEND_HOST_MAP_ENTRY_WORDS 2
#define FRONTEND_HOST_MAP_WORD_COUNT 289
#define FRONTEND_HOST_FRAME_SHAPE "fram"
#define FRONTEND_HOST_BACKGROUND_SHAPE "bgnd"
#define FRONTEND_HOST_SHAPE_PATH_FORMAT "%s%d.3sh"
#define FRONTEND_TITLE_FRAME_RATE 60
#define FRONTEND_CREDITS_BACKGROUND_PATH "frontend/display/credits/bgnd.cel"
#define FRONTEND_CREDITS_STAMP_PATH "frontend/display/credits/stamp.cel"
#define FRONTEND_CREDITS_GROUP_PATH "frontend/display/credits/group.cel"
#define FRONTEND_CREDITS_PAGE_PATH_FORMAT "frontend/display/credits/%d.cel"
#define FRONTEND_CREDITS_MUSIC_PATH "credits.44k.sw.aifc"
#define FRONTEND_CREDITS_PAGE_COUNT 3
#define FRONTEND_CREDITS_THIRD_PAGE_INDEX 2
#define FRONTEND_CREDITS_INITIAL_WAIT_FRAMES 150
#define FRONTEND_CREDITS_FRAME_LIMIT 0x898
#define FRONTEND_CREDITS_X_POSITION 0x1E0000
#define FRONTEND_CREDITS_INITIAL_Y_POSITION 0x190000
#define FRONTEND_CREDITS_SCROLL_STEP 0x10000
#define FRONTEND_CREDITS_SELECTION_MASK 0x600000
#define FRONTEND_CREDITS_GROUP_MASK 0x400000
#define FRONTEND_CREDITS_BACKGROUND_MASK 0x200000
#define FRONTEND_CREDITS_PATH_BYTE_COUNT 80
#define FINISH_MENU_SHAPE_PATH "frontend/display/finish.3sh"
#define FINISH_MENU_BACKGROUND_SHAPE "menu"
#define FINISH_MENU_SELECTION_SHAPE "lite"
#define FINISH_MENU_RESULT_SLOT g_POST_RACE_RESULT_SLOT
#define FINISH_MENU_FRAME_RATE 60
#define FINISH_MENU_CHOICE_COUNT 5
#define FINISH_MENU_LAST_CHOICE 4
#define FINISH_MENU_RIGHT_MASK 0x80000000
#define FINISH_MENU_LEFT_MASK 0x40000000
#define FINISH_MENU_CONFIRM_MASK 0x0E000000
#define FINISH_MENU_CANCEL_MASK 0x01800000
#define FINISH_MENU_INPUT_MASK 0xCF000000
#define FINISH_MENU_CONTINUE_RESULT 1
#define FINISH_MENU_ALTERNATE_RESULT 2
#define FINISH_MENU_EXIT_CHOICE 0
#define FINISH_MENU_ALTERNATE_CHOICE 1
#define FINISH_MENU_CONTINUE_CHOICE 2
#define FINISH_MENU_OPTIONS_CHOICE 3
#define FINISH_MENU_STANDINGS_CHOICE 4

/* Keep these accesses in the menu function that owns the retail instructions. */
#define release_finish_menu_shapes()                                         \
  do                                                                         \
    {                                                                        \
      free_primary_memory(g_FINISH_MENU_SHAPES);                             \
      g_FINISH_MENU_SHAPES = NULL;                                           \
    }                                                                        \
  while(0)

typedef struct FrontendPositionedShape
{
  uint8 reserved_before_position[12];
  int32 packed_position;
} FrontendPositionedShape;

typedef struct FrontendDisplayContext
{
  uint8 reserved_before_active[0x14];
  int32 active;
} FrontendDisplayContext;

typedef struct FrontendDisplayWindow
{
  uint8 reserved_before_bitmap_item[0x30];
  Item  bitmap_item;
} FrontendDisplayWindow;

extern int32            *FINISH_MENU_RESULT_SLOT;
int32                    g_FRONTEND_DISPLAY_X;
int32                    g_FRONTEND_DISPLAY_Y;
int32                    g_FRONTEND_DISPLAY_DEFAULTS[4] = { 50, 50, 1, 1 };
void                    *g_FRONTEND_DISPLAY_RESOURCE;
FrontendPositionedShape *g_FRONTEND_PRIMARY_SHAPE;
const char              *g_FRONTEND_AUXILIARY_SHAPE_NAME;
FrontendPositionedShape *g_FRONTEND_SHARED_SHAPE;
void                    *g_FRONTEND_SCENE_RESOURCE;
const char              *g_FRONTEND_SCENE_SHAPE_NAME;
FrontendPositionedShape *g_FRONTEND_SCENE_SHAPE;
void                    *g_FRONTEND_STAMP_RESOURCE;
FrontendPositionedShape *g_FRONTEND_STAMP_SHAPE;
int32                    g_FRONTEND_PRESENTATION_ENABLED = 1;
uint32                   g_FRONTEND_PREVIOUS_TICK;
int32                    g_FRONTEND_GAMMA_PALETTE_ENABLED;
uint32                   g_FRONTEND_ATTRACT_LAST_TICK;
uint32                   g_FRONTEND_HOST_MAP_LINKAGE_STATE = 0;
uint32                   g_FRONTEND_HOST_FRAME_LAST_TICK;
void                    *g_FINISH_MENU_SHAPES;
uint32                   g_FRONTEND_FRAME_LAST_TICK;

int32 g_FRONTEND_HOST_SHAPE_MAP[FRONTEND_HOST_MAP_WORD_COUNT] = {
    0x0000000B, 0x00000004, 0x0000000C, 0x00000004, 0x0000000D, 0x00000004, 0x00000015, 0x00000003,
    0x00000016, 0x00000003, 0x00000017, 0x00000003, 0x0000001F, 0x00000003, 0x00000020, 0x00000001,
    0x00000021, 0x00000003, 0x00000033, 0x00000003, 0x00000034, 0x00000001, 0x00000035, 0x00000003,
    0x0000003D, 0x00000001, 0x0000003E, 0x00000004, 0x0000003F, 0x00000003, 0x00000047, 0x00000003,
    0x00000048, 0x00000003, 0x00000049, 0x00000002, 0x00000051, 0x00000002, 0x00000052, 0x00000001,
    0x0000006F, 0x00000003, 0x00000070, 0x00000004, 0x00000071, 0x00000002, 0x00000079, 0x00000001,
    0x0000007A, 0x00000002, 0x0000007B, 0x00000005, 0x00000083, 0x00000002, 0x00000084, 0x00000002,
    0x00000085, 0x00000005, 0x0000008D, 0x00000002, 0x0000008E, 0x00000003, 0x0000008F, 0x00000003,
    0x00000097, 0x00000003, 0x000000A1, 0x00000003, 0x000000A2, 0x00000001, 0x000000A3, 0x00000001,
    0x000000AB, 0x00000001, 0x000000AC, 0x00000003, 0x000000B5, 0x00000001, 0x000000B6, 0x00000003,
    0x000000C9, 0x00000001, 0x000000CA, 0x00000001, 0x000000CB, 0x00000001, 0x000000D3, 0x00000001,
    0x000000D4, 0x00000001, 0x000000DD, 0x00000002, 0x000000DE, 0x00000001, 0x000000E7, 0x00000001,
    0x000000E8, 0x00000001, 0x000000F1, 0x00000002, 0x000000F2, 0x00000001, 0x000000F3, 0x00000002,
    0x00000105, 0x00000002, 0x00000106, 0x00000001, 0x0000010F, 0x00000002, 0x00000110, 0x00000002,
    0x00000119, 0x00000002, 0x0000011A, 0x00000002, 0x0000011B, 0x00000002, 0x0000012D, 0x00000003,
    0x0000012E, 0x00000003, 0x0000012F, 0x00000002, 0x00000137, 0x00000003, 0x00000138, 0x00000003,
    0x00000139, 0x00000003, 0x00000141, 0x00000005, 0x00000142, 0x00000005, 0x0000014B, 0x00000005,
    0x0000014C, 0x00000002, 0x0000014D, 0x00000002, 0x00000155, 0x00000002, 0x00000156, 0x00000003,
    0x00000157, 0x00000003, 0x0000015F, 0x00000002, 0x00000160, 0x00000001, 0x00000161, 0x00000002,
    0x00000169, 0x00000001, 0x0000016A, 0x00000001, 0x0000016B, 0x00000001, 0x00000173, 0x00000001,
    0x00000174, 0x00000001, 0x0000017D, 0x00000003, 0x0000017E, 0x00000001, 0x00000187, 0x00000003,
    0x00000188, 0x00000003, 0x00000189, 0x00000003, 0x00000191, 0x00000001, 0x00000192, 0x00000001,
    0x00000193, 0x00000001, 0x000001A5, 0x00000005, 0x000001A6, 0x00000005, 0x000001B0, 0x00000003,
    0x000001B1, 0x00000005, 0x000001B9, 0x00000001, 0x000001BA, 0x00000005, 0x000001BB, 0x00000005,
    0x000001CD, 0x00000005, 0x000001CE, 0x00000005, 0x000001CF, 0x00000005, 0x000001D0, 0x00000003,
    0x000001D1, 0x00000003, 0x000001D2, 0x00000003, 0x00000209, 0x00000001, 0x0000020A, 0x00000001,
    0x0000020B, 0x00000003, 0x0000020C, 0x00000001, 0x0000020D, 0x00000003, 0x0000020E, 0x00000005,
    0x0000020F, 0x00000005, 0x00000231, 0x00000002, 0x00000232, 0x00000004, 0x00000233, 0x00000002,
    0x0000023B, 0x00000002, 0x0000023C, 0x00000003, 0x0000023D, 0x00000003, 0x0000024F, 0x00000003,
    0x00000250, 0x00000001, 0x00000259, 0x00000001, 0x0000025A, 0x00000001, 0x00000281, 0x00000001,
    0x00000282, 0x00000001, 0x00000283, 0x00000002, 0x00000284, 0x00000001, 0x0000028B, 0x00000002,
    0x0000028C, 0x00000002, 0x0000028D, 0x00000002, 0x0000029F, 0x00000002, 0x000002A0, 0x00000002,
    0x000002A1, 0x00000002, 0x000002AA, 0x00000003, 0x000002AC, 0x00000003, 0x000002B3, 0x00000003,
    0x000002B4, 0x00000003, 0x000002B5, 0x00000003, 0x000002B6, 0x00000001, 0x000002BD, 0x00000001,
    0x000002BE, 0x00000001, 0x000002BF, 0x00000001, 0x000003F3, 0x00000002, 0x000003F4, 0x00000002,
    0x000003F5, 0x00000002, 0x000003F6, 0x00000002, 0x000003F7, 0x00000003, 0x000003F8, 0x00000002,
    0x00000000
};

typedef char FrontendHostShapeMapSizeCheck[sizeof(g_FRONTEND_HOST_SHAPE_MAP) == 1156 ? 1 : -1];
typedef char FrontendDisplayDefaultsSizeCheck[sizeof(g_FRONTEND_DISPLAY_DEFAULTS) == 16 ? 1 : -1];

int32 g_FINISH_MENU_SELECTION_X[FINISH_MENU_CHOICE_COUNT] = { 82, 82, 82, 82, 82 };
int32 g_FINISH_MENU_SELECTION_Y[FINISH_MENU_CHOICE_COUNT] = { 57, 85, 114, 141, 169 };

typedef char FinishMenuSelectionXSizeCheck[sizeof(g_FINISH_MENU_SELECTION_X) == 20 ? 1 : -1];
typedef char FinishMenuSelectionYSizeCheck[sizeof(g_FINISH_MENU_SELECTION_Y) == 20 ? 1 : -1];

static void
_blit_frontend_positioned_shape(FrontendPositionedShape *shape_)
{
  int32 packed_position;

  packed_position = shape_->packed_position;
  blit_runtime_shape_at(shape_,
                        packed_position >> FRONTEND_PACKED_COORDINATE_SHIFT,
                        (int16) packed_position);
}

static int32
_sleep_frontend_frame_ticks(int32 frame_ticks_)
{
  return SleepAudioTicks(frame_ticks_ * FRONTEND_AUDIO_TICKS_PER_FRAME_TICK);
}

void
present_frontend_host_frame(int32 display_mode_)
{
  uint32 current_tick;
  int32  packed_position;

  if(display_mode_ == 0)
    {
      return;
    }

  current_tick = get_runtime_tick_count();
  if((int32) (current_tick - g_FRONTEND_HOST_FRAME_LAST_TICK) < 1)
    {
      _sleep_frontend_frame_ticks(FRONTEND_RETRY_FRAME_TICKS);
      return;
    }

  begin_runtime_frame();
  if(display_mode_ != FRONTEND_DISPLAY_NO_CLEAR)
    {
      clear_runtime_display(0);
      if(display_mode_ != FRONTEND_DISPLAY_SHARED_OVERLAY)
        {
          blit_runtime_shape_at((void *) display_mode_, 0, 0);
        }
    }

  if(g_FRONTEND_PRIMARY_SHAPE != NULL)
    {
      packed_position = g_FRONTEND_SHARED_SHAPE->packed_position;
      blit_runtime_shape_at(g_FRONTEND_SHARED_SHAPE,
                            packed_position >> FRONTEND_PACKED_COORDINATE_SHIFT,
                            (int16) packed_position);
    }
  if(g_FRONTEND_SCENE_RESOURCE != NULL)
    {
      if(display_mode_ == FRONTEND_DISPLAY_SHARED_OVERLAY)
        {
          packed_position = g_FRONTEND_SHARED_SHAPE->packed_position;
          blit_runtime_shape_at(g_FRONTEND_SHARED_SHAPE,
                                packed_position >> FRONTEND_PACKED_COORDINATE_SHIFT,
                                (int16) packed_position);
        }
      else
        {
          packed_position = g_FRONTEND_SCENE_SHAPE->packed_position;
          draw_runtime_shape_at(g_FRONTEND_SCENE_SHAPE,
                                packed_position >> FRONTEND_PACKED_COORDINATE_SHIFT,
                                (int16) packed_position);
        }
    }

  draw_frontend_movie_overlay_text();
  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();
  g_FRONTEND_HOST_FRAME_LAST_TICK = get_runtime_tick_count();
}

void
present_frontend_frame(int32 display_mode_)
{
  uint32 current_tick;
  int32  packed_position;

  if(display_mode_ == 0)
    {
      return;
    }

  current_tick = get_runtime_tick_count();
  if((int32) (current_tick - g_FRONTEND_FRAME_LAST_TICK) < 1)
    {
      _sleep_frontend_frame_ticks(FRONTEND_RETRY_FRAME_TICKS);
      return;
    }

  begin_runtime_frame();
  if(display_mode_ != FRONTEND_DISPLAY_NO_CLEAR)
    {
      clear_runtime_display(0);
      if(display_mode_ != FRONTEND_DISPLAY_SHARED_OVERLAY)
        {
          blit_runtime_shape_at((void *) display_mode_, 0, 0);
        }
    }

  if(g_FRONTEND_PRIMARY_SHAPE != NULL)
    {
      packed_position = g_FRONTEND_SHARED_SHAPE->packed_position;
      blit_runtime_shape_at(g_FRONTEND_SHARED_SHAPE,
                            packed_position >> FRONTEND_PACKED_COORDINATE_SHIFT,
                            (int16) packed_position);
    }
  if(g_FRONTEND_SCENE_RESOURCE != NULL)
    {
      if(display_mode_ == FRONTEND_DISPLAY_SHARED_OVERLAY)
        {
          packed_position = g_FRONTEND_SHARED_SHAPE->packed_position;
          blit_runtime_shape_at(g_FRONTEND_SHARED_SHAPE,
                                packed_position >> FRONTEND_PACKED_COORDINATE_SHIFT,
                                (int16) packed_position);
        }
      else
        {
          packed_position = g_FRONTEND_SCENE_SHAPE->packed_position;
          draw_runtime_shape_at(g_FRONTEND_SCENE_SHAPE,
                                packed_position >> FRONTEND_PACKED_COORDINATE_SHIFT,
                                (int16) packed_position);
        }
    }

  draw_frontend_status_text();
  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();
  g_FRONTEND_FRAME_LAST_TICK = get_runtime_tick_count();
}

void
present_frontend_attract_frame(int32 display_mode_)
{
  uint32 current_tick;
  uint32 packed_extent;
  int32  height;
  int32  packed_position;

  if(display_mode_ == 0)
    {
      return;
    }

  current_tick = get_runtime_tick_count();
  if((int32) (current_tick - g_FRONTEND_ATTRACT_LAST_TICK) < 1)
    {
      _sleep_frontend_frame_ticks(FRONTEND_RETRY_FRAME_TICKS);
      return;
    }

  begin_runtime_frame();
  if(display_mode_ != FRONTEND_DISPLAY_NO_CLEAR)
    {
      clear_runtime_display(0);
      if(display_mode_ != FRONTEND_DISPLAY_SHARED_OVERLAY)
        {
          packed_extent = ((uint32 *) display_mode_)[1];
          height        = (int32) (packed_extent << FRONTEND_ATTRACT_HEIGHT_SHIFT) >>
                          FRONTEND_ATTRACT_HEIGHT_SHIFT;
          blit_runtime_shape_at((void *) display_mode_,
                                0,
                                (FRONTEND_DISPLAY_HEIGHT - height) /
                                  FRONTEND_VERTICAL_CENTER_DIVISOR);
        }
    }

  if(g_FRONTEND_PRIMARY_SHAPE != NULL)
    {
      packed_position = g_FRONTEND_SHARED_SHAPE->packed_position;
      blit_runtime_shape_at(g_FRONTEND_SHARED_SHAPE,
                            packed_position >> FRONTEND_PACKED_COORDINATE_SHIFT,
                            (int16) packed_position);
    }
  if(g_FRONTEND_SCENE_RESOURCE != NULL)
    {
      if(display_mode_ == FRONTEND_DISPLAY_SHARED_OVERLAY)
        {
          packed_position = g_FRONTEND_SHARED_SHAPE->packed_position;
          blit_runtime_shape_at(g_FRONTEND_SHARED_SHAPE,
                                packed_position >> FRONTEND_PACKED_COORDINATE_SHIFT,
                                (int16) packed_position);
        }
      else
        {
          packed_position = g_FRONTEND_SCENE_SHAPE->packed_position;
          draw_runtime_shape_at(g_FRONTEND_SCENE_SHAPE,
                                packed_position >> FRONTEND_PACKED_COORDINATE_SHIFT,
                                (int16) packed_position);
        }
    }

  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();
  g_FRONTEND_ATTRACT_LAST_TICK = get_runtime_tick_count();
}

void
present_frontend_overlay_frame(void *foreground_shape_)
{
  begin_runtime_frame();
  clear_runtime_display(0);

  if(g_FRONTEND_PRIMARY_SHAPE != NULL)
    {
      _blit_frontend_positioned_shape(g_FRONTEND_SHARED_SHAPE);
    }
  if(g_FRONTEND_SCENE_RESOURCE != NULL)
    {
      _blit_frontend_positioned_shape(g_FRONTEND_SCENE_SHAPE);
    }
  if(foreground_shape_ != NULL)
    {
      draw_runtime_shape_at(foreground_shape_, 0, 0);
    }

  finish_runtime_frame();
  present_runtime_frame();
}

void
draw_frontend_shape_at_display_position(void *shape_)
{
  if(shape_ == NULL)
    {
      return;
    }

  prepare_runtime_display();
  blit_runtime_shape_at(shape_, g_FRONTEND_DISPLAY_X, g_FRONTEND_DISPLAY_Y);
  finish_runtime_frame();
}

void
release_frontend_display_resource(void)
{
  free_primary_memory(g_FRONTEND_DISPLAY_RESOURCE);
  g_FRONTEND_DISPLAY_RESOURCE = NULL;
}

void
release_frontend_display(void)
{
  free_primary_memory(g_FRONTEND_STAMP_RESOURCE);
  g_FRONTEND_STAMP_RESOURCE = NULL;
  g_FRONTEND_STAMP_SHAPE    = NULL;
}

void
initialize_frontend_stamp(void)
{
  g_FRONTEND_STAMP_RESOURCE = load_runtime_file_data(FRONTEND_STAMP_SHAPE_PATH, 0);
  track_primary_allocation(g_FRONTEND_STAMP_RESOURCE);
  g_FRONTEND_STAMP_SHAPE =
    locate_runtime_shape(g_FRONTEND_STAMP_RESOURCE, (const uint32 *) FRONTEND_STAMP_FRAME_SHAPE);
}

void
draw_frontend_stamp(void)
{
  if(g_FRONTEND_STAMP_RESOURCE == NULL)
    {
      initialize_frontend_stamp();
    }

  draw_runtime_shape_at(g_FRONTEND_STAMP_SHAPE, 0, 0);
}

uint32
get_frontend_elapsed_ticks(void)
{
  uint32 current_tick;
  uint32 elapsed_ticks;

  current_tick             = get_runtime_tick_count();
  elapsed_ticks            = current_tick - g_FRONTEND_PREVIOUS_TICK;
  g_FRONTEND_PREVIOUS_TICK = current_tick;
  return elapsed_ticks;
}

void
initialize_frontend_display(void)
{
  if(g_FRONTEND_DISPLAY_RESOURCE == NULL)
    {
      g_FRONTEND_DISPLAY_RESOURCE = load_runtime_file_data(FRONTEND_FONT_PATH, 0);
      track_primary_allocation(g_FRONTEND_DISPLAY_RESOURCE);
    }

  initialize_runtime_font(g_FRONTEND_DISPLAY_RESOURCE);
  FRONTEND_DISPLAY_CONTEXT->active = 1;
  set_runtime_font_colors(FRONTEND_FONT_FOREGROUND_COLOR, 0);
}

int32
present_frontend_scene(int32              shape_count_,
                       const char *const *shape_names_,
                       int32              flags_,
                       const char        *fallback_path_,
                       const char        *fallback_shape_,
                       int32              suspend_music_,
                       int32              movie_mode_,
                       ...)
{
  char  movie_path[FRONTEND_MOVIE_PATH_BYTE_COUNT];
  void *video_window;
  int32 shape_index;
  int32 result;

  (void) flags_;

  for(shape_index = 0; shape_index < shape_count_; ++shape_index)
    {
      runtime_format_string(movie_path, FRONTEND_MOVIE_PATH_FORMAT, shape_names_[shape_index]);
      if(runtime_file_exists(movie_path) == 0)
        {
          return 0;
        }
    }

  if(g_FRONTEND_PRESENTATION_ENABLED == 0)
    {
      return 0;
    }
  if(suspend_music_ != 0)
    {
      suspend_frontend_music_for_scene();
    }

  g_FRONTEND_PRIMARY_SHAPE        = NULL;
  g_FRONTEND_AUXILIARY_SHAPE_NAME = NULL;
  g_FRONTEND_SHARED_SHAPE         = NULL;
  g_FRONTEND_SCENE_RESOURCE       = NULL;
  g_FRONTEND_SCENE_SHAPE_NAME     = NULL;
  g_FRONTEND_SCENE_SHAPE          = NULL;

  g_FRONTEND_SCENE_RESOURCE = load_runtime_file_data(FRONTEND_STAMP_SHAPE_PATH, 0);
  track_primary_allocation(g_FRONTEND_SCENE_RESOURCE);
  g_FRONTEND_SCENE_SHAPE_NAME = FRONTEND_STAMP_FRAME_SHAPE;
  g_FRONTEND_SCENE_SHAPE =
    locate_runtime_shape(g_FRONTEND_SCENE_RESOURCE, (const uint32 *) g_FRONTEND_SCENE_SHAPE_NAME);

  synchronize_runtime_frame();
  synchronize_runtime_frame();
  present_frontend_attract_frame(FRONTEND_DISPLAY_NO_CLEAR);
  synchronize_runtime_frame();
  synchronize_runtime_frame();
  present_frontend_attract_frame(FRONTEND_DISPLAY_NO_CLEAR);
  set_allocation_mode(ALLOCATION_MODE_SECONDARY);

  if(RACE_CONFIGURATION->stereo_enabled != 0)
    {
      disable_stereo_movie_audio();
    }
  else
    {
      enable_stereo_movie_audio();
    }
  set_frontend_movie_mode(movie_mode_);

  begin_runtime_frame();
  finish_runtime_frame();
  synchronize_runtime_frame();
  video_window = get_runtime_pending_video_window();
  draw_runtime_shape_at(((void **) video_window)[FRONTEND_VIDEO_WINDOW_SHAPE_WORD], 0, 0);
  finish_runtime_frame();
  present_runtime_frame();

  result = 0;
  for(shape_index = 0; shape_index < shape_count_; ++shape_index)
    {
      if(shape_names_[shape_index][0] == 'w')
        {
          configure_wide_movie_stream_buffers();
        }
      else if(shape_names_[shape_index][0] == 'c')
        {
          configure_compact_movie_stream_buffers();
        }
      else
        {
          configure_default_movie_stream_buffers();
        }
      result =
        play_frontend_movie_stream(shape_names_[shape_index], present_frontend_attract_frame);
      if(result == 0)
        {
          break;
        }
    }

  begin_runtime_frame();
  video_window = get_runtime_pending_video_window();
  draw_runtime_shape_at(((void **) video_window)[FRONTEND_VIDEO_WINDOW_SHAPE_WORD], 0, 0);
  finish_runtime_frame();
  present_runtime_frame();
  set_allocation_mode(ALLOCATION_MODE_PRIMARY);

  free_primary_memory(g_FRONTEND_SCENE_RESOURCE);
  g_FRONTEND_SCENE_RESOURCE = NULL;
  if(suspend_music_ != 0)
    {
      resume_frontend_music_after_scene();
    }
  if(fallback_path_ != NULL && result == 0)
    {
      display_frontend_splash(fallback_path_, fallback_shape_);
    }
  return result;
}

int32
present_frontend_host(const char *cue_code_,
                      void        (*frame_callback_)(int32),
                      const char *shape_path_,
                      const char *background_shape_,
                      int32       suspend_music_,
                      int32       skip_host_shapes_)
{
  char  movie_path[FRONTEND_HOST_MOVIE_PATH_BYTE_COUNT];
  char  host_shape_path[FRONTEND_HOST_SHAPE_PATH_BYTE_COUNT];
  void *video_window;
  int32 cue_length;
  int32 cue_key;
  int32 host_shape_number;
  int32 map_index;
  int32 result;

  (void) background_shape_;

  runtime_format_string(movie_path, FRONTEND_MOVIE_PATH_FORMAT, cue_code_);
  if(runtime_file_exists(movie_path) == 0)
    {
      return 0;
    }
  if(g_FRONTEND_PRESENTATION_ENABLED == 0)
    {
      return 1;
    }
  if(suspend_music_ != 0)
    {
      suspend_frontend_music_for_scene();
    }

  configure_compact_movie_stream_buffers();
  g_FRONTEND_PRIMARY_SHAPE        = NULL;
  g_FRONTEND_AUXILIARY_SHAPE_NAME = NULL;
  g_FRONTEND_SHARED_SHAPE         = NULL;
  g_FRONTEND_SCENE_RESOURCE       = NULL;
  g_FRONTEND_SCENE_SHAPE_NAME     = NULL;
  g_FRONTEND_SCENE_SHAPE          = NULL;

  if(skip_host_shapes_ == 0 && shape_path_ != NULL)
    {
      cue_length        = runtime_string_length(cue_code_);
      cue_key           = runtime_string_to_integer(cue_code_) * FRONTEND_HOST_CUE_RADIX +
                          runtime_string_to_integer(cue_code_ + cue_length - 1);
      host_shape_number = 0;
      map_index         = 0;
      while(g_FRONTEND_HOST_SHAPE_MAP[map_index] != 0)
        {
          if(g_FRONTEND_HOST_SHAPE_MAP[map_index] == cue_key)
            {
              host_shape_number = g_FRONTEND_HOST_SHAPE_MAP[map_index + 1];
              break;
            }
          map_index += FRONTEND_HOST_MAP_ENTRY_WORDS;
        }

      if(host_shape_number != 0)
        {
          runtime_format_string(host_shape_path,
                                FRONTEND_HOST_SHAPE_PATH_FORMAT,
                                shape_path_,
                                host_shape_number);
          g_FRONTEND_SCENE_RESOURCE = load_runtime_file_data(host_shape_path, 0);
          track_primary_allocation(g_FRONTEND_SCENE_RESOURCE);
          g_FRONTEND_SCENE_SHAPE_NAME = FRONTEND_HOST_FRAME_SHAPE;
          g_FRONTEND_SCENE_SHAPE =
            locate_runtime_shape(g_FRONTEND_SCENE_RESOURCE,
                                 (const uint32 *) g_FRONTEND_SCENE_SHAPE_NAME);
          g_FRONTEND_AUXILIARY_SHAPE_NAME = FRONTEND_HOST_BACKGROUND_SHAPE;
          g_FRONTEND_SHARED_SHAPE =
            locate_runtime_shape(g_FRONTEND_SCENE_RESOURCE,
                                 (const uint32 *) g_FRONTEND_AUXILIARY_SHAPE_NAME);
        }
    }

  synchronize_runtime_frame();
  synchronize_runtime_frame();
  frame_callback_(FRONTEND_DISPLAY_SHARED_OVERLAY);
  set_runtime_grayscale_palette();
  synchronize_runtime_frame();
  synchronize_runtime_frame();
  frame_callback_(FRONTEND_DISPLAY_SHARED_OVERLAY);

  set_allocation_mode(ALLOCATION_MODE_SECONDARY);
  if(RACE_CONFIGURATION->stereo_enabled != 0)
    {
      disable_stereo_movie_audio();
    }
  else
    {
      enable_stereo_movie_audio();
    }
  set_frontend_movie_mode(0);
  result = play_frontend_movie_stream(cue_code_, frame_callback_);
  set_allocation_mode(ALLOCATION_MODE_PRIMARY);

  begin_runtime_frame();
  video_window = get_runtime_pending_video_window();
  draw_runtime_shape_at(((void **) video_window)[FRONTEND_VIDEO_WINDOW_SHAPE_WORD], 0, 0);
  finish_runtime_frame();
  present_runtime_frame();

  if(g_FRONTEND_SCENE_RESOURCE != NULL)
    {
      synchronize_runtime_frame();
      synchronize_runtime_frame();
      frame_callback_(FRONTEND_DISPLAY_NO_CLEAR);
      synchronize_runtime_frame();
      synchronize_runtime_frame();
      frame_callback_(FRONTEND_DISPLAY_NO_CLEAR);
      free_primary_memory(g_FRONTEND_SCENE_RESOURCE);
      g_FRONTEND_SCENE_RESOURCE = NULL;
    }
  if(suspend_music_ != 0)
    {
      resume_frontend_music_after_scene();
    }
  return result;
}

void *
display_frontend_shape(const char *shape_path_, const char *shape_name_)
{
  FrontendPositionedShape *shape;
  void                    *shape_collection;
  int32                    packed_position;

  shape_collection = load_runtime_file_data(shape_path_, 0);
  track_primary_allocation(shape_collection);
  shape = locate_runtime_shape(shape_collection, (const uint32 *) shape_name_);
  begin_runtime_frame();
  clear_runtime_display(0);
  packed_position = shape->packed_position;
  blit_runtime_shape_at(shape,
                        packed_position >> FRONTEND_PACKED_COORDINATE_SHIFT,
                        (int16) packed_position);
  return shape_collection;
}

int32
display_frontend_splash(const char *shape_path_, const char *shape_name_)
{
  FrontendPositionedShape *shape;
  void                    *shape_collection;
  int32                    packed_position;

  shape_collection = load_runtime_file_data(shape_path_, 0);
  track_primary_allocation(shape_collection);
  shape = locate_runtime_shape(shape_collection, (const uint32 *) shape_name_);

  begin_runtime_frame();
  clear_runtime_display(0);
  packed_position = shape->packed_position;
  blit_runtime_shape_at(shape,
                        packed_position >> FRONTEND_PACKED_COORDINATE_SHIFT,
                        (int16) packed_position);
  draw_frontend_stamp();
  finish_runtime_frame();
  if(g_FRONTEND_GAMMA_PALETTE_ENABLED != 0)
    {
      set_runtime_gamma_grayscale_palette();
    }
  present_runtime_frame();

  begin_runtime_frame();
  synchronize_runtime_frame();
  clear_runtime_display(0);
  packed_position = shape->packed_position;
  blit_runtime_shape_at(shape,
                        packed_position >> FRONTEND_PACKED_COORDINATE_SHIFT,
                        (int16) packed_position);
  draw_frontend_stamp();
  finish_runtime_frame();
  free_primary_memory(shape_collection);
  return 0;
}

int32
display_frontend_title(const char *shape_path_, const char *shape_name_, int32 duration_)
{
  FrontendPositionedShape *shape;
  void                    *shape_collection;
  int32                    packed_position;

  shape_collection = load_runtime_file_data(shape_path_, 0);
  track_primary_allocation(shape_collection);
  shape = locate_runtime_shape(shape_collection, (const uint32 *) shape_name_);

  begin_runtime_frame();
  clear_runtime_display(0);
  packed_position = shape->packed_position;
  blit_runtime_shape_at(shape,
                        packed_position >> FRONTEND_PACKED_COORDINATE_SHIFT,
                        (int16) packed_position);
  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();

  begin_runtime_frame();
  synchronize_runtime_frame();
  clear_runtime_display(0);
  packed_position = shape->packed_position;
  blit_runtime_shape_at(shape,
                        packed_position >> FRONTEND_PACKED_COORDINATE_SHIFT,
                        (int16) packed_position);
  draw_frontend_stamp();
  finish_runtime_frame();
  free_primary_memory(shape_collection);

  if(duration_ <= 0)
    {
      return 0;
    }
  return poll_controller_input(duration_ * FRONTEND_TITLE_FRAME_RATE, 1, 0);
}

int32
run_frontend_credits(int32 restart_music_)
{
  char                   page_path[FRONTEND_CREDITS_PATH_BYTE_COUNT];
  CCB                   *pages[FRONTEND_CREDITS_PAGE_COUNT];
  FrontendDisplayWindow *display_window;
  CCB                   *background;
  CCB                   *stamp;
  CCB                   *group;
  CCB                   *display_chain;
  uint32                 control_bits;
  int32                  y_position;
  int32                  page_index;
  int32                  frame_index;
  int32                  interrupted;

  interrupted = 0;
  background  = load_runtime_cel(FRONTEND_CREDITS_BACKGROUND_PATH, NULL);
  stamp       = load_runtime_cel(FRONTEND_CREDITS_STAMP_PATH, NULL);
  group       = load_runtime_cel(FRONTEND_CREDITS_GROUP_PATH, NULL);
  for(page_index = 0; page_index < FRONTEND_CREDITS_PAGE_COUNT; ++page_index)
    {
      runtime_format_string(page_path, FRONTEND_CREDITS_PAGE_PATH_FORMAT, page_index + 1);
      pages[page_index] = load_runtime_cel(page_path, NULL);
    }

  if(RACE_CONFIGURATION->music_enabled != 0)
    {
      suspend_frontend_music_for_scene();
      start_frontend_music(FRONTEND_CREDITS_MUSIC_PATH);
      if(RACE_CONFIGURATION->music_enabled != 0)
        {
          resume_frontend_audio_stream();
        }
    }

  y_position           = FRONTEND_CREDITS_INITIAL_Y_POSITION;
  background->ccb_XPos = 0;
  background->ccb_YPos = 0;
  background->ccb_Flags &= ~CCB_LAST;
  background->ccb_NextPtr = pages[0];
  group->ccb_XPos         = 0;
  group->ccb_YPos         = 0;
  group->ccb_Flags &= ~CCB_LAST;
  group->ccb_NextPtr = pages[0];

  pages[0]->ccb_XPos = FRONTEND_CREDITS_X_POSITION;
  pages[0]->ccb_YPos = y_position;
  pages[0]->ccb_Flags &= ~CCB_LAST;
  pages[0]->ccb_NextPtr = pages[1];
  pages[1]->ccb_XPos    = FRONTEND_CREDITS_X_POSITION;
  pages[1]->ccb_YPos    = y_position + pages[0]->ccb_Height * FRONTEND_CREDITS_SCROLL_STEP;
  pages[1]->ccb_Flags &= ~CCB_LAST;
  pages[1]->ccb_NextPtr                              = pages[FRONTEND_CREDITS_THIRD_PAGE_INDEX];
  pages[FRONTEND_CREDITS_THIRD_PAGE_INDEX]->ccb_XPos = FRONTEND_CREDITS_X_POSITION;
  pages[FRONTEND_CREDITS_THIRD_PAGE_INDEX]->ccb_YPos =
    y_position + (pages[1]->ccb_Height + pages[0]->ccb_Height) * FRONTEND_CREDITS_SCROLL_STEP;
  pages[FRONTEND_CREDITS_THIRD_PAGE_INDEX]->ccb_Flags &= ~CCB_LAST;
  pages[FRONTEND_CREDITS_THIRD_PAGE_INDEX]->ccb_NextPtr = stamp;
  stamp->ccb_XPos                                       = 0;
  stamp->ccb_YPos                                       = 0;
  stamp->ccb_NextPtr                                    = NULL;
  stamp->ccb_Flags |= CCB_LAST;

  begin_runtime_frame();
  clear_runtime_display(0);
  present_runtime_frame();
  begin_runtime_frame();
  clear_runtime_display(0);
  display_window = get_runtime_displayed_video_window();
  DRAW_CELS_VENEER(display_window->bitmap_item, background);
  finish_runtime_frame();
  present_runtime_frame();

  for(frame_index = 0; frame_index < FRONTEND_CREDITS_INITIAL_WAIT_FRAMES; ++frame_index)
    {
      synchronize_runtime_frame();
      if(read_stable_controller_input() != 0)
        {
          interrupted = 1;
          break;
        }
    }

  display_chain = background;
  for(frame_index = 0; frame_index < FRONTEND_CREDITS_FRAME_LIMIT && interrupted == 0;
      ++frame_index)
    {
      control_bits = read_stable_controller_input();
      if(control_bits != 0)
        {
          if((control_bits & FRONTEND_CREDITS_SELECTION_MASK) == 0)
            {
              interrupted = 1;
              break;
            }
          if((control_bits & FRONTEND_CREDITS_GROUP_MASK) != 0)
            {
              display_chain = group;
            }
          else if((control_bits & FRONTEND_CREDITS_BACKGROUND_MASK) != 0)
            {
              display_chain = background;
            }
        }

      y_position -= FRONTEND_CREDITS_SCROLL_STEP;
      pages[0]->ccb_YPos = y_position;
      pages[1]->ccb_YPos = y_position + pages[0]->ccb_Height * FRONTEND_CREDITS_SCROLL_STEP;
      pages[FRONTEND_CREDITS_THIRD_PAGE_INDEX]->ccb_YPos =
        y_position + (pages[1]->ccb_Height + pages[0]->ccb_Height) *
                       FRONTEND_CREDITS_SCROLL_STEP;
      display_window = get_runtime_displayed_video_window();
      synchronize_runtime_frame();
      synchronize_runtime_frame();
      DRAW_CELS_VENEER(display_window->bitmap_item, display_chain);
      finish_runtime_frame();
      present_runtime_frame();
    }

  for(page_index = 0; page_index < FRONTEND_CREDITS_PAGE_COUNT; ++page_index)
    {
      unload_runtime_cel(pages[page_index]);
    }
  unload_runtime_cel(background);
  unload_runtime_cel(stamp);
  unload_runtime_cel(group);
  if(restart_music_ != 0)
    {
      suspend_frontend_music_for_scene();
      resume_frontend_music_after_scene();
    }
  return interrupted;
}

int32
draw_finish_menu_frame(int32 selection_index_)
{
  FrontendPositionedShape *shape;

  begin_runtime_frame();
  synchronize_runtime_frame();
  clear_runtime_display(0);

  if(g_FINISH_MENU_SHAPES == NULL)
    {
      g_FINISH_MENU_SHAPES = load_runtime_file_data(FINISH_MENU_SHAPE_PATH, 0);
      track_primary_allocation(g_FINISH_MENU_SHAPES);
    }

  shape = locate_runtime_shape(g_FINISH_MENU_SHAPES,
                               (const uint32 *) FINISH_MENU_BACKGROUND_SHAPE);
  _blit_frontend_positioned_shape(shape);

  shape = locate_runtime_shape(g_FINISH_MENU_SHAPES,
                               (const uint32 *) FINISH_MENU_SELECTION_SHAPE);
  draw_runtime_shape_at(shape,
                        g_FINISH_MENU_SELECTION_X[selection_index_],
                        g_FINISH_MENU_SELECTION_Y[selection_index_]);
  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();
  return 1;
}

int32
run_finish_menu(int32 timeout_seconds_)
{
  uint32 control_bits;
  int32  selection_index;
  int32  timeout_frames;

  g_FINISH_MENU_SHAPES = NULL;
  *FINISH_MENU_RESULT_SLOT = 0;
  begin_runtime_frame();
  synchronize_runtime_frame();
  clear_runtime_display(0);
  finish_runtime_frame();
  present_runtime_frame();

  selection_index = 0;
  draw_finish_menu_frame(selection_index);
  timeout_frames = timeout_seconds_ * FINISH_MENU_FRAME_RATE;

  for(;;)
    {
      control_bits = poll_controller_input(timeout_frames, 1, FINISH_MENU_INPUT_MASK);
      if(control_bits == 0)
        {
          release_finish_menu_shapes();
          return 1;
        }

      if((control_bits & FINISH_MENU_RIGHT_MASK) != 0)
        {
          selection_index++;
          if(selection_index >= FINISH_MENU_CHOICE_COUNT)
            {
              selection_index = 0;
            }
        }
      if((control_bits & FINISH_MENU_LEFT_MASK) != 0)
        {
          if(selection_index != 0)
            {
              selection_index--;
            }
          else
            {
              selection_index = FINISH_MENU_LAST_CHOICE;
            }
        }

      if((control_bits & FINISH_MENU_CONFIRM_MASK) != 0)
        {
          switch(selection_index)
            {
              case FINISH_MENU_EXIT_CHOICE:
                release_finish_menu_shapes();
                return 1;

              case FINISH_MENU_ALTERNATE_CHOICE:
                *FINISH_MENU_RESULT_SLOT = FINISH_MENU_ALTERNATE_RESULT;
                release_finish_menu_shapes();
                return 1;

              case FINISH_MENU_CONTINUE_CHOICE:
                *FINISH_MENU_RESULT_SLOT = FINISH_MENU_CONTINUE_RESULT;
                release_finish_menu_shapes();
                return 1;

              case FINISH_MENU_OPTIONS_CHOICE:
                run_frontend_options();
                control_bits = 0;
                break;

              case FINISH_MENU_STANDINGS_CHOICE:
                release_finish_menu_shapes();
                run_game_standings();
                control_bits = 0;
                break;
            }
        }

      if((control_bits & FINISH_MENU_CANCEL_MASK) != 0)
        {
          release_finish_menu_shapes();
          return 1;
        }
      draw_finish_menu_frame(selection_index);
    }
}
