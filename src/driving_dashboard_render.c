#include "bitmap_clip.h"
#include "driving_warning_display.h"
#include "game_frame_count.h"
#include "game_resource_cleanup.h"
#include "road_data.h"
#include "rotated_transform.h"
#include "runtime_rate_scalars.h"
#include "simulation_snapshot.h"
#include "vehicle_parameter_loader.h"
#include "vehicle_runtime_pool.h"

#include "operamath.h"

#include "stddef.h"

#define DASHBOARD_ACTIVE_RECORD_FLAG 0x40000000
#define DASHBOARD_ART_RECORD_BASE_INDEX 21
#define DASHBOARD_ART_RECORD_MODE_ONE_FIRST_INDEX 0
#define DASHBOARD_ART_RECORD_MODE_ONE_SECOND_INDEX 1
#define DASHBOARD_ART_RECORD_MODE_SPECIAL_INDEX 2
#define DASHBOARD_ART_RECORD_MAIN_INDEX 3
#define DASHBOARD_ART_RECORD_SECONDARY_INDEX 4
#define DASHBOARD_ART_RECORD_ROTATED_SECONDARY_INDEX 5
#define DASHBOARD_ART_RECORD_ROTATED_PRIMARY_INDEX 6
#define DASHBOARD_MODE_DEFAULT 0
#define DASHBOARD_MODE_EXPANDED 1
#define DASHBOARD_MODE_COMPACT 2
#define DASHBOARD_MODE_SPECIAL 3
#define DASHBOARD_DEFAULT_X_FIXED 0x190000
#define DASHBOARD_ALTERNATE_X_FIXED 0x4B0000
#define DASHBOARD_MAIN_Y_FIXED 0xA50000
#define DASHBOARD_SECONDARY_X_OFFSET_FIXED 0x30000
#define DASHBOARD_SECONDARY_Y_FIXED 0xAD0000
#define DASHBOARD_SPECIAL_X_FIXED 0x3C0000
#define DASHBOARD_SPECIAL_Y_FIXED 0xBE0000
#define DASHBOARD_ROTATED_X_OFFSET_FIXED 0x200000
#define DASHBOARD_ROTATED_Y_FIXED 0xCA0000
#define DASHBOARD_ROTATED_SECONDARY_POINT_X (-0x30000)
#define DASHBOARD_ROTATED_SECONDARY_POINT_Y (-0x280000)
#define DASHBOARD_ROTATED_PRIMARY_POINT_X (-0x30000)
#define DASHBOARD_ROTATED_PRIMARY_POINT_Y (-0x230000)
#define DASHBOARD_ROTATION_BASE_FIXED (-0x400000)
#define DASHBOARD_ROTATION_LIMIT_FIXED 0x740000
#define DASHBOARD_ENGINE_SPEED_DIVISOR_FIXED 0x698000
#define DASHBOARD_SPEED_SCALE_FIXED 0x23D70
#define DASHBOARD_SPEED_DIGIT_X_OFFSET_FIXED 0x310000
#define DASHBOARD_INDEX_DIGIT_X_OFFSET_FIXED 0x380000
#define DASHBOARD_RATE_DIGIT_X_OFFSET_FIXED 0xA0000
#define DASHBOARD_SPEED_DIGIT_Y_FIXED 0xD40000
#define DASHBOARD_INDEX_DIGIT_Y_FIXED 0xC90000
#define DASHBOARD_TEMPLATE_RECORD_COUNT 8
#define DASHBOARD_RATE_FIRST_INDEX 3
#define DASHBOARD_RATE_SECOND_INDEX 2
#define DASHBOARD_RATE_THIRD_INDEX 1
#define DASHBOARD_RATE_FIRST_DIGIT_COUNT 2
#define DASHBOARD_RATE_SECOND_DIGIT_COUNT 2
#define DASHBOARD_RATE_THIRD_DIGIT_COUNT 1
#define DASHBOARD_DIGIT_CENTER_DIVISOR 2
#define DASHBOARD_MODE_ONE_FIRST_Y_FIXED 0xAE0000
#define DASHBOARD_MODE_ONE_SECOND_Y_FIXED 0xC20000
#define DASHBOARD_MODE_ONE_THIRD_Y_FIXED 0xD60000
#define DASHBOARD_BACKDROP_LEFT 0xB4
#define DASHBOARD_BACKDROP_TOP 0xB9
#define DASHBOARD_FIXED16_SHIFT 16

typedef struct DrivingWarningDashboardDefinition
{
  uint8 reserved_000[0x248];
  int32 digit_step;
} DrivingWarningDashboardDefinition;

int32 g_DRIVING_WARNING_RATE_VALUES[4];

void
prepare_driving_dashboard_warning_records(const Vector3 *source_direction_, int32 display_mode_)
{
  const SimulationSource                  *source;
  const VehicleParameterState             *vehicle_state;
  const DrivingWarningDashboardDefinition *definition;
  DrivingWarningRecord                    *record;
  DrivingWarningRecord                    *template_record;
  DrivingWarningRecord                    *last_record;
  RotatedTransform2D                      *transform;
  Point2                                   point;
  int32                                    base_x;
  int32                                    rotation;
  int32                                    speed;
  int32                                    display_index;
  int32                                    digit_x;
  int32                                    rate_interval;
  int32                                    record_index;

  source = (const SimulationSource *) ((const uint8 *) source_direction_ -
                                       offsetof(SimulationSource, direction));
  vehicle_state = (const VehicleParameterState *) source_direction_;
  definition = (const DrivingWarningDashboardDefinition *)
                 g_DRIVING_WARNING_DISPLAY_STATE.warning_record_link_index;
  base_x     = DASHBOARD_DEFAULT_X_FIXED;
  if(display_mode_ != DASHBOARD_MODE_DEFAULT)
    base_x = DASHBOARD_ALTERNATE_X_FIXED;
  if(display_mode_ == DASHBOARD_MODE_SPECIAL)
    {
      record               = g_DRIVING_WARNING_DISPLAY_STATE
                               .first_digit_record_inputs[DASHBOARD_ART_RECORD_BASE_INDEX +
                                                          DASHBOARD_ART_RECORD_MODE_SPECIAL_INDEX];
      record->first_value  = DASHBOARD_SPECIAL_X_FIXED;
      record->second_value = DASHBOARD_SPECIAL_Y_FIXED;
      record->flags |= DASHBOARD_ACTIVE_RECORD_FLAG;
      (void) draw_current_bitmap_cels((CCB *) record);
      return;
    }

  speed = vehicle_state->vehicle_speed;
  if(speed < 0)
    speed = 0;
  record =
    g_DRIVING_WARNING_DISPLAY_STATE
      .first_digit_record_inputs[DASHBOARD_ART_RECORD_BASE_INDEX + DASHBOARD_ART_RECORD_MAIN_INDEX];
  record->first_value  = base_x;
  record->second_value = DASHBOARD_MAIN_Y_FIXED;
  record =
    g_DRIVING_WARNING_DISPLAY_STATE.first_digit_record_inputs[DASHBOARD_ART_RECORD_BASE_INDEX +
                                                              DASHBOARD_ART_RECORD_SECONDARY_INDEX];
  record->first_value  = base_x + DASHBOARD_SECONDARY_X_OFFSET_FIXED;
  record->second_value = DASHBOARD_SECONDARY_Y_FIXED;

  point.x = DASHBOARD_ROTATED_PRIMARY_POINT_X;
  point.y = DASHBOARD_ROTATED_PRIMARY_POINT_Y;
  if(g_DRIVING_WARNING_DISPLAY_STATE.dashboard_motion_divisor_initialized == 0)
    {
      g_DRIVING_WARNING_DISPLAY_STATE.dashboard_motion_divisor =
        (DASHBOARD_ENGINE_SPEED_DIVISOR_FIXED /
         vehicle_state->parameters->maximum_engine_speed);
      g_DRIVING_WARNING_DISPLAY_STATE.dashboard_motion_divisor_initialized = 1;
    }
  rotation = (g_DRIVING_WARNING_DISPLAY_STATE.dashboard_motion_divisor *
              source->dashboard_speed_value);
  if(rotation > DASHBOARD_ROTATION_LIMIT_FIXED)
    rotation = DASHBOARD_ROTATION_LIMIT_FIXED;
  transform = (RotatedTransform2D *) g_DRIVING_WARNING_DISPLAY_STATE
                .first_digit_record_inputs[DASHBOARD_ART_RECORD_BASE_INDEX +
                                           DASHBOARD_ART_RECORD_ROTATED_PRIMARY_INDEX];
  configure_rotated_2d_transform(&point,
                                 rotation + DASHBOARD_ROTATION_BASE_FIXED,
                                 base_x + DASHBOARD_ROTATED_X_OFFSET_FIXED,
                                 DASHBOARD_ROTATED_Y_FIXED,
                                 transform);

  point.x   = DASHBOARD_ROTATED_SECONDARY_POINT_X;
  point.y   = DASHBOARD_ROTATED_SECONDARY_POINT_Y;
  transform = (RotatedTransform2D *) g_DRIVING_WARNING_DISPLAY_STATE
                .first_digit_record_inputs[DASHBOARD_ART_RECORD_BASE_INDEX +
                                           DASHBOARD_ART_RECORD_ROTATED_SECONDARY_INDEX];
  configure_rotated_2d_transform(&point,
                                 source->track_transform.m01,
                                 base_x + DASHBOARD_ROTATED_X_OFFSET_FIXED,
                                 DASHBOARD_ROTATED_Y_FIXED,
                                 transform);

  g_DRIVING_WARNING_DISPLAY_STATE.driving_warning_record_count = 0;
  digit_x = base_x + DASHBOARD_SPEED_DIGIT_X_OFFSET_FIXED;
  speed   = MulSF16(speed, DASHBOARD_SPEED_SCALE_FIXED);
  enqueue_driving_warning_value(speed >> DASHBOARD_FIXED16_SHIFT,
                                digit_x,
                                DASHBOARD_SPEED_DIGIT_Y_FIXED,
                                definition->digit_step);
  digit_x       = base_x + DASHBOARD_INDEX_DIGIT_X_OFFSET_FIXED;
  display_index = vehicle_state->gear_control_index;
  if(display_index >= 0)
    display_index++;
  enqueue_driving_warning_value(display_index,
                                digit_x,
                                DASHBOARD_INDEX_DIGIT_Y_FIXED,
                                definition->digit_step);

  template_record = g_DRIVING_WARNING_DISPLAY_STATE.first_digit_record_inputs[0];
  for(record_index = 0; record_index < DASHBOARD_TEMPLATE_RECORD_COUNT; record_index++)
    {
      g_DRIVING_WARNING_RECORDS[g_DRIVING_WARNING_DISPLAY_STATE.driving_warning_record_count +
                                record_index] = *template_record;
    }

  digit_x = (base_x + DASHBOARD_RATE_DIGIT_X_OFFSET_FIXED);
  if((g_VEHICLE_RUNTIME_FRAME_DELAY_COUNTER != 0) ||
     (g_VEHICLE_RUNTIME_POOL_STATE.road_render_state != 0) ||
     (g_SECONDARY_ROAD_RENDER_CCB != NULL))
    {
      rate_interval = g_GAME_RESOURCE_STATE.driving_warning_status_interval;
    }
  else
    {
      rate_interval = G_GAME_FRAME_COUNTER;
    }

  configure_runtime_rate_scalars(rate_interval);
  enqueue_driving_warning_digits(g_DRIVING_WARNING_RATE_VALUES[DASHBOARD_RATE_FIRST_INDEX],
                                 &digit_x,
                                 DASHBOARD_SPEED_DIGIT_Y_FIXED,
                                 definition->digit_step,
                                 DASHBOARD_RATE_FIRST_DIGIT_COUNT);
  digit_x += (definition->digit_step / DASHBOARD_DIGIT_CENTER_DIVISOR);
  enqueue_driving_warning_digits(g_DRIVING_WARNING_RATE_VALUES[DASHBOARD_RATE_SECOND_INDEX],
                                 &digit_x,
                                 DASHBOARD_SPEED_DIGIT_Y_FIXED,
                                 definition->digit_step,
                                 DASHBOARD_RATE_SECOND_DIGIT_COUNT);
  digit_x += (definition->digit_step / DASHBOARD_DIGIT_CENTER_DIVISOR);
  enqueue_driving_warning_digits(g_DRIVING_WARNING_RATE_VALUES[DASHBOARD_RATE_THIRD_INDEX],
                                 &digit_x,
                                 DASHBOARD_SPEED_DIGIT_Y_FIXED,
                                 definition->digit_step,
                                 DASHBOARD_RATE_THIRD_DIGIT_COUNT);

  record       = g_DRIVING_WARNING_DISPLAY_STATE
                   .first_digit_record_inputs[DASHBOARD_ART_RECORD_BASE_INDEX +
                                              DASHBOARD_ART_RECORD_ROTATED_PRIMARY_INDEX];
  record->next = g_DRIVING_WARNING_RECORDS;
  record->flags &= ~DASHBOARD_ACTIVE_RECORD_FLAG;
  last_record =
    &g_DRIVING_WARNING_RECORDS[g_DRIVING_WARNING_DISPLAY_STATE.driving_warning_record_count - 1];
  last_record->flags |= DASHBOARD_ACTIVE_RECORD_FLAG;

  if(display_mode_ == DASHBOARD_MODE_DEFAULT)
    {
      record =
        g_DRIVING_WARNING_DISPLAY_STATE.first_digit_record_inputs[DASHBOARD_ART_RECORD_BASE_INDEX +
                                                                  DASHBOARD_ART_RECORD_MAIN_INDEX];
      (void) draw_current_bitmap_cels((CCB *) record);
    }
  else if(display_mode_ == DASHBOARD_MODE_EXPANDED)
    {
      record              = g_DRIVING_WARNING_DISPLAY_STATE
                              .first_digit_record_inputs[DASHBOARD_ART_RECORD_BASE_INDEX +
                                                         DASHBOARD_ART_RECORD_MODE_ONE_FIRST_INDEX];
      record->first_value = DASHBOARD_SPECIAL_X_FIXED;
      record->second_value = DASHBOARD_MODE_ONE_FIRST_Y_FIXED;
      record = g_DRIVING_WARNING_DISPLAY_STATE
                 .first_digit_record_inputs[DASHBOARD_ART_RECORD_BASE_INDEX +
                                            DASHBOARD_ART_RECORD_MODE_ONE_SECOND_INDEX];
      record->first_value  = DASHBOARD_SPECIAL_X_FIXED;
      record->second_value = DASHBOARD_MODE_ONE_SECOND_Y_FIXED;
      record               = g_DRIVING_WARNING_DISPLAY_STATE
                               .first_digit_record_inputs[DASHBOARD_ART_RECORD_BASE_INDEX +
                                                          DASHBOARD_ART_RECORD_MODE_SPECIAL_INDEX];
      record->first_value  = DASHBOARD_SPECIAL_X_FIXED;
      record->second_value = DASHBOARD_MODE_ONE_THIRD_Y_FIXED;
      record->flags &= ~DASHBOARD_ACTIVE_RECORD_FLAG;
      record =
        g_DRIVING_WARNING_DISPLAY_STATE.first_digit_record_inputs[DASHBOARD_ART_RECORD_BASE_INDEX];
      (void) draw_current_bitmap_cels((CCB *) record);
      draw_driving_warning_backdrop(DASHBOARD_BACKDROP_LEFT, DASHBOARD_BACKDROP_TOP);
    }
  else if(display_mode_ == DASHBOARD_MODE_COMPACT)
    {
      record               = g_DRIVING_WARNING_DISPLAY_STATE
                               .first_digit_record_inputs[DASHBOARD_ART_RECORD_BASE_INDEX +
                                                          DASHBOARD_ART_RECORD_MODE_SPECIAL_INDEX];
      record->first_value  = DASHBOARD_SPECIAL_X_FIXED;
      record->second_value = DASHBOARD_MODE_ONE_THIRD_Y_FIXED;
      record->flags &= ~DASHBOARD_ACTIVE_RECORD_FLAG;
      (void) draw_current_bitmap_cels((CCB *) record);
    }
}
