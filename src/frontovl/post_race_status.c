#include "post_race_status.h"

#include "post_race_speed.h"
#include "race_state.h"
#include "runtime_memory.h"

#include "operamath.h"

#define POST_RACE_MPH_FORMAT "%s mph"
#define POST_RACE_TIME_FORMAT "%2d:%02d.%d"

enum
{
  POST_RACE_EVENT_BYTE_COUNT         = 0xB0,
  POST_RACE_EVENT_TIME_OFFSET        = 4,
  POST_RACE_EVENT_PER_SECOND_OFFSET  = 0x0C,
  POST_RACE_EVENT_PER_FRAME_OFFSET   = 0x10,
  POST_RACE_FRAMES_PER_SECOND        = 60,
  POST_RACE_FRAMES_PER_MINUTE        = 3600,
  POST_RACE_FRAMES_PER_HOUR          = 216000,
  POST_RACE_TENTHS_PER_FRAME_FIXED16 = 0x0001AAA6,
  POST_RACE_TENTHS_DIVISOR           = 10,
  POST_RACE_SPEED_BUFFER_BYTES       = 12
};

void
format_post_race_time(int32 frame_count_, char *destination_)
{
  int32 fractional_frame_count;
  int32 minute_count;
  int32 second_count;
  int32 tenth_count;

  fractional_frame_count = (frame_count_ % POST_RACE_FRAMES_PER_HOUR);
  minute_count           = (fractional_frame_count / POST_RACE_FRAMES_PER_MINUTE);
  fractional_frame_count -= (minute_count * POST_RACE_FRAMES_PER_MINUTE);
  second_count = (fractional_frame_count / POST_RACE_FRAMES_PER_SECOND);
  fractional_frame_count -= (second_count * POST_RACE_FRAMES_PER_SECOND);
  tenth_count = (MulSF16(fractional_frame_count, POST_RACE_TENTHS_PER_FRAME_FIXED16) /
                 POST_RACE_TENTHS_DIVISOR);
  runtime_format_string(destination_,
                        POST_RACE_TIME_FORMAT,
                        minute_count,
                        second_count,
                        tenth_count);
}

void
format_ranked_post_race_status(char *event_time_destination_,
                               char *overall_time_destination_,
                               char *per_frame_speed_destination_,
                               char *per_second_speed_destination_)
{
  char   speed_text[POST_RACE_SPEED_BUFFER_BYTES];
  uint8 *event_timing;

  event_timing =
    ((uint8 *) RACE_TIMING_STATE + (RACE_CONFIGURATION->event_index * POST_RACE_EVENT_BYTE_COUNT));
  format_post_race_time(*(int32 *) (event_timing + POST_RACE_EVENT_TIME_OFFSET),
                        event_time_destination_);
  format_post_race_time(RACE_TIMING_STATE->primary_time, overall_time_destination_);
  format_per_frame_speed_mph(*(int32 *) (event_timing + POST_RACE_EVENT_PER_FRAME_OFFSET),
                             speed_text);
  runtime_format_string(per_frame_speed_destination_, POST_RACE_MPH_FORMAT, speed_text);
  format_per_second_speed_mph(*(int32 *) (event_timing + POST_RACE_EVENT_PER_SECOND_OFFSET),
                              speed_text);
  runtime_format_string(per_second_speed_destination_, POST_RACE_MPH_FORMAT, speed_text);
}

void
format_post_race_status(char *time_destination_,
                        char *per_frame_speed_destination_,
                        char *per_second_speed_destination_)
{
  char speed_text[POST_RACE_SPEED_BUFFER_BYTES];

  format_post_race_time(RACE_TIMING_STATE->primary_time, time_destination_);
  format_per_frame_speed_mph(RACE_TIMING_STATE->alternate_time, speed_text);
  runtime_format_string(per_frame_speed_destination_, POST_RACE_MPH_FORMAT, speed_text);
  format_per_second_speed_mph(RACE_TIMING_STATE->progress, speed_text);
  runtime_format_string(per_second_speed_destination_, POST_RACE_MPH_FORMAT, speed_text);
}
