#include "game_standings.h"

#include "controller_input.h"
#include "frontend_frame.h"
#include "post_race_speed.h"
#include "post_race_stats.h"
#include "post_race_status.h"
#include "race_state.h"
#include "runtime_memory.h"
#include "tracked_memory.h"

#define GAME_STANDINGS_PAGE_COUNT 3
#define GAME_STANDINGS_TRACK_COUNT 4
#define GAME_STANDINGS_SHAPE_NAME_BYTE_COUNT 8
#define GAME_STANDINGS_LINE_LIMIT 160
#define GAME_STANDINGS_LINE_BYTE_COUNT 60
#define GAME_STANDINGS_BUFFER_BYTE_COUNT 0x2580
#define GAME_STANDINGS_TRACK_RECORD_BYTE_COUNT 0x1C0
#define GAME_STANDINGS_EVENT_RECORD_BYTE_COUNT 0x70
#define GAME_STANDINGS_TIMING_RECORD_BYTE_COUNT 0xB0
#define GAME_STANDINGS_ENTRY_BYTE_COUNT 0x1C
#define GAME_STANDINGS_ENTRY_COUNT 3
#define GAME_STANDINGS_ENTRY_NAME_OFFSET 0
#define GAME_STANDINGS_ENTRY_CLASS_OFFSET 0x0E
#define GAME_STANDINGS_ENTRY_VEHICLE_OFFSET 0x0F
#define GAME_STANDINGS_ENTRY_TIME_OFFSET 0x10
#define GAME_STANDINGS_SPECIAL_ENTRY_VALUE_OFFSET 0x18
#define GAME_STANDINGS_TIME_TEXT_BYTE_COUNT 12
#define GAME_STANDINGS_EVENT_ENTRIES_OFFSET 4
#define GAME_STANDINGS_ROUTE_ENTRIES_OFFSET 0x154
#define GAME_STANDINGS_SPECIAL_EVENT_ENTRY_OFFSET 0x54
#define GAME_STANDINGS_SPECIAL_ROUTE_ENTRY_OFFSET 0x1A8
#define GAME_STANDINGS_SUMMARY_TIMING_OFFSET 0x214
#define GAME_STANDINGS_PRIMARY_TIME_OFFSET 4
#define GAME_STANDINGS_SECONDARY_TIME_OFFSET 0x9C
#define GAME_STANDINGS_PRIMARY_FRAME_SPEED_OFFSET 0x10
#define GAME_STANDINGS_SECONDARY_FRAME_SPEED_OFFSET 0xA8
#define GAME_STANDINGS_PRIMARY_SECOND_SPEED_OFFSET 0x0C
#define GAME_STANDINGS_SECONDARY_SECOND_SPEED_OFFSET 0xA4
#define GAME_STANDINGS_PRIMARY_VALUE_A_OFFSET 0x88
#define GAME_STANDINGS_PRIMARY_VALUE_B_OFFSET 0x84
#define GAME_STANDINGS_PRIMARY_VALUE_C_OFFSET 0x90
#define GAME_STANDINGS_SUMMARY_SECOND_TIME_OFFSET 0x224
#define GAME_STANDINGS_SUMMARY_FIRST_SPEED_OFFSET 0x220
#define GAME_STANDINGS_SUMMARY_SECOND_SPEED_OFFSET 0x230
#define GAME_STANDINGS_SUMMARY_VALUE_OFFSET 0x238
#define GAME_STANDINGS_SUMMARY_TOTAL_A_OFFSET 0x240
#define GAME_STANDINGS_SUMMARY_TOTAL_B_OFFSET 0x23C
#define GAME_STANDINGS_INPUT_FRAME_LIMIT 0xE10
#define GAME_STANDINGS_INPUT_MASK 0xCF000000
#define GAME_STANDINGS_NEXT_PAGE_MASK 0x8E000000
#define GAME_STANDINGS_PREVIOUS_PAGE_MASK 0x40000000
#define GAME_STANDINGS_EXIT_MASK 0x01800000
#define GAME_STANDINGS_SUMMARY_TEXT_X 0x99
#define GAME_STANDINGS_SUMMARY_TEXT_Y 0x26
#define GAME_STANDINGS_DETAIL_TEXT_X 0x3C
#define GAME_STANDINGS_DETAIL_TEXT_Y 0x4A
#define GAME_STANDINGS_LINE_HEIGHT 0x0C
#define GAME_STANDINGS_POSITION_WORD 3
#define GAME_STANDINGS_POSITION_X_SHIFT 16
#define GAME_STANDINGS_EVENT_TEXT_X 0x66
#define GAME_STANDINGS_EVENT_TEXT_Y 0x1A
#define GAME_STANDINGS_EVENT_TEXT_BYTE_COUNT 2
#define GAME_STANDINGS_SUMMARY_PAGE 0
#define GAME_STANDINGS_SEGMENT_PAGE 1
#define GAME_STANDINGS_ROUTE_PAGE 2
#define GAME_STANDINGS_NO_OPPONENT_MODE 8
#define GAME_STANDINGS_DETAIL_BLANK_LINE_COUNT 4
#define GAME_STANDINGS_SUMMARY_BLANK_LINE 7
#define GAME_STANDINGS_SUMMARY_BLANK_LINE_COUNT 2
#define GAME_STANDINGS_SUMMARY_FRAME_SPEED_LINE 2
#define GAME_STANDINGS_SUMMARY_SECOND_SPEED_LINE 3
#define GAME_STANDINGS_SUMMARY_VALUE_A_LINE 4
#define GAME_STANDINGS_SUMMARY_VALUE_B_LINE 5
#define GAME_STANDINGS_SUMMARY_VALUE_C_LINE 6
#define GAME_STANDINGS_SUMMARY_NAMES_LINE 9
#define GAME_STANDINGS_SUMMARY_TIMES_LINE 10
#define GAME_STANDINGS_SUMMARY_SPEEDS_LINE 11
#define GAME_STANDINGS_SUMMARY_VALUE_LINE 12
#define GAME_STANDINGS_SUMMARY_TOTAL_LINE 13
#define GAME_STANDINGS_SUMMARY_LINE_COUNT 14
#define GAME_STANDINGS_EMPTY_TEXT ""
#define GAME_STANDINGS_BUFFER_NAME "StatsBuf"
#define GAME_STANDINGS_SHAPE_PATH "frontend/display/gamestandings.3sh"
#define GAME_STANDINGS_SUMMARY_BACKGROUND "bgd1"
#define GAME_STANDINGS_DETAIL_BACKGROUND "bgd2"
#define GAME_STANDINGS_SEGMENT_SHAPE "seg "
#define GAME_STANDINGS_TWO_COLUMN_NAME_FORMAT "%6s %6s"
#define GAME_STANDINGS_TWO_COLUMN_TIME_FORMAT "%7s %7s"
#define GAME_STANDINGS_TWO_COLUMN_VALUE_FORMAT "%5s %5s"
#define GAME_STANDINGS_NUMBER_FORMAT "%d"
#define GAME_STANDINGS_ENTRY_FORMAT "%-10s %7s  %-6s %s"
#define GAME_STANDINGS_SPECIAL_ENTRY_FORMAT "%-10s %5s  %-6s %s"

static char *g_GAME_STANDING_LINES[GAME_STANDINGS_LINE_LIMIT];

typedef struct GameStandingPageLiterals
{
  char   route_shape[GAME_STANDINGS_SHAPE_NAME_BYTE_COUNT];
  char **lines;
} GameStandingPageLiterals;

static const GameStandingPageLiterals G_GAME_STANDING_PAGE_LITERALS = { "rout",
                                                                        g_GAME_STANDING_LINES };

static const char G_GAME_STANDING_TRACK_SHAPE_STORAGE[GAME_STANDINGS_TRACK_COUNT]
                                                     [GAME_STANDINGS_SHAPE_NAME_BYTE_COUNT] = {
                                                       "alpn",
                                                       "cstl",
                                                       "city",
                                                       "test"
                                                     };

static const char *g_GAME_STANDING_TRACK_SHAPES[GAME_STANDINGS_TRACK_COUNT] = {
  G_GAME_STANDING_TRACK_SHAPE_STORAGE[0],
  G_GAME_STANDING_TRACK_SHAPE_STORAGE[1],
  G_GAME_STANDING_TRACK_SHAPE_STORAGE[2],
  G_GAME_STANDING_TRACK_SHAPE_STORAGE[3]
};
typedef char
  GameStandingTrackShapeStorageSizeCheck[sizeof(G_GAME_STANDING_TRACK_SHAPE_STORAGE) == 32 ? 1 :
                                                                                             -1];
typedef char GameStandingTrackShapesSizeCheck[sizeof(g_GAME_STANDING_TRACK_SHAPES) == 16 ? 1 : -1];
typedef char
  GameStandingPageLiteralsSizeCheck[sizeof(G_GAME_STANDING_PAGE_LITERALS) == 12 ? 1 : -1];

static int32
_read_game_standing_value(const uint8 *record_, int32 offset_)
{
  return *(const int32 *) (record_ + offset_);
}

static void
_clear_game_standing_lines(char **lines_, int32 first_line_, int32 line_count_)
{
  int32 line_index;

  for(line_index = 0; line_index < line_count_; ++line_index)
    {
      lines_[first_line_ + line_index][0] = 0;
    }
}

static void
_format_game_standing_entry(char *destination_, const uint8 *entry_, const char *format_)
{
  char time_text[GAME_STANDINGS_TIME_TEXT_BYTE_COUNT];

  format_post_race_time(_read_game_standing_value(entry_, GAME_STANDINGS_ENTRY_TIME_OFFSET),
                        time_text);
  if(_read_game_standing_value(entry_, GAME_STANDINGS_ENTRY_TIME_OFFSET) == 0)
    {
      destination_[0] = 0;
      return;
    }
  runtime_format_string(destination_,
                        format_,
                        entry_ + GAME_STANDINGS_ENTRY_NAME_OFFSET,
                        time_text,
                        g_POST_RACE_VEHICLE_NAMES[entry_[GAME_STANDINGS_ENTRY_VEHICLE_OFFSET]],
                        g_POST_RACE_CLASS_NAMES[entry_[GAME_STANDINGS_ENTRY_CLASS_OFFSET]]);
}

static void
_format_game_standing_special_entry(char *destination_, const uint8 *entry_)
{
  char speed_text[GAME_STANDINGS_TIME_TEXT_BYTE_COUNT];

  format_per_second_speed_mph(
    _read_game_standing_value(entry_, GAME_STANDINGS_SPECIAL_ENTRY_VALUE_OFFSET),
    speed_text);
  if(_read_game_standing_value(entry_, GAME_STANDINGS_SPECIAL_ENTRY_VALUE_OFFSET) == 0)
    {
      destination_[0] = 0;
      return;
    }
  runtime_format_string(destination_,
                        GAME_STANDINGS_SPECIAL_ENTRY_FORMAT,
                        entry_ + GAME_STANDINGS_ENTRY_NAME_OFFSET,
                        speed_text,
                        g_POST_RACE_VEHICLE_NAMES[entry_[GAME_STANDINGS_ENTRY_VEHICLE_OFFSET]],
                        g_POST_RACE_CLASS_NAMES[entry_[GAME_STANDINGS_ENTRY_CLASS_OFFSET]]);
}

static void
format_game_standings_lines(int32 page_, char **lines_, int32 *line_count_)
{
  uint8 *event_entries;
  uint8 *track_records;
  uint8 *route_entries;
  uint8 *timing_record;
  char   first_text[GAME_STANDINGS_TIME_TEXT_BYTE_COUNT];
  char   second_text[GAME_STANDINGS_TIME_TEXT_BYTE_COUNT];
  int32  entry_index;
  int32  line_count;

  track_records = RACE_PARTICIPANT_RECORDS +
                  RACE_CONFIGURATION->track_index * GAME_STANDINGS_TRACK_RECORD_BYTE_COUNT;
  event_entries = track_records + GAME_STANDINGS_EVENT_ENTRIES_OFFSET +
                  RACE_CONFIGURATION->event_index * GAME_STANDINGS_EVENT_RECORD_BYTE_COUNT;
  route_entries = track_records + GAME_STANDINGS_ROUTE_ENTRIES_OFFSET;
  timing_record = (uint8 *) RACE_TIMING_STATE +
                  RACE_CONFIGURATION->event_index * GAME_STANDINGS_TIMING_RECORD_BYTE_COUNT;

  if(page_ == GAME_STANDINGS_SUMMARY_PAGE)
    {
      runtime_format_string(lines_[0],
                            GAME_STANDINGS_TWO_COLUMN_NAME_FORMAT,
                            g_POST_RACE_VEHICLE_NAMES[RACE_CONFIGURATION->player_vehicle_index],
                            g_POST_RACE_VEHICLE_NAMES[RACE_CONFIGURATION->opponent_vehicle_index]);
      format_post_race_time(
        _read_game_standing_value(timing_record, GAME_STANDINGS_PRIMARY_TIME_OFFSET),
        first_text);
      format_post_race_time(
        _read_game_standing_value(timing_record, GAME_STANDINGS_SECONDARY_TIME_OFFSET),
        second_text);
      if(RACE_CONFIGURATION->opponent_vehicle_index == GAME_STANDINGS_NO_OPPONENT_MODE)
        {
          second_text[0] = 0;
        }
      runtime_format_string(lines_[1],
                            GAME_STANDINGS_TWO_COLUMN_TIME_FORMAT,
                            first_text,
                            second_text);
      format_per_frame_speed_mph(
        _read_game_standing_value(timing_record, GAME_STANDINGS_PRIMARY_FRAME_SPEED_OFFSET),
        first_text);
      format_per_frame_speed_mph(
        _read_game_standing_value(timing_record, GAME_STANDINGS_SECONDARY_FRAME_SPEED_OFFSET),
        second_text);
      if(RACE_CONFIGURATION->opponent_vehicle_index == GAME_STANDINGS_NO_OPPONENT_MODE)
        {
          second_text[0] = 0;
        }
      runtime_format_string(lines_[GAME_STANDINGS_SUMMARY_FRAME_SPEED_LINE],
                            GAME_STANDINGS_TWO_COLUMN_VALUE_FORMAT,
                            first_text,
                            second_text);
      format_per_second_speed_mph(
        _read_game_standing_value(timing_record, GAME_STANDINGS_PRIMARY_SECOND_SPEED_OFFSET),
        first_text);
      format_per_second_speed_mph(
        _read_game_standing_value(timing_record, GAME_STANDINGS_SECONDARY_SECOND_SPEED_OFFSET),
        second_text);
      if(RACE_CONFIGURATION->opponent_vehicle_index == GAME_STANDINGS_NO_OPPONENT_MODE)
        {
          second_text[0] = 0;
        }
      runtime_format_string(lines_[GAME_STANDINGS_SUMMARY_SECOND_SPEED_LINE],
                            GAME_STANDINGS_TWO_COLUMN_VALUE_FORMAT,
                            first_text,
                            second_text);
      runtime_format_string(
        lines_[GAME_STANDINGS_SUMMARY_VALUE_A_LINE],
        GAME_STANDINGS_NUMBER_FORMAT,
        _read_game_standing_value(timing_record, GAME_STANDINGS_PRIMARY_VALUE_A_OFFSET));
      runtime_format_string(
        lines_[GAME_STANDINGS_SUMMARY_VALUE_B_LINE],
        GAME_STANDINGS_NUMBER_FORMAT,
        _read_game_standing_value(timing_record, GAME_STANDINGS_PRIMARY_VALUE_B_OFFSET));
      runtime_format_string(
        lines_[GAME_STANDINGS_SUMMARY_VALUE_C_LINE],
        GAME_STANDINGS_NUMBER_FORMAT,
        _read_game_standing_value(timing_record, GAME_STANDINGS_PRIMARY_VALUE_C_OFFSET));
      _clear_game_standing_lines(lines_,
                                 GAME_STANDINGS_SUMMARY_BLANK_LINE,
                                 GAME_STANDINGS_SUMMARY_BLANK_LINE_COUNT);
      runtime_format_string(lines_[GAME_STANDINGS_SUMMARY_NAMES_LINE],
                            GAME_STANDINGS_TWO_COLUMN_NAME_FORMAT,
                            g_POST_RACE_VEHICLE_NAMES[RACE_CONFIGURATION->player_vehicle_index],
                            g_POST_RACE_VEHICLE_NAMES[RACE_CONFIGURATION->opponent_vehicle_index]);
      timing_record = (uint8 *) RACE_TIMING_STATE;
      format_post_race_time(
        _read_game_standing_value(timing_record, GAME_STANDINGS_SUMMARY_TIMING_OFFSET),
        first_text);
      format_post_race_time(
        _read_game_standing_value(timing_record, GAME_STANDINGS_SUMMARY_SECOND_TIME_OFFSET),
        second_text);
      if(RACE_CONFIGURATION->opponent_vehicle_index == GAME_STANDINGS_NO_OPPONENT_MODE)
        {
          second_text[0] = 0;
        }
      runtime_format_string(lines_[GAME_STANDINGS_SUMMARY_TIMES_LINE],
                            GAME_STANDINGS_TWO_COLUMN_TIME_FORMAT,
                            first_text,
                            second_text);
      format_per_second_speed_mph(
        _read_game_standing_value(timing_record, GAME_STANDINGS_SUMMARY_FIRST_SPEED_OFFSET),
        first_text);
      format_per_second_speed_mph(
        _read_game_standing_value(timing_record, GAME_STANDINGS_SUMMARY_SECOND_SPEED_OFFSET),
        second_text);
      if(RACE_CONFIGURATION->opponent_vehicle_index == GAME_STANDINGS_NO_OPPONENT_MODE)
        {
          second_text[0] = 0;
        }
      runtime_format_string(lines_[GAME_STANDINGS_SUMMARY_SPEEDS_LINE],
                            GAME_STANDINGS_TWO_COLUMN_VALUE_FORMAT,
                            first_text,
                            second_text);
      runtime_format_string(
        lines_[GAME_STANDINGS_SUMMARY_VALUE_LINE],
        GAME_STANDINGS_NUMBER_FORMAT,
        _read_game_standing_value(timing_record, GAME_STANDINGS_SUMMARY_VALUE_OFFSET));
      runtime_format_string(
        lines_[GAME_STANDINGS_SUMMARY_TOTAL_LINE],
        GAME_STANDINGS_NUMBER_FORMAT,
        _read_game_standing_value(timing_record, GAME_STANDINGS_SUMMARY_TOTAL_A_OFFSET) +
          _read_game_standing_value(timing_record, GAME_STANDINGS_SUMMARY_TOTAL_B_OFFSET) -
          _read_game_standing_value(timing_record, GAME_STANDINGS_SUMMARY_VALUE_OFFSET));
      *line_count_ = GAME_STANDINGS_SUMMARY_LINE_COUNT;
      return;
    }

  if(page_ == GAME_STANDINGS_ROUTE_PAGE)
    {
      event_entries = route_entries;
    }
  line_count = 0;
  for(entry_index = 0; entry_index < GAME_STANDINGS_ENTRY_COUNT; ++entry_index)
    {
      _format_game_standing_entry(lines_[line_count],
                                  event_entries + entry_index * GAME_STANDINGS_ENTRY_BYTE_COUNT,
                                  GAME_STANDINGS_ENTRY_FORMAT);
      ++line_count;
    }
  _clear_game_standing_lines(lines_, line_count, GAME_STANDINGS_DETAIL_BLANK_LINE_COUNT);
  line_count += GAME_STANDINGS_DETAIL_BLANK_LINE_COUNT;
  _format_game_standing_special_entry(lines_[line_count],
                                      page_ == GAME_STANDINGS_SEGMENT_PAGE ?
                                        event_entries + GAME_STANDINGS_SPECIAL_EVENT_ENTRY_OFFSET :
                                        track_records + GAME_STANDINGS_SPECIAL_ROUTE_ENTRY_OFFSET);
  if(lines_[line_count][0] != 0)
    {
      ++line_count;
    }
  *line_count_ = line_count;
}

static void
draw_game_standings_page(int32 page_, int32 line_count_, void *shape_collection_)
{
  char  event_text[GAME_STANDINGS_EVENT_TEXT_BYTE_COUNT];
  void *shape;
  int32 line_index;
  int32 packed_position;
  int32 text_x;
  int32 text_y;

  begin_runtime_frame();
  synchronize_runtime_frame();
  clear_runtime_display(0);
  shape           = locate_runtime_shape(shape_collection_,
                                         (const uint32 *) (page_ == GAME_STANDINGS_SUMMARY_PAGE ?
                                                             GAME_STANDINGS_SUMMARY_BACKGROUND :
                                                             GAME_STANDINGS_DETAIL_BACKGROUND));
  packed_position = ((int32 *) shape)[GAME_STANDINGS_POSITION_WORD];
  blit_runtime_shape_at(shape,
                        packed_position >> GAME_STANDINGS_POSITION_X_SHIFT,
                        (int16) packed_position);
  shape = locate_runtime_shape(
    shape_collection_,
    (const uint32 *) g_GAME_STANDING_TRACK_SHAPES[RACE_CONFIGURATION->track_index]);
  packed_position = ((int32 *) shape)[GAME_STANDINGS_POSITION_WORD];
  draw_runtime_shape_at(shape,
                        packed_position >> GAME_STANDINGS_POSITION_X_SHIFT,
                        (int16) packed_position);

  if(page_ == GAME_STANDINGS_ROUTE_PAGE)
    {
      shape = locate_runtime_shape(shape_collection_,
                                   (const uint32 *) G_GAME_STANDING_PAGE_LITERALS.route_shape);
      packed_position = ((int32 *) shape)[GAME_STANDINGS_POSITION_WORD];
      blit_runtime_shape_at(shape,
                            packed_position >> GAME_STANDINGS_POSITION_X_SHIFT,
                            (int16) packed_position);
    }
  else
    {
      if(page_ == GAME_STANDINGS_SEGMENT_PAGE)
        {
          shape =
            locate_runtime_shape(shape_collection_, (const uint32 *) GAME_STANDINGS_SEGMENT_SHAPE);
          packed_position = ((int32 *) shape)[GAME_STANDINGS_POSITION_WORD];
          blit_runtime_shape_at(shape,
                                packed_position >> GAME_STANDINGS_POSITION_X_SHIFT,
                                (int16) packed_position);
        }
      event_text[0] = (char) (RACE_CONFIGURATION->event_index + '1');
      event_text[1] = 0;
      draw_runtime_text(event_text, GAME_STANDINGS_EVENT_TEXT_X, GAME_STANDINGS_EVENT_TEXT_Y);
    }

  text_x = page_ == GAME_STANDINGS_SUMMARY_PAGE ? GAME_STANDINGS_SUMMARY_TEXT_X :
                                                  GAME_STANDINGS_DETAIL_TEXT_X;
  text_y = page_ == GAME_STANDINGS_SUMMARY_PAGE ? GAME_STANDINGS_SUMMARY_TEXT_Y :
                                                  GAME_STANDINGS_DETAIL_TEXT_Y;
  for(line_index = 0; line_index < line_count_; ++line_index)
    {
      draw_runtime_text(G_GAME_STANDING_PAGE_LITERALS.lines[line_index],
                        text_x,
                        text_y + line_index * GAME_STANDINGS_LINE_HEIGHT);
    }
  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();
}

void
run_game_standings(void)
{
  char  *text_buffer;
  void  *shape_collection;
  uint32 input;
  int32  line_count;
  int32  line_index;
  int32  page;

  page = GAME_STANDINGS_SUMMARY_PAGE;
  initialize_frontend_display();
  text_buffer = (char *) allocate_named_primary_memory(GAME_STANDINGS_BUFFER_BYTE_COUNT,
                                                       GAME_STANDINGS_BUFFER_NAME);
  if(text_buffer == NULL)
    {
      return;
    }
  for(line_index = 0; line_index < GAME_STANDINGS_LINE_LIMIT; ++line_index)
    {
      g_GAME_STANDING_LINES[line_index] = text_buffer + line_index * GAME_STANDINGS_LINE_BYTE_COUNT;
    }
  shape_collection = load_runtime_file_data(GAME_STANDINGS_SHAPE_PATH, 0);
  track_primary_allocation(shape_collection);

  format_game_standings_lines(page, g_GAME_STANDING_LINES, &line_count);
  draw_game_standings_page(page, line_count, shape_collection);
  for(;;)
    {
      input = poll_controller_input(GAME_STANDINGS_INPUT_FRAME_LIMIT, 1, GAME_STANDINGS_INPUT_MASK);
      if(input == 0)
        {
          break;
        }
      if(((input & GAME_STANDINGS_NEXT_PAGE_MASK) != 0) && (page < GAME_STANDINGS_PAGE_COUNT - 1))
        {
          ++page;
        }
      if(((input & GAME_STANDINGS_PREVIOUS_PAGE_MASK) != 0) && (page > 0))
        {
          --page;
        }
      if((input & GAME_STANDINGS_EXIT_MASK) != 0)
        {
          break;
        }
      format_game_standings_lines(page, g_GAME_STANDING_LINES, &line_count);
      draw_game_standings_page(page, line_count, shape_collection);
    }

  free_primary_memory(text_buffer);
  free_primary_memory(shape_collection);
}
