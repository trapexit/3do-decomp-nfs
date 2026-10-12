#pragma once

#include "geometry.h"
#include "simulation_thread_signal.h"

#include "types.h"

typedef struct DrivingWarningRecord DrivingWarningRecord;

typedef struct DrivingWarningMeterState
{
  int32 reserved_00;
  int32 input_value;
} DrivingWarningMeterState;

typedef struct DrivingWarningDisplayState
{
  int32                 control_input_display_timer;
  int32                 reserved_004;
  int32                 animation_frame;
  uint8                 reserved_00c[8];
  const int32          *warning_record_link_index;
  uint8                 reserved_018[4];
  int32                 driving_warning_record_count;
  int32                 secondary_driving_warning_record_count;
  int32                 prepared_warning_record_count;
  DrivingWarningRecord *first_digit_record_inputs[30];
  DrivingWarningRecord *second_digit_record_inputs[14];
  int32                 dynamic_object_link_flags_requested;
  int32                 dashboard_shake_range;
  int32                 dashboard_vertical_adjustment;
  int32                 dashboard_vertical_target;
  int32                 dashboard_vertical_return_target;
  uint8                 reserved_0_ec[4];
  int32                 repeated_warning_record_count;
  int32                 repeated_warning_records_enabled;
  uint8                 reserved_0_f8[4];
  int32                 dynamic_object_link_flags_enabled;
  int32                 dashboard_x_offset;
  int32                 dashboard_y_offset;
  void                 *dashboard_record_one;
  void                 *dashboard_family_file;
  void                 *dashboard_record_two;
  void                 *dashboard_record_three;
  void                 *dashboard_record_four;
  int32                 dashboard_motion_divisor;
  int32                 dashboard_motion_divisor_initialized;
  int32                 saved_dynamic_object_link_preamble;
  int32                 speeding_warning_active;
  int32                 warning_active;
  int32                 warning_timer;
  uint8                 warning_rendering;
  uint8                 reserved_135[3];
  uint8                 warning_cel_loaded;
  uint8                 reserved_139[3];
} DrivingWarningDisplayState;

struct DrivingWarningRecord
{
  uint32                       flags;
  struct DrivingWarningRecord *next;
  int32                        input_value;
  uint8                        reserved_00c[4];
  int32                        first_value;
  int32                        second_value;
  uint8                        reserved_018[0x18];
  uint32                       preamble;
  uint8                        reserved_034[8];
  int32                        width;
  int32                        height;
};

typedef struct WarningDisplayCallbackState WarningDisplayCallbackState;

extern DrivingWarningDisplayState g_DRIVING_WARNING_DISPLAY_STATE;
extern DrivingWarningRecord       g_DRIVING_WARNING_RECORDS[15];
extern DrivingWarningRecord       g_SECONDARY_DRIVING_WARNING_RECORDS[15];
extern DrivingWarningRecord       g_PREPARED_DRIVING_WARNING_RECORDS[6];
extern DrivingWarningRecord      *g_DRIVING_WARNING_RECORD_LINKS[30];
#define G_DRIVING_WARNING_TIMER_FLAGS (g_SIMULATION_THREAD_SIGNAL_STATE.render_flags)
extern volatile uint8           g_DRIVING_WARNING_DISPLAY_UPDATE_PENDING;
extern uint8                    g_DRIVING_WARNING_DISPLAY_RESOURCE_DEFINITION[];
extern int32                    g_DRIVING_WARNING_RATE_VALUES[4];
extern DrivingWarningMeterState g_DRIVING_WARNING_METER_STATE;

void
draw_dashboard_warning_screen_mask(void);
void
load_driving_warning_display_resource(uint8 *resource_definition_, int32 option_);
int32
initialize_driving_warning_records(const uint8 *resource_definition_, int32 selection_count_);
void
link_driving_warning_records(const uint8 *resource_definition_, int32 link_count_);
int32
load_driving_warning_cel_family(void);
void
load_dashboard_family_file(const char *base_path_, uint8 allocate_);
void
free_dashboard_family_file(void);
void
shutdown_driving_warning_display(void);
void
ensure_driving_warning_display_resource_loaded(void);
void
activate_speeding_warning_display(void);
void
activate_driving_warning_display(void);
void
clear_driving_warning_display(void);
void
render_driving_warning_display(const Vector3 *source_direction_);
void
render_driving_warning_display_variant(const Vector3 *source_direction_);
void
render_driving_warning_background(const Vector3 *source_direction_);
void
render_driving_warning_background_variant(const Vector3 *source_direction_);
void
render_active_driving_warning(void);
void
draw_driving_warning_screen_mask(void);
void
draw_driving_warning_alert_frame(void);
void
render_driving_warning_rate_values(int32 interval_);
void
render_primary_control_input_indicator(void);
void
render_secondary_control_input_indicator(void);
void
prepare_driving_dashboard_warning_records(const Vector3 *source_direction_, int32 display_mode_);
void
draw_driving_warning_backdrop(int32 left_, int32 top_);
void
draw_driving_warning_dashboard_backdrop(void);
void
configure_driving_warning_rotated_art(const Vector3 *source_direction_);
void
configure_driving_warning_dynamic_art(const Vector3 *source_direction_);
void
configure_driving_warning_transition_art(const Vector3 *source_direction_);
void
prepare_driving_warning_rate_records(const Vector3 *source_direction_,
                                     int32          base_x_,
                                     int32          base_y_);
void
render_driving_warning_dashboard(const Vector3 *source_direction_);
void
position_driving_warning_art_records(void);
void
update_driving_warning_timer_flags(void);
void
enqueue_driving_warning_record(const DrivingWarningRecord *input_,
                               int32                       first_value_,
                               int32                       second_value_);
void
enqueue_secondary_driving_warning_record(const DrivingWarningRecord *source_,
                                         int32                       first_value_,
                                         int32                       second_value_);
void
enqueue_driving_warning_digits(int32  value_,
                               int32 *first_value_,
                               int32  second_value_,
                               int32  first_step_,
                               int32  digit_count_);
void
enqueue_secondary_driving_warning_digits(int32  value_,
                                         int32 *first_value_,
                                         int32  second_value_,
                                         int32  first_step_,
                                         int32  digit_count_);
void
enqueue_driving_warning_value(int32 value_,
                              int32 first_value_,
                              int32 second_value_,
                              int32 first_step_);
void
submit_driving_warning_digits(int32  value_,
                              int32 *first_value_,
                              int32  second_value_,
                              int32  first_step_);
void
submit_secondary_driving_warning_digits(int32 value_,
                                        int32 first_value_,
                                        int32 second_value_,
                                        int32 first_step_);
void
prepare_driving_warning_record(const DrivingWarningRecord *input_,
                               int32                       first_value_,
                               int32                       second_value_);
void
prepare_driving_warning_digits(int32 value_,
                               int32 first_value_,
                               int32 second_value_,
                               int32 first_step_);
void
complete_driving_warning_display_callback(WarningDisplayCallbackState *state_);
void
install_driving_warning_display_callback(WarningDisplayCallbackState *state_,
                                         int32                        callback_count_);
