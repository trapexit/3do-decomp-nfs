#include "driving_warning_display.h"

#include "bitmap_clip.h"
#include "control_input_poll_state.h"
#include "game_frame_count.h"
#include "headered_file.h"
#include "memory.h"
#include "player_vehicle_state.h"
#include "pointer_array.h"
#include "render_thread_signal.h"
#include "simulation_snapshot.h"
#include "string_utilities.h"
#include "vehicle_callbacks.h"
#include "vehicle_dynamics_state.h"
#include "vehicle_drive_statistics.h"
#include "vehicle_simulation_state.h"

#define DASHBOARD_FAMILY_PATH_BYTES 0x50
#define DASHBOARD_FAMILY_PATH_SUFFIX ".BigDashFam"
#define DRIVING_WARNING_TIMER_INITIAL_VALUE -1000
#define DRIVING_WARNING_TIMER_RESET_FLAG_ONE 0x01
#define DRIVING_WARNING_TIMER_RESET_FLAG_MASK 0x03
#define DRIVING_WARNING_TIMER_FLAG_ONE 0x01
#define DRIVING_WARNING_TIMER_FLAG_TWO 0x04
#define DRIVING_WARNING_RECORD_ACTIVE_FLAG 0x40000000
#define DRIVING_WARNING_RECORD_WORD_COUNT 17
#define PREPARED_DRIVING_WARNING_RECORD_COUNT 6
#define DRIVING_WARNING_RECORD_LINK_COUNT 30
#define SPEEDING_WARNING_CEL_FAMILY_PATH "drivedata/driveart/speedingt0.celFam"
#define WARNING_CEL_FAMILY_PATH "drivedata/driveart/warningt0.celFam"
#define DRIVING_WARNING_DIGIT_DIVISOR 10
#define DRIVING_WARNING_MAXIMUM_DIGIT_COUNT 10
#define DRIVING_WARNING_PREPARED_DIGIT_BASE_INDEX 10
#define DRIVING_WARNING_NEUTRAL_RECORD_INDEX 10
#define DRIVING_WARNING_REVERSE_RECORD_INDEX 11
#define DRIVING_WARNING_DISPLAY_FINISHING_STATE 2
#define DRIVING_WARNING_RESOURCE_DEFINITION_BYTES 0x74
#define DRIVING_WARNING_DASHBOARD_PRIMARY_RECORD_INDEX 29
#define DRIVING_WARNING_DASHBOARD_SECONDARY_RECORD_INDEX 28
#define DRIVING_WARNING_DASHBOARD_CENTER_X_FIXED 0xA00000
#define DRIVING_WARNING_DASHBOARD_WIDTH_SCALE_FIXED 0x10000
#define DRIVING_WARNING_DASHBOARD_RECORD_Y_FIXED 0x230000
#define DRIVING_WARNING_PRIMARY_DIGIT_X_OFFSET_FIXED 0x780000
#define DRIVING_WARNING_PRIMARY_DIGIT_Y_FIXED 0x250000
#define DRIVING_WARNING_SECONDARY_DIGIT_X_OFFSET_FIXED 0x900000
#define DRIVING_WARNING_SECONDARY_DIGIT_Y_FIXED 0x260000
#define DRIVING_WARNING_DIGIT_STEP_FIXED 0x90000
#define DRIVING_WARNING_WIDTH_CENTER_DIVISOR 2
#define DRIVING_WARNING_VARIANT_CYCLE_INPUT 0x48200000
#define DRIVING_WARNING_VARIANT_MODE_LIMIT 3
#define DRIVING_WARNING_VARIANT_RENDER_COMMAND 10

typedef union WarningDisplayCallback
{
  void (*complete)(WarningDisplayCallbackState *state_);
  void (*update)(void *state_);
} WarningDisplayCallback;

typedef struct WarningDisplayCallbackOwner
{
  uint8                  reserved_000[0x4c4];
  WarningDisplayCallback callback;
} WarningDisplayCallbackOwner;

typedef struct WarningDisplayCallbackCounter
{
  uint8 reserved_000[0x84];
  int32 pending_callback_count;
} WarningDisplayCallbackCounter;

struct WarningDisplayCallbackState
{
  uint8                          reserved_000[0x7c];
  WarningDisplayCallbackOwner   *callback_owner;
  uint8                          reserved_080[0x34];
  WarningDisplayCallbackCounter *callback_counter;
};

DrivingWarningDisplayState g_DRIVING_WARNING_DISPLAY_STATE;
DrivingWarningRecord       g_DRIVING_WARNING_RECORDS[15];
DrivingWarningRecord       g_SECONDARY_DRIVING_WARNING_RECORDS[15];
DrivingWarningRecord  g_PREPARED_DRIVING_WARNING_RECORDS[PREPARED_DRIVING_WARNING_RECORD_COUNT];
DrivingWarningRecord *g_DRIVING_WARNING_RECORD_LINKS[DRIVING_WARNING_RECORD_LINK_COUNT];
volatile uint8        g_DRIVING_WARNING_DISPLAY_UPDATE_PENDING;
uint8                 g_DRIVING_WARNING_DISPLAY_UPDATE_PADDING[3];
uint8 g_DRIVING_WARNING_DISPLAY_RESOURCE_DEFINITION[DRIVING_WARNING_RESOURCE_DEFINITION_BYTES];

void
render_driving_warning_display_variant(const Vector3 *source_direction_)
{
  if((g_DRIVING_WARNING_DISPLAY_STATE.speeding_warning_active != 0) ||
     (g_DRIVING_WARNING_DISPLAY_STATE.warning_active != 0))
    {
      (void) load_driving_warning_cel_family();
      render_active_driving_warning();
    }
  else
    {
      ensure_driving_warning_display_resource_loaded();
      if(g_DRIVING_WARNING_DISPLAY_STATE.animation_frame == 0)
        render_driving_warning_dashboard(source_direction_);
      else
        render_driving_warning_background_variant(source_direction_);
      g_DRIVING_WARNING_DISPLAY_STATE.warning_timer = DRIVING_WARNING_TIMER_INITIAL_VALUE;
    }

  if(g_CONTROL_INPUT_POLL_STATE.status_04 != DRIVING_WARNING_VARIANT_CYCLE_INPUT)
    {
      return;
    }
  ++g_DRIVING_WARNING_DISPLAY_STATE.animation_frame;
  if(g_DRIVING_WARNING_DISPLAY_STATE.animation_frame > DRIVING_WARNING_VARIANT_MODE_LIMIT)
    {
      g_DRIVING_WARNING_DISPLAY_STATE.animation_frame = 0;
    }
  (void) send_render_io_command(DRIVING_WARNING_VARIANT_RENDER_COMMAND);
}

void
load_dashboard_family_file(const char *base_path_, uint8 allocate_)
{
  char path[DASHBOARD_FAMILY_PATH_BYTES];

  copy_string(path, base_path_);
  append_string(path, DASHBOARD_FAMILY_PATH_SUFFIX);
  if(allocate_ != 0)
    {
      g_DRIVING_WARNING_DISPLAY_STATE.dashboard_family_file =
        load_headered_file_with_default_options(path);
    }
  else
    {
      load_headered_resource_file(path, g_DRIVING_WARNING_DISPLAY_STATE.dashboard_family_file);
    }
  g_DRIVING_WARNING_DISPLAY_STATE.warning_timer = DRIVING_WARNING_TIMER_INITIAL_VALUE;
}

void
free_dashboard_family_file(void)
{
  if(g_DRIVING_WARNING_DISPLAY_STATE.dashboard_family_file != NULL)
    free_memory(g_DRIVING_WARNING_DISPLAY_STATE.dashboard_family_file);
}

void
shutdown_driving_warning_display(void)
{
  free_memory(g_DRIVING_WARNING_DISPLAY_STATE.dashboard_family_file);
  g_DRIVING_WARNING_DISPLAY_STATE.dashboard_family_file   = NULL;
  g_DRIVING_WARNING_DISPLAY_STATE.speeding_warning_active = 0;
  g_DRIVING_WARNING_DISPLAY_STATE.warning_active          = 0;
  g_DRIVING_WARNING_DISPLAY_UPDATE_PENDING                = 1;
  g_DRIVING_WARNING_DISPLAY_UPDATE_PENDING                = 0;
  g_DRIVING_WARNING_DISPLAY_STATE.animation_frame         = 0;
}

int32
load_driving_warning_cel_family(void)
{
  const char *cel_family_path;

  if(g_DRIVING_WARNING_DISPLAY_STATE.warning_cel_loaded != 0)
    {
      return 0;
    }

  g_DRIVING_WARNING_DISPLAY_UPDATE_PENDING           = 1;
  g_DRIVING_WARNING_DISPLAY_STATE.warning_rendering  = 0;
  g_DRIVING_WARNING_DISPLAY_STATE.warning_cel_loaded = 1;
  if(g_DRIVING_WARNING_DISPLAY_STATE.speeding_warning_active != 0)
    {
      cel_family_path = SPEEDING_WARNING_CEL_FAMILY_PATH;
    }
  else
    {
      cel_family_path = WARNING_CEL_FAMILY_PATH;
    }

  load_headered_resource_file(cel_family_path,
                              g_DRIVING_WARNING_DISPLAY_STATE.dashboard_family_file);
  g_DRIVING_WARNING_DISPLAY_STATE.dashboard_record_three = get_headered_pointer_array_element(
    (void *const *) g_DRIVING_WARNING_DISPLAY_STATE.dashboard_family_file,
    0);
  g_DRIVING_WARNING_DISPLAY_UPDATE_PENDING = 0;
  return 0;
}

void
render_driving_warning_display(const Vector3 *source_direction_)
{
  if((g_DRIVING_WARNING_DISPLAY_STATE.speeding_warning_active != 0) ||
     (g_DRIVING_WARNING_DISPLAY_STATE.warning_active != 0))
    {
      (void) load_driving_warning_cel_family();
      render_active_driving_warning();
      return;
    }

  ensure_driving_warning_display_resource_loaded();
  render_driving_warning_background(source_direction_);
  g_DRIVING_WARNING_DISPLAY_STATE.warning_timer = DRIVING_WARNING_TIMER_INITIAL_VALUE;
}

void
render_driving_warning_background(const Vector3 *source_direction_)
{
  g_DRIVING_WARNING_DISPLAY_STATE.driving_warning_record_count           = 0;
  g_DRIVING_WARNING_DISPLAY_STATE.secondary_driving_warning_record_count = 0;
  g_DRIVING_WARNING_DISPLAY_STATE.prepared_warning_record_count          = 0;
  prepare_driving_dashboard_warning_records(source_direction_, 0);

  if(g_DRIVING_WARNING_DISPLAY_STATE.control_input_display_timer != 0)
    {
      render_primary_control_input_indicator();
    }
  else if(g_VEHICLE_STATISTICS_NOTIFICATION_TIMER != 0)
    {
      render_secondary_control_input_indicator();
    }

  render_active_driving_warning();
}

void
render_driving_warning_background_variant(const Vector3 *source_direction_)
{
  g_DRIVING_WARNING_DISPLAY_STATE.driving_warning_record_count  = 0;
  g_DRIVING_WARNING_DISPLAY_STATE.prepared_warning_record_count = 0;
  prepare_driving_dashboard_warning_records(source_direction_,
                                            g_DRIVING_WARNING_DISPLAY_STATE.animation_frame);

  if(g_DRIVING_WARNING_DISPLAY_STATE.control_input_display_timer != 0)
    {
      render_primary_control_input_indicator();
    }
  else if(g_VEHICLE_STATISTICS_NOTIFICATION_TIMER != 0)
    {
      render_secondary_control_input_indicator();
    }

  render_active_driving_warning();
}

void
render_primary_control_input_indicator(void)
{
  DrivingWarningRecord *record;
  int32                 first_value;
  int32                 display_value;

  record      = g_DRIVING_WARNING_DISPLAY_STATE
                  .first_digit_record_inputs[DRIVING_WARNING_DASHBOARD_PRIMARY_RECORD_INDEX];
  first_value = DRIVING_WARNING_DASHBOARD_CENTER_X_FIXED -
                ((record->width / DRIVING_WARNING_WIDTH_CENTER_DIVISOR) *
                 DRIVING_WARNING_DASHBOARD_WIDTH_SCALE_FIXED);
  record->first_value  = first_value;
  record->second_value = DRIVING_WARNING_DASHBOARD_RECORD_Y_FIXED;
  (void) draw_current_bitmap_cels((CCB *) record);

  display_value = (g_VEHICLE_DRIVE_STATISTICS.summary.lane_search_limit -
                   (int32) g_VEHICLE_DRIVE_STATISTICS.summary.recovery_count +
                   g_VEHICLE_DRIVE_STATISTICS.summary.recovery_adjustment);
  submit_secondary_driving_warning_digits(display_value,
                                          first_value +
                                            DRIVING_WARNING_PRIMARY_DIGIT_X_OFFSET_FIXED,
                                          DRIVING_WARNING_PRIMARY_DIGIT_Y_FIXED,
                                          DRIVING_WARNING_DIGIT_STEP_FIXED);
  if(g_DRIVING_WARNING_DISPLAY_STATE.control_input_display_timer > 0)
    g_DRIVING_WARNING_DISPLAY_STATE.control_input_display_timer--;
}

void
render_secondary_control_input_indicator(void)
{
  DrivingWarningRecord *width_record;
  DrivingWarningRecord *record;
  int32                 first_value;
  int32                 display_value;

  width_record = g_DRIVING_WARNING_DISPLAY_STATE
                   .first_digit_record_inputs[DRIVING_WARNING_DASHBOARD_PRIMARY_RECORD_INDEX];
  record       = g_DRIVING_WARNING_DISPLAY_STATE
                   .first_digit_record_inputs[DRIVING_WARNING_DASHBOARD_SECONDARY_RECORD_INDEX];
  first_value  = DRIVING_WARNING_DASHBOARD_CENTER_X_FIXED -
                 ((width_record->width / DRIVING_WARNING_WIDTH_CENTER_DIVISOR) *
                  DRIVING_WARNING_DASHBOARD_WIDTH_SCALE_FIXED);
  record->first_value  = first_value;
  record->second_value = DRIVING_WARNING_DASHBOARD_RECORD_Y_FIXED;
  (void) draw_current_bitmap_cels((CCB *) record);

  display_value = (g_VEHICLE_DRIVE_STATISTICS.summary.lane_search_limit -
                   (int32) g_VEHICLE_DRIVE_STATISTICS.summary.recovery_count +
                   g_VEHICLE_DRIVE_STATISTICS.summary.recovery_adjustment);
  submit_secondary_driving_warning_digits(display_value,
                                          first_value +
                                            DRIVING_WARNING_SECONDARY_DIGIT_X_OFFSET_FIXED,
                                          DRIVING_WARNING_SECONDARY_DIGIT_Y_FIXED,
                                          DRIVING_WARNING_DIGIT_STEP_FIXED);
  if(g_VEHICLE_STATISTICS_NOTIFICATION_TIMER > 0)
    g_VEHICLE_STATISTICS_NOTIFICATION_TIMER--;
}

void
ensure_driving_warning_display_resource_loaded(void)
{
  if(g_DRIVING_WARNING_DISPLAY_STATE.warning_rendering != 0)
    {
      return;
    }
  g_DRIVING_WARNING_DISPLAY_UPDATE_PENDING = 1;
  load_driving_warning_display_resource(g_DRIVING_WARNING_DISPLAY_RESOURCE_DEFINITION, 0);
  g_DRIVING_WARNING_DISPLAY_UPDATE_PENDING = 0;
}

void
activate_speeding_warning_display(void)
{
  g_DRIVING_WARNING_DISPLAY_STATE.speeding_warning_active = 1;
  g_DRIVING_WARNING_DISPLAY_STATE.warning_timer           = DRIVING_WARNING_TIMER_INITIAL_VALUE;
}

void
activate_driving_warning_display(void)
{
  g_DRIVING_WARNING_DISPLAY_STATE.warning_active = 1;
  g_DRIVING_WARNING_DISPLAY_STATE.warning_timer  = DRIVING_WARNING_TIMER_INITIAL_VALUE;
}

void
clear_driving_warning_display(void)
{
  if((g_DRIVING_WARNING_DISPLAY_STATE.speeding_warning_active != 0) ||
     (g_DRIVING_WARNING_DISPLAY_STATE.warning_active != 0))
    {
      g_DRIVING_WARNING_DISPLAY_STATE.warning_timer = G_GAME_FRAME_COUNTER;
    }
  g_DRIVING_WARNING_DISPLAY_STATE.speeding_warning_active = 0;
  g_DRIVING_WARNING_DISPLAY_STATE.warning_active          = 0;
}

void
update_driving_warning_timer_flags(void)
{
  G_DRIVING_WARNING_TIMER_FLAGS = 0;
  if((G_GAME_FRAME_COUNTER & DRIVING_WARNING_TIMER_RESET_FLAG_ONE) == 0)
    {
      G_DRIVING_WARNING_TIMER_FLAGS = DRIVING_WARNING_TIMER_FLAG_ONE;
    }
  if((G_GAME_FRAME_COUNTER & DRIVING_WARNING_TIMER_RESET_FLAG_MASK) == 0)
    {
      G_DRIVING_WARNING_TIMER_FLAGS |= DRIVING_WARNING_TIMER_FLAG_TWO;
    }
}

void
enqueue_driving_warning_record(const DrivingWarningRecord *input_,
                               int32                       first_value_,
                               int32                       second_value_)
{
  DrivingWarningRecord *record;
  DrivingWarningRecord *previous_record;

  record = &g_DRIVING_WARNING_RECORDS[g_DRIVING_WARNING_DISPLAY_STATE.driving_warning_record_count];
  record->first_value  = first_value_;
  record->second_value = second_value_;
  record->input_value  = input_->input_value;
  record->flags &= ~DRIVING_WARNING_RECORD_ACTIVE_FLAG;
  if(g_DRIVING_WARNING_DISPLAY_STATE.driving_warning_record_count > 0)
    {
      previous_record =
        &g_DRIVING_WARNING_RECORDS[g_DRIVING_WARNING_DISPLAY_STATE.driving_warning_record_count -
                                   1];
      previous_record->next = record;
    }
  ++g_DRIVING_WARNING_DISPLAY_STATE.driving_warning_record_count;
}

void
enqueue_secondary_driving_warning_record(const DrivingWarningRecord *source_,
                                         int32                       first_value_,
                                         int32                       second_value_)
{
  DrivingWarningRecord *record;
  DrivingWarningRecord *previous_record;
  int32                 record_word_index;
  const int32          *source_words;
  int32                *record_words;

  record       = &g_SECONDARY_DRIVING_WARNING_RECORDS[g_DRIVING_WARNING_DISPLAY_STATE
                                                        .secondary_driving_warning_record_count];
  source_words = (const int32 *) source_;
  record_words = (int32 *) record;
  for(record_word_index = 0; record_word_index < DRIVING_WARNING_RECORD_WORD_COUNT;
      ++record_word_index)
    {
      record_words[record_word_index] = source_words[record_word_index];
    }
  record->first_value  = first_value_;
  record->second_value = second_value_;
  record->flags &= ~DRIVING_WARNING_RECORD_ACTIVE_FLAG;
  if(g_DRIVING_WARNING_DISPLAY_STATE.secondary_driving_warning_record_count > 0)
    {
      previous_record =
        &g_SECONDARY_DRIVING_WARNING_RECORDS
          [g_DRIVING_WARNING_DISPLAY_STATE.secondary_driving_warning_record_count - 1];
      previous_record->next = record;
    }
  ++g_DRIVING_WARNING_DISPLAY_STATE.secondary_driving_warning_record_count;
}

void
prepare_driving_warning_record(const DrivingWarningRecord *input_,
                               int32                       first_value_,
                               int32                       second_value_)
{
  DrivingWarningRecord *record;
  int32                 record_link_index;

  record               = &g_PREPARED_DRIVING_WARNING_RECORDS[g_DRIVING_WARNING_DISPLAY_STATE
                                                               .prepared_warning_record_count];
  record->next         = record + 1;
  record               = record->next;
  record->input_value  = input_->input_value;
  record_link_index    = *g_DRIVING_WARNING_DISPLAY_STATE.warning_record_link_index;
  record->next         = g_DRIVING_WARNING_RECORD_LINKS[record_link_index + 1];
  record->first_value  = first_value_;
  record->second_value = second_value_;
  ++g_DRIVING_WARNING_DISPLAY_STATE.prepared_warning_record_count;
}

void
prepare_driving_warning_digits(int32 value_,
                               int32 first_value_,
                               int32 second_value_,
                               int32 first_step_)
{
  int32 digit_index;
  int32 digits[DRIVING_WARNING_MAXIMUM_DIGIT_COUNT];

  digit_index = 0;
  while(value_ >= DRIVING_WARNING_DIGIT_DIVISOR)
    {
      digits[digit_index] =
        value_ - ((value_ / DRIVING_WARNING_DIGIT_DIVISOR) * DRIVING_WARNING_DIGIT_DIVISOR);
      value_ /= DRIVING_WARNING_DIGIT_DIVISOR;
      ++digit_index;
    }
  digits[digit_index] = value_;

  do
    {
      prepare_driving_warning_record(
        g_DRIVING_WARNING_DISPLAY_STATE.first_digit_record_inputs
          [DRIVING_WARNING_PREPARED_DIGIT_BASE_INDEX + digits[digit_index]],
        first_value_,
        second_value_);
      first_value_ -= first_step_;
      --digit_index;
    }
  while(digit_index >= 0);
}

void
enqueue_driving_warning_digits(int32  value_,
                               int32 *first_value_,
                               int32  second_value_,
                               int32  first_step_,
                               int32  digit_count_)
{
  int32 digit_index;
  int32 digits[DRIVING_WARNING_MAXIMUM_DIGIT_COUNT];

  for(digit_index = 0; digit_index < digit_count_; ++digit_index)
    {
      if(value_ >= DRIVING_WARNING_DIGIT_DIVISOR)
        {
          digits[digit_index] =
            value_ - ((value_ / DRIVING_WARNING_DIGIT_DIVISOR) * DRIVING_WARNING_DIGIT_DIVISOR);
          value_ /= DRIVING_WARNING_DIGIT_DIVISOR;
        }
      else
        {
          digits[digit_index] = value_;
          value_              = 0;
        }
    }

  while(digit_count_ > 0)
    {
      --digit_count_;
      enqueue_driving_warning_record(
        g_DRIVING_WARNING_DISPLAY_STATE.first_digit_record_inputs[digits[digit_count_]],
        *first_value_,
        second_value_);
      *first_value_ += first_step_;
    }
}

void
enqueue_secondary_driving_warning_digits(int32  value_,
                                         int32 *first_value_,
                                         int32  second_value_,
                                         int32  first_step_,
                                         int32  digit_count_)
{
  int32 digit_index;
  int32 digits[DRIVING_WARNING_MAXIMUM_DIGIT_COUNT];

  for(digit_index = 0; digit_index < digit_count_; ++digit_index)
    {
      if(value_ >= DRIVING_WARNING_DIGIT_DIVISOR)
        {
          digits[digit_index] =
            value_ - ((value_ / DRIVING_WARNING_DIGIT_DIVISOR) * DRIVING_WARNING_DIGIT_DIVISOR);
          value_ /= DRIVING_WARNING_DIGIT_DIVISOR;
        }
      else
        {
          digits[digit_index] = value_;
          value_              = 0;
        }
    }

  while(digit_count_ > 0)
    {
      --digit_count_;
      enqueue_secondary_driving_warning_record(
        g_DRIVING_WARNING_DISPLAY_STATE.second_digit_record_inputs[digits[digit_count_]],
        *first_value_,
        second_value_);
      *first_value_ += first_step_;
    }
}

void
enqueue_driving_warning_value(int32 value_,
                              int32 first_value_,
                              int32 second_value_,
                              int32 first_step_)
{
  int32 digit_index;
  int32 display_digit_index;
  int32 digits[DRIVING_WARNING_MAXIMUM_DIGIT_COUNT];

  if(value_ < 0)
    {
      if(value_ == -1)
        {
          enqueue_driving_warning_record(
            g_DRIVING_WARNING_DISPLAY_STATE
              .second_digit_record_inputs[DRIVING_WARNING_NEUTRAL_RECORD_INDEX],
            first_value_,
            second_value_);
        }
      else
        {
          enqueue_driving_warning_record(
            g_DRIVING_WARNING_DISPLAY_STATE
              .second_digit_record_inputs[DRIVING_WARNING_REVERSE_RECORD_INDEX],
            first_value_,
            second_value_);
        }
      return;
    }

  digit_index = 0;
  while(value_ >= DRIVING_WARNING_DIGIT_DIVISOR)
    {
      digits[digit_index] =
        value_ - ((value_ / DRIVING_WARNING_DIGIT_DIVISOR) * DRIVING_WARNING_DIGIT_DIVISOR);
      value_ /= DRIVING_WARNING_DIGIT_DIVISOR;
      ++digit_index;
    }
  digits[digit_index] = value_;

  display_digit_index = 0;
  do
    {
      enqueue_driving_warning_record(
        g_DRIVING_WARNING_DISPLAY_STATE.first_digit_record_inputs[digits[display_digit_index]],
        first_value_,
        second_value_);
      first_value_ -= first_step_;
      ++display_digit_index;
    }
  while(display_digit_index <= digit_index);
}

void
submit_driving_warning_digits(int32  value_,
                              int32 *first_value_,
                              int32  second_value_,
                              int32  first_step_)
{
  int32                 digit_index;
  int32                 digits[DRIVING_WARNING_MAXIMUM_DIGIT_COUNT];
  DrivingWarningRecord *record;

  digit_index = 0;
  while(value_ >= DRIVING_WARNING_DIGIT_DIVISOR)
    {
      digits[digit_index] =
        value_ - ((value_ / DRIVING_WARNING_DIGIT_DIVISOR) * DRIVING_WARNING_DIGIT_DIVISOR);
      value_ /= DRIVING_WARNING_DIGIT_DIVISOR;
      ++digit_index;
    }
  digits[digit_index] = value_;

  do
    {
      record = g_DRIVING_WARNING_DISPLAY_STATE.first_digit_record_inputs[digits[digit_index]];
      record->first_value  = *first_value_;
      record->second_value = second_value_;
      record->flags |= DRIVING_WARNING_RECORD_ACTIVE_FLAG;
      *first_value_ += first_step_;
      draw_current_bitmap_cels((CCB *) record);
      --digit_index;
    }
  while(digit_index >= 0);
}

void
submit_secondary_driving_warning_digits(int32 value_,
                                        int32 first_value_,
                                        int32 second_value_,
                                        int32 first_step_)
{
  int32                 digit_index;
  int32                 digits[DRIVING_WARNING_MAXIMUM_DIGIT_COUNT];
  DrivingWarningRecord *record;

  digit_index = 0;
  while(value_ >= DRIVING_WARNING_DIGIT_DIVISOR)
    {
      digits[digit_index] =
        value_ - ((value_ / DRIVING_WARNING_DIGIT_DIVISOR) * DRIVING_WARNING_DIGIT_DIVISOR);
      value_ /= DRIVING_WARNING_DIGIT_DIVISOR;
      ++digit_index;
    }
  digits[digit_index] = value_;

  do
    {
      record = g_DRIVING_WARNING_DISPLAY_STATE.second_digit_record_inputs[digits[digit_index]];
      record->first_value  = first_value_;
      record->second_value = second_value_;
      first_value_ -= first_step_;
      draw_current_bitmap_cels((CCB *) record);
      --digit_index;
    }
  while(digit_index >= 0);
}

void
complete_driving_warning_display_callback(WarningDisplayCallbackState *state_)
{
  SimulationSource *source;

  --state_->callback_counter->pending_callback_count;
  if(state_->callback_counter->pending_callback_count > 0)
    return;

  source                                  = (SimulationSource *) G_ACTIVE_SIMULATION_SOURCE;
  state_->callback_owner->callback.update = update_driving_warning_vehicle_state;
  clear_driving_warning_display();
  source->warning_display_pending = 0;
  source->warning_display_index   = -1;
  if(source->warning_display_state != 0)
    source->warning_display_state = DRIVING_WARNING_DISPLAY_FINISHING_STATE;
  g_PLAYER_VEHICLE_RECOVERY_PENDING = 1;
}

void
install_driving_warning_display_callback(WarningDisplayCallbackState *state_, int32 callback_count_)
{
  state_->callback_counter->pending_callback_count = callback_count_;
  state_->callback_owner->callback.complete        = complete_driving_warning_display_callback;
}
