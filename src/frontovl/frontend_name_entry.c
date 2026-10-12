#include "frontend_name_entry.h"

#include "controller_input.h"
#include "frontend_frame.h"
#include "frontend_sound.h"
#include "post_race_legacy.h"
#include "runtime_memory.h"
#include "runtime_string.h"
#include "sdk_veneers.h"
#include "tracked_memory.h"

#define FRONTEND_NAME_ENTRY_SHAPE_PATH "frontend/display/userinp.3sh"
#define FRONTEND_NAME_ENTRY_BACKGROUND_SHAPE "bgnd"
#define FRONTEND_NAME_ENTRY_TIME_SHAPE "time"
#define FRONTEND_NAME_ENTRY_SPEED_SHAPE "spd "
#define FRONTEND_NAME_ENTRY_SCORE_SHAPE "scor"
#define FRONTEND_NAME_ENTRY_INBOX_SHAPE "nbox"
#define FRONTEND_NAME_ENTRY_LOWERCASE_SHAPE "lcas"
#define FRONTEND_NAME_ENTRY_UPPERCASE_SHAPE "ucas"
#define FRONTEND_NAME_ENTRY_CURSOR_SHAPE "crsr"
#define FRONTEND_NAME_ENTRY_SHAPE_POSITION_WORD 3
#define FRONTEND_NAME_ENTRY_SHAPE_X_SHIFT 16
#define FRONTEND_NAME_ENTRY_FOREGROUND_COLOR 0x7FFF
#define FRONTEND_NAME_ENTRY_TEXT_X 69
#define FRONTEND_NAME_ENTRY_TEXT_Y 205
#define FRONTEND_NAME_ENTRY_CURSOR_FIRST_X 81
#define FRONTEND_NAME_ENTRY_CURSOR_FIRST_Y 36
#define FRONTEND_NAME_ENTRY_CURSOR_SPACING 23
#define FRONTEND_NAME_ENTRY_BUFFER_BYTE_COUNT 84
#define FRONTEND_NAME_ENTRY_COLUMN_COUNT 7
#define FRONTEND_NAME_ENTRY_ROW_COUNT 6
#define FRONTEND_NAME_ENTRY_SPECIAL_ROW 5
#define FRONTEND_NAME_ENTRY_DELETE_COLUMN 5
#define FRONTEND_NAME_ENTRY_ACCEPT_COLUMN 6
#define FRONTEND_NAME_ENTRY_FRAMES_PER_UNIT 60
#define FRONTEND_NAME_ENTRY_REPEAT_SAMPLE_LIMIT 4
#define FRONTEND_NAME_ENTRY_ANONYMOUS_BYTE_COUNT 10
#define FRONTEND_NAME_ENTRY_CURSOR_CHARACTER '_'
#define FRONTEND_NAME_ENTRY_DIRECTION_MASK 0xF0000000
#define FRONTEND_NAME_ENTRY_ACTION_RELEASE_MASK 0x0D800000
#define FRONTEND_NAME_ENTRY_CONFIRM_MASK 0x01000000
#define FRONTEND_NAME_ENTRY_BACKSPACE_MASK 0x04000000
#define FRONTEND_NAME_ENTRY_ADD_CHARACTER_MASK 0x08000000
#define FRONTEND_NAME_ENTRY_CANCEL_MASK 0x00800000
#define FRONTEND_NAME_ENTRY_UP_MASK 0x80000000
#define FRONTEND_NAME_ENTRY_RIGHT_MASK 0x20000000
#define FRONTEND_NAME_ENTRY_DOWN_MASK 0x40000000
#define FRONTEND_NAME_ENTRY_LEFT_MASK 0x10000000
#define FRONTEND_NAME_ENTRY_UPPERCASE_MASK 0x00600000

typedef struct FrontendNameEntryCursorPosition
{
  int32 column;
  int32 row;
} FrontendNameEntryCursorPosition;

static const FrontendNameEntryCursorPosition G_FRONTEND_NAME_ENTRY_CURSOR_POSITION_INITIALIZER = {
  0,
  0
};

typedef char
  FrontendNameEntryCursorPositionSizeCheck[sizeof(FrontendNameEntryCursorPosition) == 8 ? 1 : -1];

char  g_FRONTEND_LOWERCASE_CHARACTERS[44] = "abcdefghijklmnopqrstuvwxyz0123456789'-.   ";
char  g_FRONTEND_UPPERCASE_CHARACTERS[44] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\"/()&*";
void *g_FRONTEND_NAME_ENTRY_SHAPES;

typedef char
  FrontendLowercaseCharactersSizeCheck[sizeof(g_FRONTEND_LOWERCASE_CHARACTERS) == 44 ? 1 : -1];
typedef char
  FrontendUppercaseCharactersSizeCheck[sizeof(g_FRONTEND_UPPERCASE_CHARACTERS) == 44 ? 1 : -1];

static void
_blit_name_entry_shape(const char *shape_name_)
{
  void *shape;
  int32 position;

  shape    = locate_runtime_shape(g_FRONTEND_NAME_ENTRY_SHAPES, (const uint32 *) shape_name_);
  position = ((int32 *) shape)[FRONTEND_NAME_ENTRY_SHAPE_POSITION_WORD];
  blit_runtime_shape_at(shape, position >> FRONTEND_NAME_ENTRY_SHAPE_X_SHIFT, (int16) position);
}

static void
_draw_name_entry_shape(const char *shape_name_)
{
  void *shape;
  int32 position;

  shape    = locate_runtime_shape(g_FRONTEND_NAME_ENTRY_SHAPES, (const uint32 *) shape_name_);
  position = ((int32 *) shape)[FRONTEND_NAME_ENTRY_SHAPE_POSITION_WORD];
  draw_runtime_shape_at(shape, position >> FRONTEND_NAME_ENTRY_SHAPE_X_SHIFT, (int16) position);
}

int32
draw_frontend_name_entry(int32       uppercase_,
                         const char *text_,
                         int32       cursor_column_,
                         int32       cursor_row_,
                         int32       show_time_,
                         int32       show_speed_,
                         int32       show_score_)
{
  void *cursor_shape;

  initialize_frontend_display();
  set_runtime_font_colors(FRONTEND_NAME_ENTRY_FOREGROUND_COLOR, 0);
  begin_runtime_frame();
  clear_runtime_display(0);

  if(g_FRONTEND_NAME_ENTRY_SHAPES == NULL)
    {
      g_FRONTEND_NAME_ENTRY_SHAPES = load_runtime_file_data(FRONTEND_NAME_ENTRY_SHAPE_PATH, 0);
      track_primary_allocation(g_FRONTEND_NAME_ENTRY_SHAPES);
    }

  _blit_name_entry_shape(FRONTEND_NAME_ENTRY_BACKGROUND_SHAPE);
  if(show_time_ != 0)
    {
      _blit_name_entry_shape(FRONTEND_NAME_ENTRY_TIME_SHAPE);
    }
  if(show_speed_ != 0)
    {
      _blit_name_entry_shape(FRONTEND_NAME_ENTRY_SPEED_SHAPE);
    }
  if(show_score_ != 0)
    {
      _blit_name_entry_shape(FRONTEND_NAME_ENTRY_SCORE_SHAPE);
    }

  _draw_name_entry_shape(FRONTEND_NAME_ENTRY_INBOX_SHAPE);
  if(uppercase_ == 0)
    {
      _draw_name_entry_shape(FRONTEND_NAME_ENTRY_LOWERCASE_SHAPE);
    }
  else
    {
      _draw_name_entry_shape(FRONTEND_NAME_ENTRY_UPPERCASE_SHAPE);
    }

  cursor_shape = locate_runtime_shape(g_FRONTEND_NAME_ENTRY_SHAPES,
                                      (const uint32 *) FRONTEND_NAME_ENTRY_CURSOR_SHAPE);
  draw_runtime_shape_at(
    cursor_shape,
    cursor_column_ * FRONTEND_NAME_ENTRY_CURSOR_SPACING + FRONTEND_NAME_ENTRY_CURSOR_FIRST_X,
    cursor_row_ * FRONTEND_NAME_ENTRY_CURSOR_SPACING + FRONTEND_NAME_ENTRY_CURSOR_FIRST_Y);
  draw_runtime_text(text_, FRONTEND_NAME_ENTRY_TEXT_X, FRONTEND_NAME_ENTRY_TEXT_Y);
  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();
  return 1;
}

int32
enter_frontend_name(char *destination_,
                    int32 maximum_length_,
                    int32 timeout_units_,
                    int32 show_time_,
                    int32 show_speed_,
                    int32 show_score_)
{
  char                            entered_name[FRONTEND_NAME_ENTRY_BUFFER_BYTE_COUNT];
  FrontendNameEntryCursorPosition cursor_position;
  uint32                          control_bits;
  int32                           frame_count;
  int32                           maximum_frames;
  int32                           name_length;
  int32                           repeat_sample;
  int32                           uppercase;
  int32                           uppercase_displayed;

  cursor_position     = G_FRONTEND_NAME_ENTRY_CURSOR_POSITION_INITIALIZER;
  name_length         = 0;
  uppercase_displayed = 0;
  entered_name[0]     = FRONTEND_NAME_ENTRY_CURSOR_CHARACTER;
  entered_name[1]     = 0;
  if(draw_frontend_name_entry(0,
                              entered_name,
                              cursor_position.column,
                              cursor_position.row,
                              show_time_,
                              show_speed_,
                              show_score_) == 0)
    {
      free_primary_memory(g_FRONTEND_NAME_ENTRY_SHAPES);
      g_FRONTEND_NAME_ENTRY_SHAPES = NULL;
      return 0;
    }

  control_bits   = 0;
  frame_count    = 0;
  maximum_frames = timeout_units_ * FRONTEND_NAME_ENTRY_FRAMES_PER_UNIT;
  while(frame_count < maximum_frames)
    {
      if((control_bits & FRONTEND_NAME_ENTRY_DIRECTION_MASK) == 0)
        {
          if((control_bits & FRONTEND_NAME_ENTRY_ACTION_RELEASE_MASK) != 0)
            {
              do
                {
                  control_bits = read_stable_controller_input();
                }
              while((control_bits & FRONTEND_NAME_ENTRY_ACTION_RELEASE_MASK) != 0);
            }
          else
            {
              synchronize_runtime_frame();
              if(read_stable_controller_input() != 0)
                {
                  control_bits = read_stable_controller_input();
                }
              else
                {
                  control_bits = 0;
                }
            }
        }
      else
        {
          repeat_sample = 0;
          do
            {
              read_stable_controller_input();
              read_stable_controller_input();
              control_bits = read_stable_controller_input();
              ++repeat_sample;
            }
          while((control_bits & FRONTEND_NAME_ENTRY_DIRECTION_MASK) != 0 &&
                repeat_sample < FRONTEND_NAME_ENTRY_REPEAT_SAMPLE_LIMIT);
        }

      if(control_bits == 0)
        {
          if(uppercase_displayed != 0)
            {
              uppercase_displayed = 0;
              if(draw_frontend_name_entry(0,
                                          entered_name,
                                          cursor_position.column,
                                          cursor_position.row,
                                          show_time_,
                                          show_speed_,
                                          show_score_) == 0)
                {
                  free_primary_memory(g_FRONTEND_NAME_ENTRY_SHAPES);
                  g_FRONTEND_NAME_ENTRY_SHAPES = NULL;
                  return 0;
                }
            }
          ++frame_count;
          continue;
        }

      if((control_bits & FRONTEND_NAME_ENTRY_CONFIRM_MASK) != 0)
        {
          play_confirmation_sound();
          entered_name[name_length] = 0;
          if(entered_name[0] == 0)
            {
              RUNTIME_MEMCPY_VENEER(destination_,
                                    "Anonymous",
                                    FRONTEND_NAME_ENTRY_ANONYMOUS_BYTE_COUNT);
            }
          else
            {
              copy_null_terminated_string(destination_, entered_name);
            }
          free_primary_memory(g_FRONTEND_NAME_ENTRY_SHAPES);
          g_FRONTEND_NAME_ENTRY_SHAPES = NULL;
          return 1;
        }

      if((control_bits & FRONTEND_NAME_ENTRY_BACKSPACE_MASK) != 0)
        {
          if(name_length == 0)
            {
              play_error_sound();
            }
          else
            {
              play_navigation_sound();
              --name_length;
              entered_name[name_length]     = FRONTEND_NAME_ENTRY_CURSOR_CHARACTER;
              entered_name[name_length + 1] = 0;
            }
        }

      if((control_bits & FRONTEND_NAME_ENTRY_UP_MASK) != 0)
        {
          play_navigation_sound();
          cursor_position.row = (cursor_position.row + 1) % FRONTEND_NAME_ENTRY_ROW_COUNT;
        }
      if((control_bits & FRONTEND_NAME_ENTRY_RIGHT_MASK) != 0)
        {
          play_navigation_sound();
          cursor_position.column = (cursor_position.column + 1) % FRONTEND_NAME_ENTRY_COLUMN_COUNT;
        }
      if((control_bits & FRONTEND_NAME_ENTRY_DOWN_MASK) != 0)
        {
          play_navigation_sound();
          --cursor_position.row;
          if(cursor_position.row < 0)
            {
              cursor_position.row = FRONTEND_NAME_ENTRY_ROW_COUNT - 1;
            }
        }
      if((control_bits & FRONTEND_NAME_ENTRY_LEFT_MASK) != 0)
        {
          play_navigation_sound();
          --cursor_position.column;
          if(cursor_position.column < 0)
            {
              cursor_position.column = FRONTEND_NAME_ENTRY_COLUMN_COUNT - 1;
            }
        }

      uppercase = (control_bits & FRONTEND_NAME_ENTRY_UPPERCASE_MASK) != 0;
      if((control_bits & FRONTEND_NAME_ENTRY_ADD_CHARACTER_MASK) != 0)
        {
          if(cursor_position.row == FRONTEND_NAME_ENTRY_SPECIAL_ROW && uppercase == 0 &&
             cursor_position.column == FRONTEND_NAME_ENTRY_DELETE_COLUMN)
            {
              if(name_length == 0)
                {
                  play_error_sound();
                }
              else
                {
                  play_navigation_sound();
                  --name_length;
                  entered_name[name_length]     = FRONTEND_NAME_ENTRY_CURSOR_CHARACTER;
                  entered_name[name_length + 1] = 0;
                }
            }
          else if(cursor_position.row == FRONTEND_NAME_ENTRY_SPECIAL_ROW && uppercase == 0 &&
                  cursor_position.column == FRONTEND_NAME_ENTRY_ACCEPT_COLUMN)
            {
              play_navigation_sound();
              entered_name[name_length] = 0;
              copy_null_terminated_string(destination_, entered_name);
              free_primary_memory(g_FRONTEND_NAME_ENTRY_SHAPES);
              g_FRONTEND_NAME_ENTRY_SHAPES = NULL;
              return 1;
            }
          else if(name_length >= maximum_length_)
            {
              play_error_sound();
            }
          else
            {
              play_navigation_sound();
              if(uppercase != 0)
                {
                  entered_name[name_length] =
                    g_FRONTEND_UPPERCASE_CHARACTERS
                      [(cursor_position.row * FRONTEND_NAME_ENTRY_COLUMN_COUNT) +
                       cursor_position.column];
                }
              else
                {
                  entered_name[name_length] =
                    g_FRONTEND_LOWERCASE_CHARACTERS
                      [(cursor_position.row * FRONTEND_NAME_ENTRY_COLUMN_COUNT) +
                       cursor_position.column];
                }
              ++name_length;
              entered_name[name_length]     = FRONTEND_NAME_ENTRY_CURSOR_CHARACTER;
              entered_name[name_length + 1] = 0;
            }
        }

      if((control_bits & FRONTEND_NAME_ENTRY_CANCEL_MASK) != 0)
        {
          play_confirmation_sound();
          free_primary_memory(g_FRONTEND_NAME_ENTRY_SHAPES);
          g_FRONTEND_NAME_ENTRY_SHAPES = NULL;
          RUNTIME_MEMCPY_VENEER(destination_,
                                "Anonymous",
                                FRONTEND_NAME_ENTRY_ANONYMOUS_BYTE_COUNT);
          return 0;
        }

      uppercase_displayed = uppercase;
      if(draw_frontend_name_entry(uppercase,
                                  entered_name,
                                  cursor_position.column,
                                  cursor_position.row,
                                  show_time_,
                                  show_speed_,
                                  show_score_) == 0)
        {
          free_primary_memory(g_FRONTEND_NAME_ENTRY_SHAPES);
          g_FRONTEND_NAME_ENTRY_SHAPES = NULL;
          return 0;
        }
      ++frame_count;
    }

  free_primary_memory(g_FRONTEND_NAME_ENTRY_SHAPES);
  g_FRONTEND_NAME_ENTRY_SHAPES = NULL;
  return 0;
}
