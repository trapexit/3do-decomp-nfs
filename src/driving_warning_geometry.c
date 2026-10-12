#include "bitmap_clip.h"
#include "driving_warning_display.h"
#include "dynamic_object_activation.h"
#include "random_number.h"
#include "rotated_transform.h"
#include "simulation_snapshot.h"
#include "vehicle_dynamics_state.h"

#include "operamath.h"

#include "stddef.h"

#define DRIVING_WARNING_FIXED16_SHIFT 16
#define DRIVING_WARNING_GEOMETRY_SCALE_SHIFT 8
#define DRIVING_WARNING_ROTATION_SCALE 2
#define DRIVING_WARNING_TRANSFORM_RECORD_OFFSET 2
#define DRIVING_WARNING_ART_RECORD_OFFSET 3
#define DRIVING_WARNING_ART_POSITION_COUNT 17
#define DRIVING_WARNING_INTEGER_FIXED_MASK 0xFFFF0000
#define DRIVING_WARNING_DYNAMIC_SPEED_SCALE 0x23D70
#define DRIVING_WARNING_SPECIAL_DYNAMIC_MODE 1
#define DRIVING_WARNING_PREPARED_DIGIT_RECORD_INDEX 1
#define DRIVING_WARNING_SECONDARY_RECORD_OFFSET 1
#define DRIVING_WARNING_DYNAMIC_VALUE_LIMIT_OFFSET 0x5DC
#define DRIVING_WARNING_RECORD_ACTIVE_FLAG 0x40000000
#define DRIVING_WARNING_TRANSITION_SOURCE_INDEX_OFFSET 2
#define DRIVING_WARNING_TRANSITION_CURRENT_PHASE_LIMIT 12
#define DRIVING_WARNING_TRANSITION_CENTERED_PHASE 13
#define DRIVING_WARNING_TRANSITION_TARGET_PHASE 14
#define DRIVING_WARNING_TRANSITION_LINK_RECORD_OFFSET 4
#define DRIVING_WARNING_TRANSITION_ANCHOR_RECORD_OFFSET 7
#define DRIVING_WARNING_TRANSITION_FIRST_RECORD_OFFSET 8
#define DRIVING_WARNING_TRANSITION_REFERENCE_RECORD_OFFSET 11
#define DRIVING_WARNING_TRANSITION_CENTER_POINT_INDEX 1
#define DRIVING_WARNING_TRANSITION_INTERPOLATION_COUNT 4
#define DRIVING_WARNING_TRANSITION_CENTERING_DIVISOR 2
#define DRIVING_WARNING_SECONDARY_TEMPLATE_STATE_INDEX 12
#define DRIVING_WARNING_FIXED_PIXEL_SCALE 0x10000
#define DRIVING_WARNING_MOTION_POSITION_INDEX 16
#define DRIVING_WARNING_MOTION_INPUT_SHIFT 5
#define DRIVING_WARNING_RECORD_PREAMBLE_FLAG 0x00000001
#define DRIVING_WARNING_SHAKE_MOTION_THRESHOLD 0x10000
#define DRIVING_WARNING_VERTICAL_RISE_STEP 0xA0000
#define DRIVING_WARNING_VERTICAL_RETURN_STEP 0x30000
#define DRIVING_WARNING_VERTICAL_RECOVERY_STEP 0x40000
#define DRIVING_WARNING_SIZE_ASSERT(name, expression)                                              \
  typedef char driving_warning_size_assert_##name[(expression) ? 1 : -1]

typedef struct DrivingWarningArtPosition
{
  int32 record_count;
  int32 x_origin;
  int32 y_origin;
  int32 horizontal_factor;
  uint8 reserved_10[4];
  int32 vertical_factor;
  int32 secondary_vertical_factor;
} DrivingWarningArtPosition;

typedef struct DrivingWarningGeometryDefinition
{
  DrivingWarningArtPosition art_positions[DRIVING_WARNING_ART_POSITION_COUNT];
  Point2                    rotation_point;
  int32                     rotation_x_origin;
  int32                     rotation_y_origin;
  uint8                     reserved_1_ec[4];
  int32                     dynamic_base_x_factor;
  int32                     dynamic_base_y_factor;
  int32                     primary_dynamic_x_origin;
  int32                     primary_dynamic_y_origin;
  Point2                    primary_dynamic_rotation_point;
  int32                     primary_dynamic_rotation_base;
  int32                     primary_dynamic_rotation_factor;
  int32                     secondary_dynamic_x_origin;
  int32                     secondary_dynamic_y_origin;
  Point2                    secondary_dynamic_rotation_point;
  int32                     secondary_dynamic_rotation_base;
  int32                     secondary_dynamic_rotation_factor;
  int32                     horizontal_scale;
  uint8                     reserved_22_c[8];
  int32                     vertical_scale;
  int32                     rate_x_origin;
  int32                     rate_y_origin;
  int32                     rate_x_factor;
  int32                     rate_y_factor;
  int32                     digit_step;
  int32                     rate_x_offset;
  int32                     rate_y_offset;
  uint8                     reserved_254[0x44];
  int32                     backdrop_x_origin;
  int32                     backdrop_y_origin;
  uint8                     reserved_2_a0[0x10];
  int32                     backdrop_x_offset;
  int32                     backdrop_y_offset;
  int32                     transition_x_origin;
  int32                     transition_y_origin;
  Point2                    transition_points[9];
  int32                     transition_record_count;
  int32                     special_dynamic_mode;
} DrivingWarningGeometryDefinition;

typedef struct DrivingWarningDynamicLimit
{
  uint8 reserved_000[0xA8];
  int32 base_value_limit;
} DrivingWarningDynamicLimit;

typedef struct DrivingWarningDynamicSource
{
  uint8                             reserved_000[0x5C];
  int32                             primary_motion;
  uint8                             reserved_060[0x340];
  int32                             dashboard_horizontal_motion;
  int32                             dashboard_vertical_motion;
  uint8                             reserved_3_a8[0x50];
  int32                             transition_state;
  int32                             secondary_motion;
  uint8                             reserved_400[0x28];
  int32                             transition_position_index;
  uint8                             reserved_42_c[0x30];
  int32                             transition_target_index;
  int32                             transition_phase;
  uint8                             reserved_464[0x4C];
  const DrivingWarningDynamicLimit *dynamic_limit;
} DrivingWarningDynamicSource;

DRIVING_WARNING_SIZE_ASSERT(record_preamble_offset,
                            offsetof(DrivingWarningRecord, preamble) == 0x30);
DRIVING_WARNING_SIZE_ASSERT(display_dynamic_flags_offset,
                            offsetof(DrivingWarningDisplayState,
                                     dynamic_object_link_flags_requested) == 0xD8);
DRIVING_WARNING_SIZE_ASSERT(display_dashboard_offset,
                            offsetof(DrivingWarningDisplayState, dashboard_x_offset) == 0x100);
DRIVING_WARNING_SIZE_ASSERT(display_saved_preamble_offset,
                            offsetof(DrivingWarningDisplayState,
                                     saved_dynamic_object_link_preamble) == 0x124);
DRIVING_WARNING_SIZE_ASSERT(geometry_horizontal_scale_offset,
                            offsetof(DrivingWarningGeometryDefinition, horizontal_scale) == 0x228);
DRIVING_WARNING_SIZE_ASSERT(geometry_transition_origin_offset,
                            offsetof(DrivingWarningGeometryDefinition, transition_x_origin) ==
                              0x2B8);
DRIVING_WARNING_SIZE_ASSERT(geometry_special_mode_offset,
                            offsetof(DrivingWarningGeometryDefinition, special_dynamic_mode) ==
                              0x30C);
DRIVING_WARNING_SIZE_ASSERT(source_dashboard_motion_offset,
                            offsetof(DrivingWarningDynamicSource, dashboard_horizontal_motion) ==
                              0x3A0);
DRIVING_WARNING_SIZE_ASSERT(source_transition_state_offset,
                            offsetof(DrivingWarningDynamicSource, transition_state) == 0x3F8);
DRIVING_WARNING_SIZE_ASSERT(source_dynamic_limit_offset,
                            offsetof(DrivingWarningDynamicSource, dynamic_limit) == 0x4B0);

void
render_driving_warning_dashboard(const Vector3 *source_direction_)
{
  DrivingWarningGeometryDefinition  *definition;
  const DrivingWarningArtPosition   *motion_position;
  const DrivingWarningDynamicSource *source;
  DrivingWarningRecord              *last_record;
  DrivingWarningRecord              *record;
  int32                              base_record_index;
  int32                              maximum_record_index;
  int32                              rate_x;
  int32                              rate_y;
  int32                              random_offset;
  int32                              record_index;
  int32                              vertical_adjustment;
  int32                              vertical_target;
  int32                              vertical_return_target;

  source = (const DrivingWarningDynamicSource *) source_direction_;
  definition =
    (DrivingWarningGeometryDefinition *) g_DRIVING_WARNING_DISPLAY_STATE.warning_record_link_index;
  base_record_index = definition->art_positions[0].record_count;
  g_DRIVING_WARNING_DISPLAY_STATE.driving_warning_record_count           = 0;
  g_DRIVING_WARNING_DISPLAY_STATE.secondary_driving_warning_record_count = 0;
  g_DRIVING_WARNING_DISPLAY_STATE.prepared_warning_record_count          = 0;

  maximum_record_index = base_record_index + definition->transition_record_count +
                         DRIVING_WARNING_TRANSITION_ANCHOR_RECORD_OFFSET;
  if((g_DRIVING_WARNING_DISPLAY_STATE.dynamic_object_link_flags_requested != 0) &&
     (is_dynamic_object_activation_available() != 0))
    {
      if(g_DRIVING_WARNING_DISPLAY_STATE.dynamic_object_link_flags_enabled == 0)
        {
          for(record_index = 0; record_index <= maximum_record_index; ++record_index)
            {
              g_DRIVING_WARNING_RECORD_LINKS[record_index]->preamble |=
                DRIVING_WARNING_RECORD_PREAMBLE_FLAG;
            }
          g_DRIVING_WARNING_DISPLAY_STATE.dynamic_object_link_flags_enabled = 1;
        }
    }
  else if(g_DRIVING_WARNING_DISPLAY_STATE.dynamic_object_link_flags_enabled != 0)
    {
      for(record_index = 0; record_index <= maximum_record_index; ++record_index)
        {
          g_DRIVING_WARNING_RECORD_LINKS[record_index]->preamble &=
            ~DRIVING_WARNING_RECORD_PREAMBLE_FLAG;
        }
      g_DRIVING_WARNING_DISPLAY_STATE.dynamic_object_link_flags_enabled = 0;
      g_DRIVING_WARNING_RECORD_LINKS[base_record_index + DRIVING_WARNING_ART_RECORD_OFFSET]
        ->preamble = g_DRIVING_WARNING_DISPLAY_STATE.saved_dynamic_object_link_preamble;
    }

  motion_position = &definition->art_positions[DRIVING_WARNING_MOTION_POSITION_INDEX];
  definition->horizontal_scale =
    ((motion_position->horizontal_factor >> DRIVING_WARNING_GEOMETRY_SCALE_SHIFT) *
     -(source->dashboard_horizontal_motion >> DRIVING_WARNING_MOTION_INPUT_SHIFT)) >>
    DRIVING_WARNING_GEOMETRY_SCALE_SHIFT;
  definition->vertical_scale =
    ((motion_position->secondary_vertical_factor >> DRIVING_WARNING_GEOMETRY_SCALE_SHIFT) *
     -(source->dashboard_vertical_motion >> DRIVING_WARNING_MOTION_INPUT_SHIFT)) >>
    DRIVING_WARNING_GEOMETRY_SCALE_SHIFT;
  g_DRIVING_WARNING_DISPLAY_STATE.dashboard_x_offset = motion_position->x_origin;
  g_DRIVING_WARNING_DISPLAY_STATE.dashboard_y_offset = motion_position->y_origin;

  if((g_DRIVING_WARNING_DISPLAY_STATE.dashboard_shake_range != 0) &&
     (source->primary_motion > DRIVING_WARNING_SHAKE_MOTION_THRESHOLD))
    {
      random_offset =
        scale_by_random_fraction(g_DRIVING_WARNING_DISPLAY_STATE.dashboard_shake_range) -
        (g_DRIVING_WARNING_DISPLAY_STATE.dashboard_shake_range /
         DRIVING_WARNING_TRANSITION_CENTERING_DIVISOR);
      g_DRIVING_WARNING_DISPLAY_STATE.dashboard_x_offset +=
        random_offset * DRIVING_WARNING_FIXED_PIXEL_SCALE;
      random_offset =
        scale_by_random_fraction(g_DRIVING_WARNING_DISPLAY_STATE.dashboard_shake_range) -
        (g_DRIVING_WARNING_DISPLAY_STATE.dashboard_shake_range /
         DRIVING_WARNING_TRANSITION_CENTERING_DIVISOR);
      g_DRIVING_WARNING_DISPLAY_STATE.dashboard_y_offset +=
        random_offset * DRIVING_WARNING_FIXED_PIXEL_SCALE;
    }

  vertical_adjustment = g_DRIVING_WARNING_DISPLAY_STATE.dashboard_vertical_adjustment;
  vertical_target     = g_DRIVING_WARNING_DISPLAY_STATE.dashboard_vertical_target;
  if((vertical_adjustment < vertical_target) && (vertical_target != 0))
    {
      g_DRIVING_WARNING_DISPLAY_STATE.dashboard_y_offset -= vertical_adjustment;
      vertical_adjustment += DRIVING_WARNING_VERTICAL_RISE_STEP;
      g_DRIVING_WARNING_DISPLAY_STATE.dashboard_vertical_adjustment = vertical_adjustment;
      if(vertical_adjustment >= vertical_target)
        {
          g_DRIVING_WARNING_DISPLAY_STATE.dashboard_vertical_target =
            g_DRIVING_WARNING_DISPLAY_STATE.dashboard_vertical_return_target;
        }
    }
  else
    {
      vertical_return_target = g_DRIVING_WARNING_DISPLAY_STATE.dashboard_vertical_return_target;
      if(vertical_adjustment > vertical_return_target)
        {
          g_DRIVING_WARNING_DISPLAY_STATE.dashboard_y_offset -= vertical_adjustment;
          vertical_adjustment -= DRIVING_WARNING_VERTICAL_RETURN_STEP;
          g_DRIVING_WARNING_DISPLAY_STATE.dashboard_vertical_adjustment = vertical_adjustment;
          if(vertical_adjustment <= vertical_return_target)
            {
              g_DRIVING_WARNING_DISPLAY_STATE.dashboard_vertical_target        = 0;
              g_DRIVING_WARNING_DISPLAY_STATE.dashboard_vertical_return_target = 0;
            }
        }
      else if(vertical_adjustment < vertical_return_target)
        {
          g_DRIVING_WARNING_DISPLAY_STATE.dashboard_y_offset -= vertical_adjustment;
          g_DRIVING_WARNING_DISPLAY_STATE.dashboard_vertical_adjustment =
            vertical_adjustment + DRIVING_WARNING_VERTICAL_RECOVERY_STEP;
        }
    }

  draw_driving_warning_dashboard_backdrop();
  position_driving_warning_art_records();
  configure_driving_warning_dynamic_art(source_direction_);
  configure_driving_warning_rotated_art(source_direction_);
  configure_driving_warning_transition_art(source_direction_);
  rate_x = (definition->rate_x_factor * definition->horizontal_scale) + definition->rate_x_origin;
  rate_y = (definition->rate_y_factor * definition->vertical_scale) + definition->rate_y_origin;
  prepare_driving_warning_rate_records(source_direction_, rate_x, rate_y);

  last_record =
    &g_DRIVING_WARNING_RECORDS[g_DRIVING_WARNING_DISPLAY_STATE.driving_warning_record_count - 1];
  last_record->flags |= DRIVING_WARNING_RECORD_ACTIVE_FLAG;
  record = g_DRIVING_WARNING_RECORD_LINKS[0];
  (void) draw_current_bitmap_cels((CCB *) record);
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
configure_driving_warning_dynamic_art(const Vector3 *source_direction_)
{
  const DrivingWarningGeometryDefinition *definition;
  const DrivingWarningDynamicSource      *source;
  DrivingWarningRecord                   *record;
  RotatedTransform2D                     *transform;
  int32                                   base_x;
  int32                                   base_y;
  int32                                   primary_motion;
  int32                                   secondary_motion;
  int32                                   secondary_motion_limit;
  int32                                   rotation;
  int32                                   record_index;

  source       = (const DrivingWarningDynamicSource *) source_direction_;
  definition   = (const DrivingWarningGeometryDefinition *)
                   g_DRIVING_WARNING_DISPLAY_STATE.warning_record_link_index;
  base_x       = ((definition->dynamic_base_x_factor * definition->horizontal_scale) +
                  g_DRIVING_WARNING_DISPLAY_STATE.dashboard_x_offset) &
                 DRIVING_WARNING_INTEGER_FIXED_MASK;
  base_y       = ((definition->dynamic_base_y_factor * definition->vertical_scale) +
                  g_DRIVING_WARNING_DISPLAY_STATE.dashboard_y_offset) &
                 DRIVING_WARNING_INTEGER_FIXED_MASK;
  record_index = definition->art_positions[0].record_count;

  primary_motion = source->primary_motion;
  if(primary_motion < 0)
    primary_motion = -primary_motion;
  if(definition->special_dynamic_mode == DRIVING_WARNING_SPECIAL_DYNAMIC_MODE)
    {
      record = g_DRIVING_WARNING_RECORD_LINKS[record_index - 1];
      record->next =
        &g_PREPARED_DRIVING_WARNING_RECORDS[DRIVING_WARNING_PREPARED_DIGIT_RECORD_INDEX];
      primary_motion = MulSF16(primary_motion, DRIVING_WARNING_DYNAMIC_SPEED_SCALE);
      prepare_driving_warning_digits(primary_motion >> DRIVING_WARNING_FIXED16_SHIFT,
                                     definition->primary_dynamic_x_origin + base_x,
                                     definition->primary_dynamic_y_origin + base_y,
                                     definition->primary_dynamic_rotation_factor);
    }
  else
    {
      rotation =
        ((definition->primary_dynamic_rotation_factor >> DRIVING_WARNING_GEOMETRY_SCALE_SHIFT) *
         (primary_motion >> DRIVING_WARNING_GEOMETRY_SCALE_SHIFT)) +
        definition->primary_dynamic_rotation_base;
      transform = (RotatedTransform2D *) g_DRIVING_WARNING_RECORD_LINKS[record_index];
      configure_rotated_2d_transform(&definition->primary_dynamic_rotation_point,
                                     rotation,
                                     definition->primary_dynamic_x_origin + base_x,
                                     definition->primary_dynamic_y_origin + base_y,
                                     transform);
    }

  secondary_motion = source->secondary_motion;
  secondary_motion_limit =
    source->dynamic_limit->base_value_limit + DRIVING_WARNING_DYNAMIC_VALUE_LIMIT_OFFSET;
  if(secondary_motion > secondary_motion_limit)
    secondary_motion = secondary_motion_limit;
  rotation  = (definition->secondary_dynamic_rotation_factor * secondary_motion) +
              definition->secondary_dynamic_rotation_base;
  transform = (RotatedTransform2D *)
    g_DRIVING_WARNING_RECORD_LINKS[record_index + DRIVING_WARNING_SECONDARY_RECORD_OFFSET];
  configure_rotated_2d_transform(&definition->secondary_dynamic_rotation_point,
                                 rotation,
                                 definition->secondary_dynamic_x_origin + base_x,
                                 definition->secondary_dynamic_y_origin + base_y,
                                 transform);
}

void
configure_driving_warning_transition_art(const Vector3 *source_direction_)
{
  const DrivingWarningGeometryDefinition *definition;
  const DrivingWarningDynamicSource      *source;
  const Point2                           *center;
  const Point2                           *current;
  const Point2                           *target;
  DrivingWarningRecord                   *anchor_record;
  DrivingWarningRecord                   *first_record;
  DrivingWarningRecord                   *record;
  DrivingWarningRecord                   *reference_record;
  int32                                   selected_x;
  int32                                   selected_y;
  int32                                   x_step;
  int32                                   y_step;
  int32                                   x_offset;
  int32                                   y_offset;
  int32                                   base_record_index;
  int32                                   record_index;

  source            = (const DrivingWarningDynamicSource *) source_direction_;
  definition        = (const DrivingWarningGeometryDefinition *)
                        g_DRIVING_WARNING_DISPLAY_STATE.warning_record_link_index;
  base_record_index = definition->art_positions[0].record_count;
  first_record      = g_DRIVING_WARNING_RECORD_LINKS[base_record_index +
                                                     DRIVING_WARNING_TRANSITION_LINK_RECORD_OFFSET];
  if((g_DRIVING_WARNING_DISPLAY_STATE
        .second_digit_record_inputs[DRIVING_WARNING_SECONDARY_TEMPLATE_STATE_INDEX] == NULL) &&
     (source->transition_phase <= 0))
    {
      first_record->next = g_DRIVING_WARNING_RECORDS;
      return;
    }

  anchor_record = g_DRIVING_WARNING_RECORD_LINKS[base_record_index +
                                                 DRIVING_WARNING_TRANSITION_ANCHOR_RECORD_OFFSET];
  anchor_record->first_value  = definition->transition_x_origin;
  anchor_record->second_value = definition->transition_y_origin;
  first_record->next          = anchor_record;
  first_record->flags &= ~DRIVING_WARNING_RECORD_ACTIVE_FLAG;

  center     = &definition->transition_points[DRIVING_WARNING_TRANSITION_CENTER_POINT_INDEX];
  selected_x = 0;
  selected_y = 0;
  if((source->transition_phase < DRIVING_WARNING_TRANSITION_CURRENT_PHASE_LIMIT) &&
     (source->transition_state < 1))
    {
      current    = &definition->transition_points[source->transition_position_index +
                                                  DRIVING_WARNING_TRANSITION_SOURCE_INDEX_OFFSET];
      selected_x = current->x;
      selected_y = current->y;
    }
  else if(source->transition_phase >= DRIVING_WARNING_TRANSITION_TARGET_PHASE)
    {
      target     = &definition->transition_points[source->transition_target_index +
                                                  DRIVING_WARNING_TRANSITION_SOURCE_INDEX_OFFSET];
      selected_x = target->x;
      selected_y = target->y;
    }
  else if(source->transition_phase == DRIVING_WARNING_TRANSITION_CENTERED_PHASE)
    {
      target     = &definition->transition_points[source->transition_target_index +
                                                  DRIVING_WARNING_TRANSITION_SOURCE_INDEX_OFFSET];
      selected_x = target->x;
      selected_y = center->y;
    }
  else if(source->transition_phase == DRIVING_WARNING_TRANSITION_CURRENT_PHASE_LIMIT)
    {
      current    = &definition->transition_points[source->transition_position_index +
                                                  DRIVING_WARNING_TRANSITION_SOURCE_INDEX_OFFSET];
      selected_x = current->x;
      selected_y = center->y;
    }

  record = g_DRIVING_WARNING_RECORD_LINKS[base_record_index +
                                          DRIVING_WARNING_TRANSITION_FIRST_RECORD_OFFSET];
  if(definition->transition_record_count != DRIVING_WARNING_TRANSITION_INTERPOLATION_COUNT)
    {
      record->first_value  = definition->transition_x_origin + selected_x;
      record->second_value = definition->transition_y_origin + selected_y;
      record->next         = g_DRIVING_WARNING_RECORDS;
      return;
    }

  x_step   = (selected_x - center->x) / DRIVING_WARNING_TRANSITION_INTERPOLATION_COUNT;
  y_step   = (selected_y - center->y) / DRIVING_WARNING_TRANSITION_INTERPOLATION_COUNT;
  x_offset = x_step;
  y_offset = y_step;
  reference_record =
    g_DRIVING_WARNING_RECORD_LINKS[base_record_index +
                                   DRIVING_WARNING_TRANSITION_REFERENCE_RECORD_OFFSET];
  for(record_index = 0; record_index < definition->transition_record_count; ++record_index)
    {
      record = g_DRIVING_WARNING_RECORD_LINKS[base_record_index +
                                              DRIVING_WARNING_TRANSITION_FIRST_RECORD_OFFSET +
                                              record_index];
      record->first_value  = definition->transition_x_origin + x_offset + center->x -
                             (((record->width - reference_record->width) /
                               DRIVING_WARNING_TRANSITION_CENTERING_DIVISOR) *
                              DRIVING_WARNING_FIXED_PIXEL_SCALE);
      record->second_value = definition->transition_y_origin + y_offset + center->y -
                             (((record->height - reference_record->height) /
                               DRIVING_WARNING_TRANSITION_CENTERING_DIVISOR) *
                              DRIVING_WARNING_FIXED_PIXEL_SCALE);
      x_offset += x_step;
      y_offset += y_step;
    }
  record = g_DRIVING_WARNING_RECORD_LINKS[base_record_index + definition->transition_record_count +
                                          DRIVING_WARNING_TRANSITION_ANCHOR_RECORD_OFFSET];
  record->next = g_DRIVING_WARNING_RECORDS;
}

void
draw_driving_warning_dashboard_backdrop(void)
{
  const DrivingWarningGeometryDefinition *definition;
  int32                                   left;
  int32                                   top;

  definition = (const DrivingWarningGeometryDefinition *)
                 g_DRIVING_WARNING_DISPLAY_STATE.warning_record_link_index;
  left = ((definition->horizontal_scale * definition->backdrop_x_offset) +
          definition->backdrop_x_origin + g_DRIVING_WARNING_DISPLAY_STATE.dashboard_x_offset) >>
         DRIVING_WARNING_FIXED16_SHIFT;
  top  = ((definition->vertical_scale * definition->backdrop_y_offset) +
          definition->backdrop_y_origin + g_DRIVING_WARNING_DISPLAY_STATE.dashboard_y_offset) >>
         DRIVING_WARNING_FIXED16_SHIFT;
  draw_driving_warning_backdrop(left, top);
}

void
configure_driving_warning_rotated_art(const Vector3 *source_direction_)
{
  const DrivingWarningGeometryDefinition *definition;
  const SimulationSource                 *source;
  DrivingWarningRecord                   *record;
  RotatedTransform2D                     *transform;
  int32                                   base_x;
  int32                                   base_y;

  source     = (const SimulationSource *) ((const uint8 *) source_direction_ -
                                           offsetof(SimulationSource, direction));
  definition = (const DrivingWarningGeometryDefinition *)
                 g_DRIVING_WARNING_DISPLAY_STATE.warning_record_link_index;
  base_x     = g_DRIVING_WARNING_DISPLAY_STATE.dashboard_x_offset + definition->rotation_x_origin +
               (definition->horizontal_scale << DRIVING_WARNING_GEOMETRY_SCALE_SHIFT);
  base_y     = g_DRIVING_WARNING_DISPLAY_STATE.dashboard_y_offset + definition->rotation_y_origin +
               (definition->vertical_scale << DRIVING_WARNING_GEOMETRY_SCALE_SHIFT);
  transform  = (RotatedTransform2D *)
    g_DRIVING_WARNING_RECORD_LINKS[definition->art_positions[0].record_count +
                                   DRIVING_WARNING_TRANSFORM_RECORD_OFFSET];
  configure_rotated_2d_transform(&definition->rotation_point,
                                 source->alternate_surface_projection_origin.y *
                                   DRIVING_WARNING_ROTATION_SCALE,
                                 base_x,
                                 base_y,
                                 transform);

  record               = g_DRIVING_WARNING_RECORD_LINKS[definition->art_positions[0].record_count +
                                                        DRIVING_WARNING_ART_RECORD_OFFSET];
  record->first_value  = base_x + definition->rotation_point.x;
  record->second_value = base_y + definition->rotation_point.y;
}

void
position_driving_warning_art_records(void)
{
  const DrivingWarningGeometryDefinition *definition;
  const DrivingWarningArtPosition        *position;
  DrivingWarningRecord                   *record;
  int32                                   record_count;
  int32                                   record_index;

  definition   = (const DrivingWarningGeometryDefinition *)
                   g_DRIVING_WARNING_DISPLAY_STATE.warning_record_link_index;
  record_count = definition->art_positions[0].record_count;
  for(record_index = 0; record_index < record_count; ++record_index)
    {
      position            = &definition->art_positions[record_index];
      record              = g_DRIVING_WARNING_RECORD_LINKS[record_index];
      record->first_value = (position->horizontal_factor * definition->horizontal_scale) +
                            position->x_origin + g_DRIVING_WARNING_DISPLAY_STATE.dashboard_x_offset;
      record->second_value = (definition->vertical_scale * position->vertical_factor) +
                             position->y_origin +
                             g_DRIVING_WARNING_DISPLAY_STATE.dashboard_y_offset;
      record->first_value &= DRIVING_WARNING_INTEGER_FIXED_MASK;
      record->second_value &= DRIVING_WARNING_INTEGER_FIXED_MASK;
    }
}
