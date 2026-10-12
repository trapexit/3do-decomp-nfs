#include "bitmap_clip.h"
#include "ccb_manager.h"
#include "driving_warning_display.h"
#include "frontend_render_state.h"
#include "game_frame_count.h"
#include "game_resource_cleanup.h"
#include "quad_render.h"
#include "road_data.h"
#include "runtime_rate_scalars.h"
#include "simulation_snapshot.h"
#include "vehicle_runtime_pool.h"

#define WARNING_ACTIVE_FLAG 0x40000000
#define WARNING_SCREEN_LEFT 0
#define WARNING_SCREEN_RIGHT 0x13F
#define WARNING_SCREEN_TOP 0
#define WARNING_SCREEN_BOTTOM 0xEF
#define WARNING_SCREEN_COLOR 1
#define WARNING_FRAME_COLOR 0xF
#define WARNING_OPAQUE_PIXC 0x1F001F00
#define WARNING_MASK_PIXC 0x1F811F81
#define WARNING_FRAME_PIXC 0x9BE19BE1
#define WARNING_TITLE_INDEX 7
#define WARNING_TITLE_X_FIXED 0x780000
#define WARNING_TITLE_Y_FIXED 0x730000
#define WARNING_STATUS_X_FIXED 0x8C0000
#define WARNING_STATUS_Y_FIXED 0x720000
#define WARNING_STATUS_STEP_FIXED 0x90000
#define WARNING_STATUS_SECOND_OFFSET 0x50000
#define WARNING_STATUS_DIGITS 2
#define WARNING_STATUS_FIRST_RATE_INDEX 3
#define WARNING_STATUS_SECOND_RATE_INDEX 2
#define WARNING_VARIANT_FIRST_INDEX 0
#define WARNING_VARIANT_SECOND_INDEX 1
#define WARNING_VARIANT_X_FIXED 0x1A0000
#define WARNING_VARIANT_Y_FIXED 0x150000
#define WARNING_MODE_INDEX_BASE 11
#define WARNING_MODE_LIMIT 7
#define WARNING_MODE_X_FIXED 0x140000
#define WARNING_MODE_Y_FIXED 0x5F0000
#define WARNING_SELECTED_X_FIXED 0x460000
#define WARNING_SELECTED_Y_FIXED 0x510000
#define WARNING_FRAME_LEFT 0x16
#define WARNING_FRAME_RIGHT 0x5A
#define WARNING_FRAME_TOP 0x49
#define WARNING_FRAME_BOTTOM 0x55
#define WARNING_METER_INPUT_BASE 0x7080
#define WARNING_METER_SCALE 50
#define WARNING_METER_X_BASE_FIXED 0x170000
#define WARNING_METER_Y_FIXED 0x560000
#define WARNING_METER_RECORD_INDEX 2
#define WARNING_FIXED16_SHIFT 16
#define WARNING_ALERT_FIRST_LEFT 0x5F
#define WARNING_ALERT_FIRST_RIGHT 0xE1
#define WARNING_ALERT_FIRST_TOP 0x6E
#define WARNING_ALERT_FIRST_BOTTOM 0x82
#define WARNING_ALERT_SECOND_LEFT 0x64
#define WARNING_ALERT_SECOND_RIGHT 0xDC
#define WARNING_ALERT_SECOND_TOP 0x71
#define WARNING_ALERT_SECOND_BOTTOM 0x7F

typedef struct DrivingWarningRuntimeState
{
  uint8                 reserved_000[8];
  uint8                 screen_mask_requested;
  uint8                 reserved_009[3];
  int32                 background_animation_frame;
  uint8                 reserved_010[8];
  int32                 dashboard_record_count;
  uint8                 reserved_01_c[0x1C];
  int32                 first_screen_mask_requested;
  uint8                 reserved_03_c[4];
  int32                 second_screen_mask_requested;
  uint8                 reserved_044[8];
  int32                 dashboard_variant;
  uint8                 reserved_050[0x24];
  int32                 status_interval;
  uint8                 reserved_078[8];
  int32                 selected_art_record_index;
  uint8                 reserved_084[0x0C];
  int32                 alert_animation_frame;
  uint8                 reserved_094[0x10];
  DrivingWarningRecord *art_records[21];
} DrivingWarningRuntimeState;

DrivingWarningMeterState g_DRIVING_WARNING_METER_STATE;
uint8                    g_DRIVING_WARNING_METER_RESERVED_STATE[8];

void
draw_dashboard_warning_screen_mask(void)
{
  RenderCcb *ccb;

  ccb = g_CCB_MANAGER_STATE.allocation_cursor;
  draw_solid_quad(WARNING_SCREEN_COLOR,
                  WARNING_SCREEN_LEFT,
                  WARNING_SCREEN_TOP,
                  WARNING_SCREEN_RIGHT,
                  WARNING_SCREEN_TOP,
                  WARNING_SCREEN_RIGHT,
                  WARNING_SCREEN_BOTTOM,
                  WARNING_SCREEN_LEFT,
                  WARNING_SCREEN_BOTTOM);
  ccb->fields.pixel_control = WARNING_MASK_PIXC;
  ccb->fields.flags |= WARNING_ACTIVE_FLAG;
  (void) draw_current_bitmap_cels((CCB *) ccb);
}

void
draw_driving_warning_screen_mask(void)
{
  RenderCcb *ccb;

  ccb = g_CCB_MANAGER_STATE.allocation_cursor;
  draw_solid_quad(WARNING_SCREEN_COLOR,
                  WARNING_SCREEN_LEFT,
                  WARNING_SCREEN_TOP,
                  WARNING_SCREEN_RIGHT,
                  WARNING_SCREEN_TOP,
                  WARNING_SCREEN_RIGHT,
                  WARNING_SCREEN_BOTTOM,
                  WARNING_SCREEN_LEFT,
                  WARNING_SCREEN_BOTTOM);
  ccb->fields.pixel_control = WARNING_MASK_PIXC;
  ccb->fields.flags |= WARNING_ACTIVE_FLAG;
  (void) draw_current_bitmap_cels((CCB *) ccb);
}

void
draw_driving_warning_alert_frame(void)
{
  RenderCcb *ccb;

  ccb = g_CCB_MANAGER_STATE.allocation_cursor;
  draw_solid_quad(WARNING_SCREEN_COLOR,
                  WARNING_SCREEN_LEFT,
                  WARNING_SCREEN_TOP,
                  WARNING_SCREEN_RIGHT,
                  WARNING_SCREEN_TOP,
                  WARNING_SCREEN_RIGHT,
                  WARNING_SCREEN_BOTTOM,
                  WARNING_SCREEN_LEFT,
                  WARNING_SCREEN_BOTTOM);
  ccb->fields.pixel_control = WARNING_MASK_PIXC;
  ccb->fields.flags |= WARNING_ACTIVE_FLAG;
  (void) draw_current_bitmap_cels((CCB *) ccb);
  g_CCB_MANAGER_STATE.allocation_cursor = ccb;

  draw_solid_quad(WARNING_FRAME_COLOR,
                  WARNING_ALERT_FIRST_LEFT,
                  WARNING_ALERT_FIRST_TOP,
                  WARNING_ALERT_FIRST_RIGHT,
                  WARNING_ALERT_FIRST_TOP,
                  WARNING_ALERT_FIRST_RIGHT,
                  WARNING_ALERT_FIRST_BOTTOM,
                  WARNING_ALERT_FIRST_LEFT,
                  WARNING_ALERT_FIRST_BOTTOM);
  ccb->fields.pixel_control = WARNING_FRAME_PIXC;
  ccb->fields.flags |= WARNING_ACTIVE_FLAG;
  (void) draw_current_bitmap_cels((CCB *) ccb);
  g_CCB_MANAGER_STATE.allocation_cursor = ccb;

  draw_solid_quad(WARNING_FRAME_COLOR,
                  WARNING_ALERT_SECOND_LEFT,
                  WARNING_ALERT_SECOND_TOP,
                  WARNING_ALERT_SECOND_RIGHT,
                  WARNING_ALERT_SECOND_TOP,
                  WARNING_ALERT_SECOND_RIGHT,
                  WARNING_ALERT_SECOND_BOTTOM,
                  WARNING_ALERT_SECOND_LEFT,
                  WARNING_ALERT_SECOND_BOTTOM);
  ccb->fields.pixel_control = WARNING_FRAME_PIXC;
  ccb->fields.flags |= WARNING_ACTIVE_FLAG;
  (void) draw_current_bitmap_cels((CCB *) ccb);
  g_CCB_MANAGER_STATE.allocation_cursor = ccb;
}

void
render_driving_warning_rate_values(int32 interval_)
{
  DrivingWarningRecord *last_record;
  int32                 digit_x;

  g_DRIVING_WARNING_DISPLAY_STATE.secondary_driving_warning_record_count = 0;
  digit_x                                                                = WARNING_STATUS_X_FIXED;
  configure_runtime_rate_scalars(interval_);
  enqueue_secondary_driving_warning_digits(
    g_DRIVING_WARNING_RATE_VALUES[WARNING_STATUS_FIRST_RATE_INDEX],
    &digit_x,
    WARNING_STATUS_Y_FIXED,
    WARNING_STATUS_STEP_FIXED,
    WARNING_STATUS_DIGITS);
  digit_x += WARNING_STATUS_SECOND_OFFSET;
  enqueue_secondary_driving_warning_digits(
    g_DRIVING_WARNING_RATE_VALUES[WARNING_STATUS_SECOND_RATE_INDEX],
    &digit_x,
    WARNING_STATUS_Y_FIXED,
    WARNING_STATUS_STEP_FIXED,
    WARNING_STATUS_DIGITS);

  last_record = &g_SECONDARY_DRIVING_WARNING_RECORDS
                  [g_DRIVING_WARNING_DISPLAY_STATE.secondary_driving_warning_record_count - 1];
  last_record->flags |= WARNING_ACTIVE_FLAG;
  (void) draw_current_bitmap_cels((CCB *) last_record);
}

void
render_active_driving_warning(void)
{
  DrivingWarningRuntimeState *state;
  DrivingWarningRecord       *record;
  RenderCcb                  *ccb;
  int32                       mode_index;
  int32                       meter_input;
  int32                       meter_position;

  state = (DrivingWarningRuntimeState *) &g_GAME_RESOURCE_STATE;
  if(state->screen_mask_requested != 0)
    draw_driving_warning_screen_mask();

  if((state->alert_animation_frame != 0) || (g_VEHICLE_RUNTIME_POOL_STATE.road_render_state != 0) ||
     (g_SECONDARY_ROAD_RENDER_CCB != NULL))
    {
      draw_driving_warning_alert_frame();
      record               = state->art_records[WARNING_TITLE_INDEX];
      record->first_value  = WARNING_TITLE_X_FIXED;
      record->second_value = WARNING_TITLE_Y_FIXED;
      (void) draw_current_bitmap_cels((CCB *) record);
      render_driving_warning_rate_values(state->status_interval);
      if(state->alert_animation_frame != 0)
        state->alert_animation_frame++;

      if(state->background_animation_frame <= 0)
        {
          ccb = g_CCB_MANAGER_STATE.allocation_cursor;
          draw_solid_quad(WARNING_SCREEN_COLOR,
                          WARNING_SCREEN_LEFT,
                          WARNING_SCREEN_TOP,
                          WARNING_SCREEN_RIGHT,
                          WARNING_SCREEN_TOP,
                          WARNING_SCREEN_RIGHT,
                          WARNING_SCREEN_BOTTOM,
                          WARNING_SCREEN_LEFT,
                          WARNING_SCREEN_BOTTOM);
          ccb->fields.pixel_control = WARNING_OPAQUE_PIXC;
          ccb->fields.flags |= WARNING_ACTIVE_FLAG;
          (void) draw_current_bitmap_cels((CCB *) ccb);
          g_CCB_MANAGER_STATE.allocation_cursor = ccb;
        }
    }

  if(state->dashboard_record_count > 1)
    {
      if(state->dashboard_variant < 0)
        record = state->art_records[WARNING_VARIANT_FIRST_INDEX];
      else
        record = state->art_records[WARNING_VARIANT_SECOND_INDEX];
      record->first_value  = WARNING_VARIANT_X_FIXED;
      record->second_value = WARNING_VARIANT_Y_FIXED;
      (void) draw_current_bitmap_cels((CCB *) record);

      mode_index = g_FRONTEND_RENDER_STATE.projection.source->dashboard_art_index;
      if(mode_index < WARNING_MODE_LIMIT)
        {
          record               = state->art_records[WARNING_MODE_INDEX_BASE + mode_index];
          record->first_value  = WARNING_MODE_X_FIXED;
          record->second_value = WARNING_MODE_Y_FIXED;
          (void) draw_current_bitmap_cels((CCB *) record);
        }

      record               = state->art_records[state->selected_art_record_index];
      record->first_value  = WARNING_SELECTED_X_FIXED;
      record->second_value = WARNING_SELECTED_Y_FIXED;
      (void) draw_current_bitmap_cels((CCB *) record);

      ccb = g_CCB_MANAGER_STATE.allocation_cursor;
      draw_solid_quad(0,
                      WARNING_FRAME_LEFT,
                      WARNING_FRAME_TOP,
                      WARNING_FRAME_RIGHT,
                      WARNING_FRAME_TOP,
                      WARNING_FRAME_RIGHT,
                      WARNING_FRAME_BOTTOM,
                      WARNING_FRAME_LEFT,
                      WARNING_FRAME_BOTTOM);
      ccb->fields.pixel_control = WARNING_MASK_PIXC;
      ccb->fields.flags |= WARNING_ACTIVE_FLAG;
      (void) draw_current_bitmap_cels((CCB *) ccb);
      g_CCB_MANAGER_STATE.allocation_cursor = ccb;

      meter_input = g_DRIVING_WARNING_METER_STATE.input_value - WARNING_METER_INPUT_BASE;
      if(meter_input < 0)
        meter_input = 0;
      meter_position =
        ((G_GAME_FRAME_COUNTER - meter_input) * WARNING_METER_SCALE) / WARNING_METER_INPUT_BASE;
      record               = state->art_records[WARNING_METER_RECORD_INDEX];
      record->first_value  = (meter_position << WARNING_FIXED16_SHIFT) + WARNING_METER_X_BASE_FIXED;
      record->second_value = WARNING_METER_Y_FIXED;
      (void) draw_current_bitmap_cels((CCB *) record);
    }

  if((state->first_screen_mask_requested != 0) || (state->second_screen_mask_requested != 0))
    {
      ccb = g_CCB_MANAGER_STATE.allocation_cursor;
      draw_solid_quad(WARNING_SCREEN_COLOR,
                      WARNING_SCREEN_LEFT,
                      WARNING_SCREEN_TOP,
                      WARNING_SCREEN_RIGHT,
                      WARNING_SCREEN_TOP,
                      WARNING_SCREEN_RIGHT,
                      WARNING_SCREEN_BOTTOM,
                      WARNING_SCREEN_LEFT,
                      WARNING_SCREEN_BOTTOM);
      ccb->fields.pixel_control = WARNING_OPAQUE_PIXC;
      ccb->fields.flags |= WARNING_ACTIVE_FLAG;
      (void) draw_current_bitmap_cels((CCB *) ccb);
      g_CCB_MANAGER_STATE.allocation_cursor = ccb;
    }
}
