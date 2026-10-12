#include "driving_warning_display.h"
#include "game_frame_count.h"
#include "game_resource_cleanup.h"
#include "road_data.h"
#include "runtime_rate_scalars.h"
#include "simulation_snapshot.h"
#include "vehicle_runtime_pool.h"

#define DRIVING_WARNING_RATE_FIRST_INDEX 3
#define DRIVING_WARNING_RATE_SECOND_INDEX 2
#define DRIVING_WARNING_RATE_THIRD_INDEX 1
#define DRIVING_WARNING_RATE_FIRST_DIGIT_COUNT 2
#define DRIVING_WARNING_RATE_SECOND_DIGIT_COUNT 2
#define DRIVING_WARNING_RATE_THIRD_DIGIT_COUNT 1
#define DRIVING_WARNING_RATE_DIGIT_CENTER_DIVISOR 2
#define DRIVING_WARNING_RATE_RECORD_OFFSET 4
#define DRIVING_WARNING_REPEATED_RECORD_OFFSET 5
#define DRIVING_WARNING_SPECIAL_RECORD_OFFSET 6
#define DRIVING_WARNING_RECORD_WORD_COUNT 17
#define DRIVING_WARNING_RECORD_ACTIVE_FLAG 0x40000000
#define DRIVING_WARNING_SPECIAL_X_OFFSET_FIXED 0x350000
#define DRIVING_WARNING_REPEATED_X_OFFSET_FIXED 0x1D0000
#define DRIVING_WARNING_REPEATED_X_STEP_FIXED 0x40000
#define DRIVING_WARNING_REPEATED_Y_OFFSET_FIXED 0x40000

typedef struct DrivingWarningRateDefinition
{
  int32 record_count;
  uint8 reserved_004[0x244];
  int32 digit_step;
  int32 rate_x_offset;
  int32 rate_y_offset;
} DrivingWarningRateDefinition;

void
prepare_driving_warning_rate_records(const Vector3 *source_direction_, int32 base_x_, int32 base_y_)
{
  const DrivingWarningRateDefinition *definition;
  const DrivingWarningRecord         *template_record;
  const int32                        *template_words;
  DrivingWarningRecord               *record;
  int32                              *record_words;
  int32                               digit_x;
  int32                               digit_y;
  int32                               interval;
  int32                               record_word_index;
  int32                               repeated_record_index;
  int32                               base_record_index;

  (void) source_direction_;
  definition = (const DrivingWarningRateDefinition *)
                 g_DRIVING_WARNING_DISPLAY_STATE.warning_record_link_index;
  base_x_ += g_DRIVING_WARNING_DISPLAY_STATE.dashboard_x_offset;
  base_y_ += g_DRIVING_WARNING_DISPLAY_STATE.dashboard_y_offset;
  base_record_index = definition->record_count;
  record = g_DRIVING_WARNING_RECORD_LINKS[base_record_index + DRIVING_WARNING_RATE_RECORD_OFFSET];
  record->first_value  = base_x_;
  record->second_value = base_y_;

  if((g_VEHICLE_RUNTIME_FRAME_DELAY_COUNTER != 0) ||
     (g_VEHICLE_RUNTIME_POOL_STATE.road_render_state != 0) || (g_SECONDARY_ROAD_RENDER_CCB != NULL))
    {
      interval = g_GAME_RESOURCE_STATE.driving_warning_status_interval;
    }
  else
    {
      interval = G_GAME_FRAME_COUNTER;
    }
  configure_runtime_rate_scalars(interval);

  digit_x = base_x_ + definition->rate_x_offset;
  digit_y = base_y_ + definition->rate_y_offset;
  enqueue_driving_warning_digits(g_DRIVING_WARNING_RATE_VALUES[DRIVING_WARNING_RATE_FIRST_INDEX],
                                 &digit_x,
                                 digit_y,
                                 definition->digit_step,
                                 DRIVING_WARNING_RATE_FIRST_DIGIT_COUNT);
  digit_x += definition->digit_step / DRIVING_WARNING_RATE_DIGIT_CENTER_DIVISOR;
  enqueue_driving_warning_digits(g_DRIVING_WARNING_RATE_VALUES[DRIVING_WARNING_RATE_SECOND_INDEX],
                                 &digit_x,
                                 digit_y,
                                 definition->digit_step,
                                 DRIVING_WARNING_RATE_SECOND_DIGIT_COUNT);
  digit_x += definition->digit_step / DRIVING_WARNING_RATE_DIGIT_CENTER_DIVISOR;
  enqueue_driving_warning_digits(g_DRIVING_WARNING_RATE_VALUES[DRIVING_WARNING_RATE_THIRD_INDEX],
                                 &digit_x,
                                 digit_y,
                                 definition->digit_step,
                                 DRIVING_WARNING_RATE_THIRD_DIGIT_COUNT);

  digit_x = base_x_ + DRIVING_WARNING_REPEATED_X_OFFSET_FIXED;
  if((g_DRIVING_WARNING_DISPLAY_STATE.repeated_warning_records_enabled != 0) &&
     (g_DRIVING_WARNING_DISPLAY_STATE.repeated_warning_record_count != 0))
    {
      template_record =
        g_DRIVING_WARNING_RECORD_LINKS[base_record_index + DRIVING_WARNING_SPECIAL_RECORD_OFFSET];
      record =
        &g_DRIVING_WARNING_RECORDS[g_DRIVING_WARNING_DISPLAY_STATE.driving_warning_record_count];
      template_words = (const int32 *) template_record;
      record_words   = (int32 *) record;
      for(record_word_index = 0; record_word_index < DRIVING_WARNING_RECORD_WORD_COUNT;
          ++record_word_index)
        {
          record_words[record_word_index] = template_words[record_word_index];
        }
      record->next = record + 1;
      record->flags &= ~DRIVING_WARNING_RECORD_ACTIVE_FLAG;
      enqueue_driving_warning_record(template_record,
                                     base_x_ + DRIVING_WARNING_SPECIAL_X_OFFSET_FIXED,
                                     base_y_ + DRIVING_WARNING_REPEATED_Y_OFFSET_FIXED);
    }

  for(repeated_record_index = 0;
      repeated_record_index < g_DRIVING_WARNING_DISPLAY_STATE.repeated_warning_record_count - 1;
      ++repeated_record_index)
    {
      digit_x += DRIVING_WARNING_REPEATED_X_STEP_FIXED;
      template_record =
        g_DRIVING_WARNING_RECORD_LINKS[base_record_index + DRIVING_WARNING_REPEATED_RECORD_OFFSET];
      record =
        &g_DRIVING_WARNING_RECORDS[g_DRIVING_WARNING_DISPLAY_STATE.driving_warning_record_count];
      template_words = (const int32 *) template_record;
      record_words   = (int32 *) record;
      for(record_word_index = 0; record_word_index < DRIVING_WARNING_RECORD_WORD_COUNT;
          ++record_word_index)
        {
          record_words[record_word_index] = template_words[record_word_index];
        }
      record->next = record + 1;
      record->flags &= ~DRIVING_WARNING_RECORD_ACTIVE_FLAG;
      enqueue_driving_warning_record(template_record,
                                     digit_x,
                                     base_y_ + DRIVING_WARNING_REPEATED_Y_OFFSET_FIXED);
    }
}
