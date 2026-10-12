/* Computes vehicle orientation inputs for road-animation rendering. */

#include "ccb_manager.h"
#include "cel_render.h"
#include "fixed_math.h"
#include "frontend_render_state.h"
#include "game_frame_count.h"
#include "player_vehicle_state.h"
#include "radix_sort.h"
#include "random_number.h"
#include "render_bounds.h"
#include "simulation_common_art.h"
#include "simulation_resource_cleanup.h"
#include "simulation_snapshot.h"
#include "simulation_thread_signal.h"
#include "tire_smoke.h"
#include "track_position.h"
#include "vehicle_render_resource_selection.h"
#include "vehicle_runtime_pool.h"
#include "vehicle_selection_state.h"

#include "operamath.h"

#define VEHICLE_ROAD_RENDER_ANGLE_HALF_TURN 0x800000
#define VEHICLE_ROAD_RENDER_ANGLE_FULL_TURN 0x1000000
#define VEHICLE_ROAD_RENDER_ANGLE_MASK 0x00FFFFFF
#define VEHICLE_ROAD_RENDER_SHAPE_DETAIL_INDEX (RENDER_SHAPE_POINTER_COUNT - 1)
#define VEHICLE_ROAD_RENDER_SURFACE_DETAIL_LIMIT 10
#define VEHICLE_ROAD_RENDER_TRACK_RANGE 0x2A
#define VEHICLE_ROAD_RENDER_NEAR_DEPTH_LIMIT 10
#define VEHICLE_ROAD_RENDER_REVERSE_DEPTH_LIMIT 0x180
#define VEHICLE_ROAD_RENDER_HIGH_HEADING_LIMIT 0x1E0000
#define VEHICLE_ROAD_RENDER_WHEEL_TURN_MIN 0x0A0000
#define VEHICLE_ROAD_RENDER_WHEEL_TURN_MAX 0x760000
#define VEHICLE_ROAD_RENDER_DETAIL_MINIMUM 2
#define VEHICLE_ROAD_RENDER_DETAIL_HIGHEST 4
#define VEHICLE_ROAD_RENDER_ROTATION_SHIFT 14
#define VEHICLE_ROAD_RENDER_POSITION_SHIFT 8
#define VEHICLE_ROAD_RENDER_HEADING_INTEGER_SHIFT 16
#define VEHICLE_ROAD_RENDER_FIXED16_SHIFT 16
#define VEHICLE_ROAD_RENDER_FIXED16_ONE 0x10000
#define VEHICLE_ROAD_RENDER_AXLE_DIVISOR 2
#define VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT 3
#define VEHICLE_ROAD_RENDER_VERTEX_DEPTH_COMPONENT 2
#define VEHICLE_ROAD_RENDER_REVERSE_DISTANCE_DIVISOR 4
#define VEHICLE_ROAD_RENDER_HEADING_FORCE_DETAIL_MIN 0x0F0000
#define VEHICLE_ROAD_RENDER_HEADING_FORCE_DETAIL_MAX 0x710000
#define VEHICLE_ROAD_RENDER_POLYGON_VERTEX_COUNT 4
#define VEHICLE_ROAD_RENDER_POLYGON_KEY_SHIFT 16
#define VEHICLE_ROAD_RENDER_SORT_REFRESH_PERIOD 14
#define VEHICLE_ROAD_RENDER_PRIMARY_DEPTH_LIMIT 150
#define VEHICLE_ROAD_RENDER_POLYGON_DEPTH_THRESHOLD 70
#define VEHICLE_ROAD_RENDER_SPECIAL_CCB_PIXEL_CONTROL 0x17001700
#define VEHICLE_ROAD_RENDER_SPECIAL_TRACK_TYPE_A 4
#define VEHICLE_ROAD_RENDER_SPECIAL_TRACK_TYPE_B 7
#define VEHICLE_ROAD_RENDER_SPECIAL_TRACK_TYPE_C 9
#define VEHICLE_ROAD_RENDER_OPTIONAL_BATCH_INDEX 2
#define VEHICLE_ROAD_RENDER_SPECIAL_STATE_INDEX 2
#define VEHICLE_ROAD_RENDER_ANIMATED_SHAPE_FLAG_MASK 0xC0000
#define VEHICLE_ROAD_RENDER_SPECIAL_FRAME_COUNTER_LIMIT 2
#define VEHICLE_ROAD_RENDER_FRONTEND_EXIT_COMPLETE 4
#define VEHICLE_ROAD_RENDER_SPECIAL_MINIMUM_SHAPE_COUNT 2
#define VEHICLE_ROAD_RENDER_SPECIAL_SHAPE_INDEX_STEP 2
#define VEHICLE_ROAD_RENDER_SPECIAL_STANDARD_SHAPE_COUNT 4
#define VEHICLE_ROAD_RENDER_ANIMATED_SHAPE_STRIDE 12
#define VEHICLE_ROAD_RENDER_ANIMATED_SHAPE_FRAME_STRIDE 3
#define VEHICLE_ROAD_RENDER_ANIMATED_SHAPE_INDEX_STRIDE 4
#define VEHICLE_ROAD_RENDER_HIGH_SHAPE_MAX_MOTION 0x1E0000
#define VEHICLE_ROAD_RENDER_HIGH_SHAPE_MIN_MOTION 0x199900
#define VEHICLE_ROAD_RENDER_HIGH_SHAPE_PHASE_MOTION 0x0A0000
#define VEHICLE_ROAD_RENDER_HIGH_SHAPE_DYNAMIC_FLAG 2
#define VEHICLE_ROAD_RENDER_HIGH_SHAPE_VARIANT_FINAL 3
#define VEHICLE_ROAD_RENDER_HIGH_SHAPE_PRIMARY_FIRST 0
#define VEHICLE_ROAD_RENDER_HIGH_SHAPE_PRIMARY_SECOND 1
#define VEHICLE_ROAD_RENDER_HIGH_SHAPE_SECONDARY_FIRST 2
#define VEHICLE_ROAD_RENDER_HIGH_SHAPE_SECONDARY_SECOND 3
#define VEHICLE_ROAD_RENDER_SPRITE_DETAIL_LEVEL 1
#define VEHICLE_ROAD_RENDER_SPRITE_STRAIGHT_HEADING 0x400000
#define VEHICLE_ROAD_RENDER_SPRITE_DIRECTION_SHIFT 7
#define VEHICLE_ROAD_RENDER_SPRITE_DEFAULT_SHIFT 8
#define VEHICLE_ROAD_RENDER_SPRITE_SCALE_SHIFT 8
#define VEHICLE_ROAD_RENDER_SPRITE_HORIZONTAL_SHIFT 9
#define VEHICLE_ROAD_RENDER_SPRITE_VERTICAL_SHIFT 8
#define VEHICLE_ROAD_RENDER_SPRITE_HEADER_HIGH_SHIFT 4
#define VEHICLE_ROAD_RENDER_SPRITE_HEADER_LOW_SHIFT 20
#define VEHICLE_ROAD_RENDER_SPRITE_HEADER_SIGN_SHIFT 20
#define VEHICLE_ROAD_RENDER_SPRITE_VARIANT_FORWARD 1
#define VEHICLE_ROAD_RENDER_SPRITE_VARIANT_DEFAULT 2
#define VEHICLE_ROAD_RENDER_SPRITE_VARIANT_FIRST 0
#define VEHICLE_ROAD_RENDER_SPRITE_HIGH_VARIANT_LAST 4
#define VEHICLE_ROAD_RENDER_SPRITE_NEGATIVE_HORIZONTAL_BIAS 0x1FF
#define VEHICLE_ROAD_RENDER_SPRITE_NEGATIVE_VERTICAL_BIAS 0xFF
#define VEHICLE_ROAD_RENDER_SPRITE_ROTATION_COMPONENT_SHIFT 8
#define VEHICLE_ROAD_RENDER_SPRITE_ROTATION_CCB_SHIFT 4
#define VEHICLE_ROAD_RENDER_EFFECT_CEL_COUNT 3
#define VEHICLE_ROAD_RENDER_EFFECT_POSITION_SHIFT 16
#define VEHICLE_ROAD_RENDER_EFFECT_SCALE_SHIFT 8
#define VEHICLE_ROAD_RENDER_EFFECT_CENTER_SHIFT 9
#define VEHICLE_ROAD_RENDER_EFFECT_NEGATIVE_SCALE_BIAS 0xFF
#define VEHICLE_ROAD_RENDER_EFFECT_NEGATIVE_CENTER_BIAS 0x1FF
#define VEHICLE_ROAD_RENDER_EFFECT_PIXEL_CONTROL 0x1F801F80
#define VEHICLE_ROAD_RENDER_EFFECT_PROJECTION_DIVISOR 0x38400
#define VEHICLE_ROAD_RENDER_EFFECT_HALF_SCALE 0x8000
#define VEHICLE_ROAD_RENDER_EFFECT_RANDOM_RANGE 4
#define VEHICLE_ROAD_RENDER_EFFECT_RANDOM_BIAS 2
#define VEHICLE_ROAD_RENDER_EFFECT_INTENSITY_SHIFT 18
#define VEHICLE_ROAD_RENDER_EFFECT_NEGATIVE_INTENSITY_BIAS 0x3FFFF
#define VEHICLE_ROAD_RENDER_EFFECT_MAX_INTENSITY 0x180
#define VEHICLE_ROAD_RENDER_EFFECT_HEADING_MINIMUM 0xC0000
#define VEHICLE_ROAD_RENDER_EFFECT_HEADING_MAXIMUM 0x740000
#define VEHICLE_ROAD_RENDER_EFFECT_FIXED_MIDPOINT_SHIFT 17
#define VEHICLE_ROAD_RENDER_PALETTE_REFERENCE_FLAG_MASK 0xFF000000
#define VEHICLE_ROAD_RENDER_PALETTE_REFERENCE_OFFSET_MASK 0x00FFFFFF
#define VEHICLE_ROAD_RENDER_SPECIAL_PALETTE_VARIANT_INDEX 4
#define VEHICLE_ROAD_RENDER_REFERENCE_HEADING_HIGH_INDEX 4
#define VEHICLE_ROAD_RENDER_REFERENCE_HEADING_LOW_INDEX 5
#define VEHICLE_ROAD_RENDER_REFERENCE_HEADING_BYTE_SHIFT 8
#define VEHICLE_ROAD_RENDER_REFERENCE_HEADING_SHIFT 10

#define VEHICLE_ROAD_EFFECT_IMPULSE_FRONT_FIRST_VERTEX_INDEX 1
#define VEHICLE_ROAD_EFFECT_IMPULSE_FRONT_SECOND_VERTEX_INDEX 3
#define VEHICLE_ROAD_EFFECT_IMPULSE_REAR_FIRST_VERTEX_INDEX 20
#define VEHICLE_ROAD_EFFECT_IMPULSE_REAR_SECOND_VERTEX_INDEX 22
#define VEHICLE_ROAD_EFFECT_TYPE_FRONT_FIRST_VERTEX_INDEX 39
#define VEHICLE_ROAD_EFFECT_TYPE_FRONT_SECOND_VERTEX_INDEX 41
#define VEHICLE_ROAD_EFFECT_TYPE_REAR_FIRST_VERTEX_INDEX 58
#define VEHICLE_ROAD_EFFECT_TYPE_REAR_SECOND_VERTEX_INDEX 60
#define VEHICLE_ROAD_RENDER_TIRE_SMOKE_VERTEX_INDEX_COUNT 3
#define VEHICLE_ROAD_RENDER_TIRE_SMOKE_STATUS_FLAG 0x00000200

#define VEHICLE_ROAD_WHEEL_FRONT_LEFT_INDEX 0
#define VEHICLE_ROAD_WHEEL_FRONT_RIGHT_INDEX 1
#define VEHICLE_ROAD_WHEEL_FRONT_LEFT_REAR_INDEX 2
#define VEHICLE_ROAD_WHEEL_FRONT_RIGHT_REAR_INDEX 3
#define VEHICLE_ROAD_WHEEL_FRONT_LEFT_X_BASE 4
#define VEHICLE_ROAD_WHEEL_FRONT_LEFT_Z_BASE 6
#define VEHICLE_ROAD_WHEEL_FRONT_RIGHT_X_BASE 7
#define VEHICLE_ROAD_WHEEL_FRONT_RIGHT_Z_BASE 9
#define VEHICLE_ROAD_WHEEL_FRONT_LEFT_REAR_X_BASE 10
#define VEHICLE_ROAD_WHEEL_FRONT_LEFT_REAR_Z_BASE 12
#define VEHICLE_ROAD_WHEEL_FRONT_RIGHT_REAR_X_BASE 13
#define VEHICLE_ROAD_WHEEL_FRONT_RIGHT_REAR_Z_BASE 15
#define VEHICLE_ROAD_WHEEL_REAR_LEFT_INDEX 19
#define VEHICLE_ROAD_WHEEL_REAR_RIGHT_INDEX 20
#define VEHICLE_ROAD_WHEEL_REAR_LEFT_REAR_INDEX 21
#define VEHICLE_ROAD_WHEEL_REAR_RIGHT_REAR_INDEX 22
#define VEHICLE_ROAD_WHEEL_REAR_LEFT_X_BASE 23
#define VEHICLE_ROAD_WHEEL_REAR_LEFT_Z_BASE 25
#define VEHICLE_ROAD_WHEEL_REAR_RIGHT_X_BASE 26
#define VEHICLE_ROAD_WHEEL_REAR_RIGHT_Z_BASE 28
#define VEHICLE_ROAD_WHEEL_REAR_LEFT_REAR_X_BASE 29
#define VEHICLE_ROAD_WHEEL_REAR_LEFT_REAR_Z_BASE 31
#define VEHICLE_ROAD_WHEEL_REAR_RIGHT_REAR_X_BASE 32
#define VEHICLE_ROAD_WHEEL_REAR_RIGHT_REAR_Z_BASE 34

typedef RenderResourcePolygonBatch VehicleRoadPolygonBatch;

typedef RenderResourceRecord VehicleRoadRenderResourceState;

typedef struct VehicleRoadEffectGeometry
{
  int32 front_first_vertex_index;
  int32 front_second_vertex_index;
  int32 rear_first_vertex_index;
  int32 rear_second_vertex_index;
  int32 uses_fixed_midpoint;
} VehicleRoadEffectGeometry;

VehicleRoadWheelShapeState g_VEHICLE_ROAD_WHEEL_SHAPE_STATE;
uint8                      g_VEHICLE_ROAD_RENDER_RESERVED_STATE[0x320];

static int32
_truncate_vehicle_road_effect_value(int32 value_, int32 shift_, int32 negative_bias_);
static int32
_calculate_vehicle_road_effect_center_offset(int32 scale_, int32 dimension_log2_);
static int32
_calculate_vehicle_road_effect_intensity(int32 value_);
static int32
_is_vehicle_road_effect_side_visible(int32 heading_, int32 layer_, int32 rear_side_);
static void
_calculate_vehicle_road_effect_midpoint(Point2 *midpoint_,
                                        int32   first_index_,
                                        int32   second_index_,
                                        int32   fixed_midpoint_);
static void
_render_vehicle_road_collision_effect(int32                            value_,
                                      int32                            horizontal_base_scale_,
                                      int32                            vertical_base_scale_,
                                      int32                            heading_,
                                      int32                            layer_,
                                      const VehicleRoadEffectGeometry *geometry_);
static void
_render_vehicle_road_tire_smoke(VehicleSimulationState         *state_,
                                VehicleRoadRenderResourceState *resource_);

int32
calculate_vehicle_road_reference_x_projection(const VehicleSimulationState *state_,
                                              int32                         reference_x_,
                                              int32                         reserved_,
                                              int32                         reference_z_)
{
  const ReferenceDistanceRecord *record;
  int32                          heading;

  record = &g_REFERENCE_DISTANCE_RECORDS[state_->simulation_source.alternate_track_segment_index];
  heading =
    (int32) (int16) (((uint16) record
                        ->packed_track_direction[VEHICLE_ROAD_RENDER_REFERENCE_HEADING_HIGH_INDEX]
                      << VEHICLE_ROAD_RENDER_REFERENCE_HEADING_BYTE_SHIFT) |
                     (uint16) record
                       ->packed_track_direction[VEHICLE_ROAD_RENDER_REFERENCE_HEADING_LOW_INDEX]);
  heading <<= VEHICLE_ROAD_RENDER_REFERENCE_HEADING_SHIFT;

  /* Retail evaluates the z component but returns only the x projection. */
  (void) MulSF16(SinF16(heading), record->position.z - reference_z_);
  (void) reserved_;
  return MulSF16(CosF16(heading), record->position.x - reference_x_);
}

int32
calculate_vehicle_road_render_heading_difference(const VehicleSimulationState *state_)
{
  const SimulationRenderState *render_state;
  int32                        camera_heading;
  int32                        heading_difference;
  int32                        vehicle_heading;

  render_state = (const SimulationRenderState *) g_FRONTEND_RENDER_STATE.projection.source;
  camera_heading =
    Atan2F16((state_->simulation_source.position.z - render_state->current_position.z) >>
               VEHICLE_ROAD_RENDER_POSITION_SHIFT,
             (state_->simulation_source.position.x - render_state->current_position.x) >>
               VEHICLE_ROAD_RENDER_POSITION_SHIFT);
  if(camera_heading > VEHICLE_ROAD_RENDER_ANGLE_HALF_TURN)
    camera_heading -= VEHICLE_ROAD_RENDER_ANGLE_FULL_TURN;

  vehicle_heading = state_->simulation_source.alternate_heading;
  if(vehicle_heading > VEHICLE_ROAD_RENDER_ANGLE_HALF_TURN)
    vehicle_heading -= VEHICLE_ROAD_RENDER_ANGLE_FULL_TURN;

  heading_difference = (camera_heading - vehicle_heading);
  if(heading_difference < 0)
    heading_difference += VEHICLE_ROAD_RENDER_ANGLE_FULL_TURN;

  heading_difference &= VEHICLE_ROAD_RENDER_ANGLE_MASK;
  if(heading_difference > VEHICLE_ROAD_RENDER_ANGLE_HALF_TURN)
    heading_difference -= VEHICLE_ROAD_RENDER_ANGLE_FULL_TURN;

  return heading_difference;
}

void
render_vehicle_road_effect_sprite(int32 screen_x_,
                                  int32 screen_y_,
                                  int32 horizontal_scale_,
                                  int32 vertical_scale_)
{
  CCB       *source_cel;
  RenderCcb *ccb;
  int32      horizontal_integer_scale;
  int32      vertical_integer_scale;
  int32      cel_index;
  int32      product;

  cel_index                 = scale_by_random_fraction(VEHICLE_ROAD_RENDER_EFFECT_CEL_COUNT);
  source_cel                = g_SIMULATION_COMMON_ART_CELS[cel_index];
  ccb                       = allocate_template_ccb();
  ccb->fields.flags         = source_cel->ccb_Flags;
  ccb->fields.source_offset = (uint32) source_cel->ccb_SourcePtr;
  ccb->fields.palette       = (uint16 *) source_cel->ccb_PLUTPtr;
  ccb->fields.x_position =
    (int32) (((uint32) screen_x_ << VEHICLE_ROAD_RENDER_EFFECT_POSITION_SHIFT) -
             (uint32) _calculate_vehicle_road_effect_center_offset(horizontal_scale_,
                                                                   source_cel->ccb_Width));
  ccb->fields.y_position =
    (int32) (((uint32) screen_y_ << VEHICLE_ROAD_RENDER_EFFECT_POSITION_SHIFT) -
             (uint32) _calculate_vehicle_road_effect_center_offset(vertical_scale_,
                                                                   source_cel->ccb_Height));

  horizontal_integer_scale =
    _truncate_vehicle_road_effect_value(horizontal_scale_,
                                        VEHICLE_ROAD_RENDER_EFFECT_SCALE_SHIFT,
                                        VEHICLE_ROAD_RENDER_EFFECT_NEGATIVE_SCALE_BIAS);
  product = (int32) ((uint32) ccb->fields.horizontal_dx * (uint32) horizontal_integer_scale);
  ccb->fields.horizontal_dx =
    _truncate_vehicle_road_effect_value(product,
                                        VEHICLE_ROAD_RENDER_EFFECT_SCALE_SHIFT,
                                        VEHICLE_ROAD_RENDER_EFFECT_NEGATIVE_SCALE_BIAS);
  ccb->fields.horizontal_dy = 0;
  ccb->fields.vertical_dx   = 0;

  vertical_integer_scale =
    _truncate_vehicle_road_effect_value(vertical_scale_,
                                        VEHICLE_ROAD_RENDER_EFFECT_SCALE_SHIFT,
                                        VEHICLE_ROAD_RENDER_EFFECT_NEGATIVE_SCALE_BIAS);
  product = (int32) ((uint32) ccb->fields.vertical_dy * (uint32) vertical_integer_scale);
  ccb->fields.vertical_dy =
    _truncate_vehicle_road_effect_value(product,
                                        VEHICLE_ROAD_RENDER_EFFECT_SCALE_SHIFT,
                                        VEHICLE_ROAD_RENDER_EFFECT_NEGATIVE_SCALE_BIAS);
  ccb->fields.horizontal_ddx = 0;
  ccb->fields.horizontal_ddy = 0;
  ccb->fields.pixel_control  = VEHICLE_ROAD_RENDER_EFFECT_PIXEL_CONTROL;
}

void
render_vehicle_road_collision_effects(VehicleSimulationState *state_, int32 layer_)
{
  SimulationRenderState    *render_state;
  SimulationSource         *source;
  VehicleRoadEffectGeometry impulse_geometry;
  VehicleRoadEffectGeometry type_geometry;
  int32                     base_scale;
  int32                     heading;
  int32                     projection_scale;

  source = &state_->simulation_source;
  if((source->high_detail_shape_flags[0] == 0) && (source->high_detail_shape_flags[1] == 0))
    return;
  if((source->collision_impulse == 0) && (source->collision_type == 0))
    return;

  heading = calculate_vehicle_road_render_heading_difference(state_);
  if(state_->render_detail_level != VEHICLE_ROAD_RENDER_DETAIL_HIGHEST)
    return;

  render_state = (SimulationRenderState *) g_FRONTEND_RENDER_STATE.projection.source;
  (void) Atan2F16(
    (source->position.z - render_state->current_position.z) >> VEHICLE_ROAD_RENDER_POSITION_SHIFT,
    (source->position.x - render_state->current_position.x) >> VEHICLE_ROAD_RENDER_POSITION_SHIFT);

  base_scale = VEHICLE_ROAD_RENDER_FIXED16_ONE;
  if((layer_ != 0) && (g_SIMULATION_VECTOR_Z_ADJUSTMENT == 0))
    base_scale = VEHICLE_ROAD_RENDER_EFFECT_HALF_SCALE;
  projection_scale =
    source->road_animation_view_position.z / VEHICLE_ROAD_RENDER_EFFECT_PROJECTION_DIVISOR;
  projection_scale *=
    _truncate_vehicle_road_effect_value(base_scale,
                                        VEHICLE_ROAD_RENDER_EFFECT_SCALE_SHIFT,
                                        VEHICLE_ROAD_RENDER_EFFECT_NEGATIVE_SCALE_BIAS);

  impulse_geometry.front_first_vertex_index = VEHICLE_ROAD_EFFECT_IMPULSE_FRONT_FIRST_VERTEX_INDEX;
  impulse_geometry.front_second_vertex_index =
    VEHICLE_ROAD_EFFECT_IMPULSE_FRONT_SECOND_VERTEX_INDEX;
  impulse_geometry.rear_first_vertex_index  = VEHICLE_ROAD_EFFECT_IMPULSE_REAR_FIRST_VERTEX_INDEX;
  impulse_geometry.rear_second_vertex_index = VEHICLE_ROAD_EFFECT_IMPULSE_REAR_SECOND_VERTEX_INDEX;
  impulse_geometry.uses_fixed_midpoint      = 0;
  _render_vehicle_road_collision_effect(source->collision_impulse,
                                        projection_scale,
                                        projection_scale,
                                        heading,
                                        layer_,
                                        &impulse_geometry);

  type_geometry.front_first_vertex_index  = VEHICLE_ROAD_EFFECT_TYPE_FRONT_FIRST_VERTEX_INDEX;
  type_geometry.front_second_vertex_index = VEHICLE_ROAD_EFFECT_TYPE_FRONT_SECOND_VERTEX_INDEX;
  type_geometry.rear_first_vertex_index   = VEHICLE_ROAD_EFFECT_TYPE_REAR_FIRST_VERTEX_INDEX;
  type_geometry.rear_second_vertex_index  = VEHICLE_ROAD_EFFECT_TYPE_REAR_SECOND_VERTEX_INDEX;
  type_geometry.uses_fixed_midpoint       = 1;
  _render_vehicle_road_collision_effect(source->collision_type,
                                        projection_scale,
                                        projection_scale,
                                        heading,
                                        layer_,
                                        &type_geometry);
}

void
select_vehicle_render_palette(RenderResourceRecord *resource_, int32 palette_index_)
{
  ThreeDObject *object;
  uint8       **shape_pointers;
  uint8        *palette_shape;
  uint8        *shape;
  uint32        palette_reference;
  int32         detail_index;
  int32         shape_index;

  for(detail_index = 0; detail_index < RENDER_SHAPE_POINTER_COUNT; detail_index += 1)
    {
      palette_shape = resource_->palette_shapes[detail_index][palette_index_];
      if(palette_shape != NULL)
        {
          object         = resource_->objects[detail_index];
          shape_pointers = resource_->shape_pointer_allocations[detail_index];
          for(shape_index = 0; shape_index < object->shape_count; shape_index += 1)
            {
              shape             = shape_pointers[shape_index];
              palette_reference = *(uint32 *) shape;
              *(uint32 *) shape =
                (palette_reference & VEHICLE_ROAD_RENDER_PALETTE_REFERENCE_FLAG_MASK) |
                (((uint32) palette_shape - (uint32) shape) &
                 VEHICLE_ROAD_RENDER_PALETTE_REFERENCE_OFFSET_MASK);

              if(detail_index == 0)
                {
                  shape             = resource_->high_detail_shape_variants
                                        [VEHICLE_ROAD_RENDER_SPECIAL_PALETTE_VARIANT_INDEX];
                  palette_reference = *(uint32 *) shape;
                  *(uint32 *) shape =
                    (palette_reference & VEHICLE_ROAD_RENDER_PALETTE_REFERENCE_FLAG_MASK) |
                    (((uint32) palette_shape - (uint32) shape) &
                     VEHICLE_ROAD_RENDER_PALETTE_REFERENCE_OFFSET_MASK);

                  shape             = resource_->distant_sprite_resources[0];
                  palette_reference = *(uint32 *) shape;
                  *(uint32 *) shape =
                    (palette_reference & VEHICLE_ROAD_RENDER_PALETTE_REFERENCE_FLAG_MASK) |
                    (((uint32) palette_shape - (uint32) shape) &
                     VEHICLE_ROAD_RENDER_PALETTE_REFERENCE_OFFSET_MASK);

                  shape             = resource_->distant_sprite_resources[1];
                  palette_reference = *(uint32 *) shape;
                  *(uint32 *) shape =
                    (palette_reference & VEHICLE_ROAD_RENDER_PALETTE_REFERENCE_FLAG_MASK) |
                    (((uint32) palette_shape - (uint32) shape) &
                     VEHICLE_ROAD_RENDER_PALETTE_REFERENCE_OFFSET_MASK);
                }
            }
          resource_->active_palette_shape = palette_index_;
        }
    }
}

void
update_special_vehicle_road_render_shapes(VehicleSimulationState *state_, int32 detail_)
{
  uint8                         **shape_pointers;
  uint8                          *shape;
  VehicleRoadRenderResourceState *render_resource;
  VehicleRuntimeObjectState      *runtime_object;
  int32                           detail_index;
  int32                           animated_status;
  int32                           frame_index;
  int32                           shape_count;
  int32                           shape_index;
  int32                           shape_step;

  if(G_FRONTEND_EXIT_REQUESTED == VEHICLE_ROAD_RENDER_FRONTEND_EXIT_COMPLETE)
    return;

  runtime_object  = (VehicleRuntimeObjectState *) state_;
  render_resource = state_->render_resource_state;
  detail_index    = VEHICLE_ROAD_RENDER_DETAIL_HIGHEST - detail_;
  shape_pointers  = ((RenderResourceRecord *) render_resource)
                      ->shape_pointer_allocations[detail_ - VEHICLE_ROAD_RENDER_DETAIL_MINIMUM];
  if((runtime_object->status_flags & VEHICLE_ROAD_RENDER_ANIMATED_SHAPE_FLAG_MASK) != 0)
    {
      animated_status = 1;
      if(g_VEHICLE_ROAD_ANIMATION_STATE.special_shape_last_frame != G_GAME_FRAME_COUNTER)
        {
          g_VEHICLE_ROAD_ANIMATION_STATE.special_shape_frame_countdown =
            1 - g_VEHICLE_ROAD_ANIMATION_STATE.special_shape_frame_countdown;
          if(g_VEHICLE_ROAD_ANIMATION_STATE.special_shape_frame_countdown != 0)
            {
              g_VEHICLE_ROAD_ANIMATION_STATE.special_shape_frame_counter++;
              if(g_VEHICLE_ROAD_ANIMATION_STATE.special_shape_frame_counter >
                 VEHICLE_ROAD_RENDER_SPECIAL_FRAME_COUNTER_LIMIT)
                {
                  g_VEHICLE_ROAD_ANIMATION_STATE.special_shape_frame_counter = 0;
                }
              g_VEHICLE_ROAD_ANIMATION_STATE.special_shape_frame_phase =
                1 - g_VEHICLE_ROAD_ANIMATION_STATE.special_shape_frame_phase;
            }
        }
      frame_index = g_VEHICLE_ROAD_ANIMATION_STATE.special_shape_frame_counter;
      if(detail_ == VEHICLE_ROAD_RENDER_DETAIL_MINIMUM)
        {
          frame_index += g_VEHICLE_ROAD_ANIMATION_STATE.special_shape_frame_phase *
                         VEHICLE_ROAD_RENDER_ANIMATED_SHAPE_FRAME_STRIDE;
        }
    }
  else
    {
      animated_status = 0;
      frame_index     = 0;
    }

  if(detail_ == VEHICLE_ROAD_RENDER_DETAIL_MINIMUM)
    {
      shape_count = VEHICLE_ROAD_RENDER_SPECIAL_MINIMUM_SHAPE_COUNT;
      shape_step  = VEHICLE_ROAD_RENDER_SPECIAL_SHAPE_INDEX_STEP;
    }
  else
    {
      shape_count = VEHICLE_ROAD_RENDER_SPECIAL_STANDARD_SHAPE_COUNT;
      shape_step  = 1;
    }

  shape =
    render_resource->animated_shapes[detail_index * VEHICLE_ROAD_RENDER_ANIMATED_SHAPE_STRIDE];
  if(shape == NULL)
    return;

  for(shape_index = 0; shape_index < shape_count; shape_index += 1)
    {
      if(animated_status != 0)
        {
          shape =
            render_resource
              ->animated_shapes[(detail_index * VEHICLE_ROAD_RENDER_ANIMATED_SHAPE_STRIDE) +
                                frame_index +
                                (shape_index * VEHICLE_ROAD_RENDER_ANIMATED_SHAPE_FRAME_STRIDE *
                                 shape_step)];
        }
      else
        {
          shape =
            render_resource
              ->animated_shapes[(detail_index * VEHICLE_ROAD_RENDER_ANIMATED_SHAPE_STRIDE) +
                                (shape_index * VEHICLE_ROAD_RENDER_ANIMATED_SHAPE_FRAME_STRIDE *
                                 shape_step)];
        }
      shape_pointers[render_resource->animated_shape_indices
                       [(detail_index * VEHICLE_ROAD_RENDER_ANIMATED_SHAPE_INDEX_STRIDE) +
                        (shape_index * shape_step)]] = shape;
    }
}

void
update_active_vehicle_high_detail_shapes(VehicleSimulationState *state_)
{
  uint8                         **shape_pointers;
  VehicleRoadRenderResourceState *render_resource;
  int32                           horizontal_difference;
  int32                           motion;
  int32                           primary_shape_variant;
  int32                           secondary_shape_variant;
  int32                           shape_variant;
  int32                           vertical_difference;

  render_resource = state_->render_resource_state;
  motion          = state_->simulation_source.pitch_motion_scale;
  if(motion < 0)
    motion = -motion;

  if(motion > VEHICLE_ROAD_RENDER_HIGH_SHAPE_MAX_MOTION)
    {
      shape_variant = VEHICLE_ROAD_RENDER_HIGH_SHAPE_VARIANT_FINAL;
    }
  else if(motion < VEHICLE_ROAD_RENDER_HIGH_SHAPE_MIN_MOTION)
    {
      shape_variant = 0;
    }
  else
    {
      if((g_VEHICLE_ROAD_ANIMATION_STATE.high_detail_shape_last_frame == G_GAME_FRAME_COUNTER) ||
         (G_FRONTEND_EXIT_REQUESTED == VEHICLE_ROAD_RENDER_FRONTEND_EXIT_COMPLETE))
        {
          shape_variant = 1;
        }
      else
        {
          g_VEHICLE_ROAD_ANIMATION_STATE.high_detail_shape_countdown =
            1 - g_VEHICLE_ROAD_ANIMATION_STATE.high_detail_shape_countdown;
          if(motion >= VEHICLE_ROAD_RENDER_HIGH_SHAPE_PHASE_MOTION)
            {
              shape_variant = g_VEHICLE_ROAD_ANIMATION_STATE.high_detail_shape_countdown + 1;
            }
          else if(g_VEHICLE_ROAD_ANIMATION_STATE.high_detail_shape_countdown != 0)
            {
              g_VEHICLE_ROAD_ANIMATION_STATE.high_detail_shape_phase =
                1 - g_VEHICLE_ROAD_ANIMATION_STATE.high_detail_shape_phase;
              shape_variant = g_VEHICLE_ROAD_ANIMATION_STATE.high_detail_shape_phase + 1;
            }
          else
            {
              shape_variant = 1;
            }
        }
      g_VEHICLE_ROAD_ANIMATION_STATE.high_detail_shape_last_frame = G_GAME_FRAME_COUNTER;
    }

  primary_shape_variant = shape_variant;

  horizontal_difference = g_VEHICLE_ROAD_ANIMATION_STATE.high_detail_shape_previous_x -
                          state_->simulation_source.alternate_surface_origin.x;
  vertical_difference   = g_VEHICLE_ROAD_ANIMATION_STATE.high_detail_shape_previous_y -
                          state_->simulation_source.alternate_surface_origin.y;
  g_VEHICLE_ROAD_ANIMATION_STATE.high_detail_shape_previous_x =
    state_->simulation_source.alternate_surface_origin.x;
  g_VEHICLE_ROAD_ANIMATION_STATE.high_detail_shape_previous_y =
    state_->simulation_source.alternate_surface_origin.y;

  if(g_PLAYER_VEHICLE_INDEX == 1)
    {
      if((state_->simulation_source.high_detail_shape_flags[0] & 1) != 0)
        {
          primary_shape_variant = 0;
        }
      else if((state_->simulation_source.high_detail_shape_flags[0] &
               VEHICLE_ROAD_RENDER_HIGH_SHAPE_DYNAMIC_FLAG) == 0)
        {
          primary_shape_variant = VEHICLE_ROAD_RENDER_HIGH_SHAPE_VARIANT_FINAL;
        }
    }
  else if((state_->simulation_source.high_detail_shape_flags[0] & 1) != 0)
    {
      if(horizontal_difference < 0)
        horizontal_difference = -horizontal_difference;
      if(vertical_difference < 0)
        vertical_difference = -vertical_difference;
      if((horizontal_difference + vertical_difference) < VEHICLE_ROAD_RENDER_HIGH_SHAPE_MIN_MOTION)
        {
          primary_shape_variant = 0;
        }
      else if((state_->simulation_source.high_detail_shape_flags[0] &
               VEHICLE_ROAD_RENDER_HIGH_SHAPE_DYNAMIC_FLAG) == 0)
        {
          primary_shape_variant = VEHICLE_ROAD_RENDER_HIGH_SHAPE_VARIANT_FINAL;
        }
    }
  else if((state_->simulation_source.high_detail_shape_flags[0] &
           VEHICLE_ROAD_RENDER_HIGH_SHAPE_DYNAMIC_FLAG) == 0)
    {
      primary_shape_variant = VEHICLE_ROAD_RENDER_HIGH_SHAPE_VARIANT_FINAL;
    }

  if((state_->simulation_source.high_detail_shape_flags[1] & 1) != 0)
    {
      secondary_shape_variant = 0;
    }
  else if((state_->simulation_source.high_detail_shape_flags[1] &
           VEHICLE_ROAD_RENDER_HIGH_SHAPE_DYNAMIC_FLAG) != 0)
    {
      secondary_shape_variant = shape_variant;
    }
  else
    {
      secondary_shape_variant = VEHICLE_ROAD_RENDER_HIGH_SHAPE_VARIANT_FINAL;
    }

  if(render_resource->high_detail_shape_variants[primary_shape_variant] == NULL)
    {
      return;
    }

  shape_pointers = ((RenderResourceRecord *) render_resource)
                     ->shape_pointer_allocations[VEHICLE_ROAD_RENDER_SHAPE_DETAIL_INDEX];
  shape_pointers[render_resource
                   ->high_detail_shape_indices[VEHICLE_ROAD_RENDER_HIGH_SHAPE_PRIMARY_FIRST]] =
    render_resource->high_detail_shape_variants[primary_shape_variant];
  shape_pointers[render_resource
                   ->high_detail_shape_indices[VEHICLE_ROAD_RENDER_HIGH_SHAPE_PRIMARY_SECOND]] =
    render_resource->high_detail_shape_variants[primary_shape_variant];
  shape_pointers[render_resource
                   ->high_detail_shape_indices[VEHICLE_ROAD_RENDER_HIGH_SHAPE_SECONDARY_FIRST]] =
    render_resource->high_detail_shape_variants[secondary_shape_variant];
  shape_pointers[render_resource
                   ->high_detail_shape_indices[VEHICLE_ROAD_RENDER_HIGH_SHAPE_SECONDARY_SECOND]] =
    render_resource->high_detail_shape_variants[secondary_shape_variant];
}

void
update_vehicle_road_render_brake_lights(VehicleSimulationState *state_)
{
  VehicleRoadRenderResourceState *render_resource;
  uint8                         **shape_pointers;
  uint32                         *surface_shape;

  render_resource = state_->render_resource_state;
  if(state_->simulation_source.traffic_brake_indicator <= VEHICLE_ROAD_RENDER_SURFACE_DETAIL_LIMIT)
    {
      surface_shape = (uint32 *) render_resource->brake_light_shapes[0];
    }
  else
    {
      surface_shape = (uint32 *) render_resource->brake_light_shapes[1];
    }

  if(surface_shape == NULL)
    return;

  shape_pointers = ((RenderResourceRecord *) render_resource)
                     ->shape_pointer_allocations[VEHICLE_ROAD_RENDER_SHAPE_DETAIL_INDEX];
  shape_pointers[render_resource->brake_light_shape_indices[0]] = (uint8 *) surface_shape;
  shape_pointers[render_resource->brake_light_shape_indices[1]] = (uint8 *) surface_shape;
}

void
update_vehicle_road_render_wheels(VehicleSimulationState *state_)
{
  ThreeDObject *object;
  int32        *material_table;
  int32         axle_offset;
  int32         signed_axle_offset;
  int32         heading_difference;
  int32         half_axle_offset;
  int32         rear_axle_offset;
  int32         rear_half_axle_offset;
  int32         wheel_vertex_index;

  object         = (ThreeDObject *) state_->render_object;
  material_table = (int32 *) object->material_table;
  signed_axle_offset =
    (state_->simulation_source.heading_rotation >> VEHICLE_ROAD_RENDER_HEADING_INTEGER_SHIFT) /
    VEHICLE_ROAD_RENDER_AXLE_DIVISOR;
  axle_offset = signed_axle_offset;
  heading_difference = calculate_vehicle_road_render_heading_difference(state_);
  if((heading_difference < VEHICLE_ROAD_RENDER_WHEEL_TURN_MIN) ||
     (VEHICLE_ROAD_RENDER_WHEEL_TURN_MAX < heading_difference))
    {
      axle_offset = 0;
    }
  half_axle_offset = axle_offset;
  if(half_axle_offset < 0)
    half_axle_offset = -half_axle_offset;
  half_axle_offset /= VEHICLE_ROAD_RENDER_AXLE_DIVISOR;

  wheel_vertex_index =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_FRONT_LEFT_INDEX];
  material_table[wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_FRONT_LEFT_X_BASE] +
    axle_offset;
  wheel_vertex_index =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_FRONT_RIGHT_INDEX];
  material_table[wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_FRONT_RIGHT_X_BASE] +
    axle_offset;
  wheel_vertex_index =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_FRONT_LEFT_INDEX];
  material_table[(wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT) +
                 VEHICLE_ROAD_RENDER_VERTEX_DEPTH_COMPONENT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_FRONT_LEFT_Z_BASE] -
    half_axle_offset;
  wheel_vertex_index =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_FRONT_RIGHT_INDEX];
  material_table[(wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT) +
                 VEHICLE_ROAD_RENDER_VERTEX_DEPTH_COMPONENT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_FRONT_RIGHT_Z_BASE] -
    half_axle_offset;
  wheel_vertex_index = g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
                         [VEHICLE_ROAD_WHEEL_FRONT_LEFT_REAR_INDEX];
  material_table[wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
      [VEHICLE_ROAD_WHEEL_FRONT_LEFT_REAR_X_BASE] -
    axle_offset;
  wheel_vertex_index = g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
                         [VEHICLE_ROAD_WHEEL_FRONT_RIGHT_REAR_INDEX];
  material_table[wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
      [VEHICLE_ROAD_WHEEL_FRONT_RIGHT_REAR_X_BASE] -
    axle_offset;
  wheel_vertex_index = g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
                         [VEHICLE_ROAD_WHEEL_FRONT_LEFT_REAR_INDEX];
  material_table[(wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT) +
                 VEHICLE_ROAD_RENDER_VERTEX_DEPTH_COMPONENT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
      [VEHICLE_ROAD_WHEEL_FRONT_LEFT_REAR_Z_BASE] +
    half_axle_offset;
  wheel_vertex_index = g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
                         [VEHICLE_ROAD_WHEEL_FRONT_RIGHT_REAR_INDEX];
  material_table[(wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT) +
                 VEHICLE_ROAD_RENDER_VERTEX_DEPTH_COMPONENT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
      [VEHICLE_ROAD_WHEEL_FRONT_RIGHT_REAR_Z_BASE] +
    half_axle_offset;

  rear_axle_offset = signed_axle_offset;
  if((heading_difference > -VEHICLE_ROAD_RENDER_WHEEL_TURN_MIN) ||
     (heading_difference < -VEHICLE_ROAD_RENDER_WHEEL_TURN_MAX))
    {
      rear_axle_offset = 0;
    }
  rear_half_axle_offset = rear_axle_offset;
  if(rear_half_axle_offset < 0)
    rear_half_axle_offset = -rear_half_axle_offset;
  rear_half_axle_offset /= VEHICLE_ROAD_RENDER_AXLE_DIVISOR;

  wheel_vertex_index =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_REAR_LEFT_INDEX];
  material_table[wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_REAR_LEFT_X_BASE] +
    rear_axle_offset;
  wheel_vertex_index =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_REAR_RIGHT_INDEX];
  material_table[wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_REAR_RIGHT_X_BASE] +
    rear_axle_offset;
  wheel_vertex_index =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_REAR_LEFT_INDEX];
  material_table[(wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT) +
                 VEHICLE_ROAD_RENDER_VERTEX_DEPTH_COMPONENT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_REAR_LEFT_Z_BASE] -
    rear_half_axle_offset;
  wheel_vertex_index =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_REAR_RIGHT_INDEX];
  material_table[(wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT) +
                 VEHICLE_ROAD_RENDER_VERTEX_DEPTH_COMPONENT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[VEHICLE_ROAD_WHEEL_REAR_RIGHT_Z_BASE] -
    rear_half_axle_offset;
  wheel_vertex_index = g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
                         [VEHICLE_ROAD_WHEEL_REAR_LEFT_REAR_INDEX];
  material_table[wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
      [VEHICLE_ROAD_WHEEL_REAR_LEFT_REAR_X_BASE] -
    rear_axle_offset;
  wheel_vertex_index = g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
                         [VEHICLE_ROAD_WHEEL_REAR_RIGHT_REAR_INDEX];
  material_table[wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
      [VEHICLE_ROAD_WHEEL_REAR_RIGHT_REAR_X_BASE] -
    rear_axle_offset;
  wheel_vertex_index = g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
                         [VEHICLE_ROAD_WHEEL_REAR_LEFT_REAR_INDEX];
  material_table[(wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT) +
                 VEHICLE_ROAD_RENDER_VERTEX_DEPTH_COMPONENT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
      [VEHICLE_ROAD_WHEEL_REAR_LEFT_REAR_Z_BASE] +
    rear_half_axle_offset;
  wheel_vertex_index = g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
                         [VEHICLE_ROAD_WHEEL_REAR_RIGHT_REAR_INDEX];
  material_table[(wheel_vertex_index * VEHICLE_ROAD_RENDER_VERTEX_COMPONENT_COUNT) +
                 VEHICLE_ROAD_RENDER_VERTEX_DEPTH_COMPONENT] =
    g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values
      [VEHICLE_ROAD_WHEEL_REAR_RIGHT_REAR_Z_BASE] +
    rear_half_axle_offset;
}

void
project_high_detail_vehicle_vertices(int32            vertex_count_,
                                     const Matrix3x3 *matrix_,
                                     const Vector3   *translation_,
                                     const Vector3   *vertices_)
{
  Vector3 *workspace;
  int32    vertex_index;

  workspace = (Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER;
  multiply_vector3_array_matrix3x3(vertex_count_, vertices_, matrix_, workspace);
  translate_vector3_array(vertex_count_, workspace, translation_, workspace);
  for(vertex_index = 0; vertex_index < vertex_count_; vertex_index++)
    {
      if(workspace[vertex_index].z <
         g_VEHICLE_ROAD_ANIMATION_STATE.minimum_vehicle_projection_depth)
        {
          workspace[vertex_index].z =
            g_VEHICLE_ROAD_ANIMATION_STATE.minimum_vehicle_projection_depth;
        }
    }
  project_vector3_array_with_depth_table(vertex_count_,
                                         workspace,
                                         (Point2 *) g_SCREEN_COORDINATE_BUFFER,
                                         0);
}

void
update_vehicle_road_polygon_depth_keys(int32                     polygon_count_,
                                       const PolygonDepthRecord *polygons_,
                                       const Vector3            *vertices_,
                                       uint32                   *keys_,
                                       uint32                   *scratch_)
{
  int32 polygon_index;
  int32 vertex_index;
  int32 vertex_slot;
  int32 minimum_depth;
  int32 depth;

  for(polygon_index = 0; polygon_index < polygon_count_; polygon_index++)
    {
      vertex_index  = ((int32) keys_[polygon_index]) >> VEHICLE_ROAD_RENDER_POLYGON_KEY_SHIFT;
      minimum_depth = vertices_[polygons_[vertex_index].vertex_indices[0]].z;
      for(vertex_slot = 1; vertex_slot < VEHICLE_ROAD_RENDER_POLYGON_VERTEX_COUNT; vertex_slot++)
        {
          depth = vertices_[polygons_[vertex_index].vertex_indices[vertex_slot]].z;
          if(depth < minimum_depth)
            minimum_depth = depth;
        }
      if(minimum_depth < g_SECONDARY_RENDER_DEPTH_LIMIT)
        minimum_depth = 0;
      keys_[polygon_index] =
        ((uint32) vertex_index << VEHICLE_ROAD_RENDER_POLYGON_KEY_SHIFT) | (uint32) minimum_depth;
    }
  sort_words_by_low16_descending(polygon_count_, keys_, scratch_);
}

void
update_vehicle_road_polygon_batch_depth_keys(int32                     polygon_count_,
                                             const PolygonDepthRecord *polygons_,
                                             const Vector3            *vertices_,
                                             uint32                   *keys_,
                                             uint32                   *scratch_)
{
  int32 polygon_index;
  int32 vertex_index;
  int32 vertex_slot;
  int32 minimum_depth;
  int32 depth;
  int32 depth_limit;

  depth_limit = g_SECONDARY_RENDER_DEPTH_LIMIT / VEHICLE_ROAD_RENDER_AXLE_DIVISOR;
  for(polygon_index = 0; polygon_index < polygon_count_; polygon_index++)
    {
      vertex_index  = ((int32) keys_[polygon_index]) >> VEHICLE_ROAD_RENDER_POLYGON_KEY_SHIFT;
      minimum_depth = vertices_[polygons_[vertex_index].vertex_indices[0]].z;
      for(vertex_slot = 1; vertex_slot < VEHICLE_ROAD_RENDER_POLYGON_VERTEX_COUNT; vertex_slot++)
        {
          depth = vertices_[polygons_[vertex_index].vertex_indices[vertex_slot]].z;
          if(depth < minimum_depth)
            minimum_depth = depth;
        }
      if(minimum_depth < depth_limit)
        minimum_depth = 0;
      keys_[polygon_index] =
        ((uint32) vertex_index << VEHICLE_ROAD_RENDER_POLYGON_KEY_SHIFT) | (uint32) minimum_depth;
    }
  sort_words_by_low16_descending(polygon_count_, keys_, scratch_);
}

static int32
_select_vehicle_road_render_detail(VehicleSimulationState *state_, int32 direction_)
{
  RenderResourceRecord *resource;
  int32                 detail_level;
  int32                 threshold;

  resource = state_->render_resource_state;
  for(detail_level = VEHICLE_ROAD_RENDER_DETAIL_HIGHEST; detail_level >= 0; detail_level--)
    {
      threshold = g_CAR_RENDER_DISTANCE_THRESHOLDS[detail_level];
      if(direction_ != 0)
        threshold /= VEHICLE_ROAD_RENDER_REVERSE_DISTANCE_DIVISOR;
      if(state_->simulation_source.road_animation_view_position.z <= threshold)
        {
          state_->render_detail_level = detail_level;
          if(detail_level >= VEHICLE_ROAD_RENDER_DETAIL_MINIMUM)
            {
              state_->render_object =
                resource->objects[detail_level - VEHICLE_ROAD_RENDER_DETAIL_MINIMUM];
              state_->render_shape_pointers =
                resource
                  ->shape_pointer_allocations[detail_level - VEHICLE_ROAD_RENDER_DETAIL_MINIMUM];
            }
          break;
        }
    }
  return state_->render_detail_level;
}

int32
prepare_vehicle_road_render(VehicleSimulationState *state_, int32 direction_)
{
  const Matrix3x3           *view_matrix;
  SimulationRenderState     *render_state;
  SimulationSource          *source;
  RenderResourceRecord      *resource;
  ThreeDObject              *object;
  VehicleRuntimeObjectState *runtime_object;
  Matrix3x3                  orientation_matrix;
  Matrix3x3                  x_rotation;
  Matrix3x3                  y_rotation;
  Matrix3x3                  z_rotation;
  Matrix3x3                  xz_rotation;
  int32                      previous_detail;
  void                      *previous_object;
  int32                      view_horizontal_position;
  int32                      track_distance;
  int32                      view_depth;

  source       = &state_->simulation_source;
  render_state = (SimulationRenderState *) g_FRONTEND_RENDER_STATE.projection.source;
  track_distance =
    source->alternate_track_segment_index - (int32) render_state->render_track_segment_index;
  if(track_distance < 0)
    track_distance = -track_distance;
  if(track_distance > VEHICLE_ROAD_RENDER_TRACK_RANGE)
    return 0;

  view_depth = source->road_animation_view_position.z;
  if(direction_ != 0)
    {
      if(((g_CAR_RENDER_DISTANCE_THRESHOLDS[0] / VEHICLE_ROAD_RENDER_REVERSE_DISTANCE_DIVISOR) <=
          view_depth) ||
         (view_depth < VEHICLE_ROAD_RENDER_REVERSE_DEPTH_LIMIT))
        {
          return 0;
        }
    }
  if((view_depth < VEHICLE_ROAD_RENDER_NEAR_DEPTH_LIMIT) ||
     (g_CAR_RENDER_DISTANCE_THRESHOLDS[0] < view_depth))
    {
      return 0;
    }
  view_horizontal_position = source->road_animation_view_position.x;
  if(view_horizontal_position < 0)
    view_horizontal_position = -view_horizontal_position;
  if(view_depth < view_horizontal_position)
    return 0;

  resource = state_->render_resource_state;
  if(view_depth < ((source->longitudinal_axle_span / VEHICLE_ROAD_RENDER_AXLE_DIVISOR) >>
                   VEHICLE_ROAD_RENDER_POSITION_SHIFT) +
                    VEHICLE_ROAD_RENDER_NEAR_DEPTH_LIMIT)
    {
      resource->sort_refresh_countdown = 1;
    }
  else if((state_ == (VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE) &&
          ((source->heading_velocity < -VEHICLE_ROAD_RENDER_HIGH_HEADING_LIMIT) ||
           (VEHICLE_ROAD_RENDER_HIGH_HEADING_LIMIT < source->heading_velocity)))
    {
      resource->sort_refresh_countdown = 1;
    }

  previous_object = state_->render_object;
  previous_detail = _select_vehicle_road_render_detail(state_, direction_);
  if(previous_detail < VEHICLE_ROAD_RENDER_DETAIL_MINIMUM)
    {
      runtime_object = (VehicleRuntimeObjectState *) state_;
      if((runtime_object->render_update_callback == (void *) update_player_vehicle_state) ||
         (runtime_object->render_update_callback == (void *) update_recovering_vehicle_simulation))
        {
          state_->render_detail_level   = VEHICLE_ROAD_RENDER_DETAIL_MINIMUM;
          state_->render_object         = resource->objects[0];
          state_->render_shape_pointers = resource->shape_pointer_allocations[0];
        }
      else
        {
          previous_detail = calculate_vehicle_road_render_heading_difference(state_);
          if(previous_detail < 0)
            previous_detail = -previous_detail;
          if((VEHICLE_ROAD_RENDER_HEADING_FORCE_DETAIL_MIN < previous_detail) &&
             (previous_detail < VEHICLE_ROAD_RENDER_HEADING_FORCE_DETAIL_MAX))
            {
              state_->render_detail_level   = VEHICLE_ROAD_RENDER_DETAIL_MINIMUM;
              state_->render_object         = resource->objects[0];
              state_->render_shape_pointers = resource->shape_pointer_allocations[0];
            }
        }
    }

  if((state_->render_detail_level >= VEHICLE_ROAD_RENDER_DETAIL_MINIMUM) &&
     (previous_object != state_->render_object))
    {
      object = (ThreeDObject *) state_->render_object;
      if(state_->render_detail_level < VEHICLE_ROAD_RENDER_DETAIL_HIGHEST)
        fill_fixed16_ramp(object->shape_count, (int32 *) resource->sort_keys);
      resource->sort_refresh_countdown = 1;
    }

  if(source->alternate_heading_refresh_pending == 0)
    {
      initialize_matrix3x3_rotation_x_1024(
        &x_rotation,
        (source->alternate_direction_x + source->mode_zero_x_rotation) >>
          VEHICLE_ROAD_RENDER_ROTATION_SHIFT);
      initialize_matrix3x3_rotation_z_1024(
        &z_rotation,
        ((source->mode_zero_z_rotation -
          (source->mode_zero_z_rotation / VEHICLE_ROAD_RENDER_REVERSE_DISTANCE_DIVISOR)) +
         source->road_render_z_rotation_offset) >>
          VEHICLE_ROAD_RENDER_ROTATION_SHIFT);
      initialize_matrix3x3_rotation_y_1024(&y_rotation,
                                           source->alternate_heading >>
                                             VEHICLE_ROAD_RENDER_ROTATION_SHIFT);
      multiply_matrix_3x3(&x_rotation, &z_rotation, &xz_rotation);
      multiply_matrix_3x3(&xz_rotation, &y_rotation, &orientation_matrix);
    }
  else
    {
      orientation_matrix = *(const Matrix3x3 *) source->reserved_37_c;
    }

  view_matrix = (const Matrix3x3 *) g_FRONTEND_RENDER_STATE.projection.view_state;
  multiply_matrix_3x3(&orientation_matrix,
                      view_matrix,
                      &g_VEHICLE_ROAD_ANIMATION_STATE.vehicle_model_view_matrix);
  if((state_ == (VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE) &&
     (state_->render_detail_level == VEHICLE_ROAD_RENDER_DETAIL_HIGHEST))
    {
      update_vehicle_road_render_wheels(state_);
    }

  if(state_->render_detail_level < VEHICLE_ROAD_RENDER_DETAIL_MINIMUM)
    {
      transform_project_vector3_array(1,
                                      &g_VEHICLE_ROAD_ANIMATION_STATE.vehicle_model_view_matrix,
                                      &source->road_animation_view_position,
                                      &g_VEHICLE_ROAD_ANIMATION_STATE.low_detail_projection_vertex,
                                      (Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
                                      (Point2 *) g_SCREEN_COORDINATE_BUFFER,
                                      0);
    }
  else
    {
      object = (ThreeDObject *) state_->render_object;
      if(state_->render_detail_level < VEHICLE_ROAD_RENDER_DETAIL_HIGHEST)
        {
          transform_project_vector3_array(object->material_count,
                                          &g_VEHICLE_ROAD_ANIMATION_STATE.vehicle_model_view_matrix,
                                          &source->road_animation_view_position,
                                          (const Vector3 *) object->material_table,
                                          (Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
                                          (Point2 *) g_SCREEN_COORDINATE_BUFFER,
                                          0);
        }
      else
        {
          project_high_detail_vehicle_vertices(
            object->material_count,
            &g_VEHICLE_ROAD_ANIMATION_STATE.vehicle_model_view_matrix,
            &source->road_animation_view_position,
            (const Vector3 *) object->material_table);
        }
    }
  return 1;
}

void
render_distant_vehicle_road_sprite(VehicleSimulationState *state_,
                                   void                   *resource_,
                                   int32                   direction_)
{
  const uint32                   *cel_header;
  const Point2                   *screen_point;
  RenderCcb                      *ccb;
  VehicleRoadRenderResourceState *render_resource;
  int32                           cosine;
  int32                           heading_difference;
  int32                           horizontal_offset;
  int32                           integer_scale;
  int32                           scale;
  int32                           sine;
  int32                           sprite_variant;
  int32                           sprite_header;
  int32                           threshold;
  int32                           vector_z_adjustment;
  int32                           vertical_offset;

  render_resource = (VehicleRoadRenderResourceState *) resource_;
  if(state_->render_detail_level == VEHICLE_ROAD_RENDER_SPRITE_DETAIL_LEVEL)
    {
      heading_difference = calculate_vehicle_road_render_heading_difference(state_);
      if(heading_difference < 0)
        heading_difference = -heading_difference;
      if(heading_difference <= VEHICLE_ROAD_RENDER_SPRITE_STRAIGHT_HEADING)
        sprite_variant = VEHICLE_ROAD_RENDER_SPRITE_VARIANT_FORWARD;
      else
        sprite_variant = VEHICLE_ROAD_RENDER_SPRITE_VARIANT_FIRST;
    }
  else
    {
      sprite_variant = VEHICLE_ROAD_RENDER_SPRITE_VARIANT_DEFAULT;
    }

  if(direction_ != 0)
    {
      threshold = g_CAR_RENDER_DISTANCE_THRESHOLDS[state_->render_detail_level + 1];
      scale     = state_->simulation_source.road_animation_view_position.z;
      if(scale < 0)
        scale += (1 << VEHICLE_ROAD_RENDER_SPRITE_DIRECTION_SHIFT) - 1;

      scale = (threshold << VEHICLE_ROAD_RENDER_SPRITE_SCALE_SHIFT) /
              (scale >> VEHICLE_ROAD_RENDER_SPRITE_DIRECTION_SHIFT);
    }
  else if(state_->render_detail_level == VEHICLE_ROAD_RENDER_SPRITE_DETAIL_LEVEL)
    {
      threshold = g_CAR_RENDER_DISTANCE_THRESHOLDS[state_->render_detail_level + 1];
      scale     = state_->simulation_source.road_animation_view_position.z;
      if(scale < 0)
        scale += (1 << VEHICLE_ROAD_RENDER_SPRITE_DEFAULT_SHIFT) - 1;

      scale = (threshold << VEHICLE_ROAD_RENDER_SPRITE_SCALE_SHIFT) /
              (scale >> VEHICLE_ROAD_RENDER_SPRITE_DEFAULT_SHIFT);
    }
  else
    {
      scale = VEHICLE_ROAD_RENDER_FIXED16_ONE;
    }

  if(sprite_variant == VEHICLE_ROAD_RENDER_SPRITE_VARIANT_FIRST)
    {
      cel_header =
        (const uint32 *)
          render_resource->high_detail_shape_variants[VEHICLE_ROAD_RENDER_SPRITE_HIGH_VARIANT_LAST];
    }
  else
    {
      cel_header =
        (const uint32 *) render_resource
          ->distant_sprite_resources[sprite_variant - VEHICLE_ROAD_RENDER_SPRITE_VARIANT_FORWARD];
    }
  sprite_header = (int32) cel_header[1];
  integer_scale = scale;
  if(integer_scale < 0)
    integer_scale += VEHICLE_ROAD_RENDER_SPRITE_NEGATIVE_VERTICAL_BIAS;

  integer_scale >>= VEHICLE_ROAD_RENDER_SPRITE_SCALE_SHIFT;
  horizontal_offset = (integer_scale * ((int32) ((uint32) sprite_header
                                                 << VEHICLE_ROAD_RENDER_SPRITE_HEADER_HIGH_SHIFT) >>
                                        VEHICLE_ROAD_RENDER_SPRITE_HEADER_SIGN_SHIFT));
  if(horizontal_offset < 0)
    horizontal_offset += VEHICLE_ROAD_RENDER_SPRITE_NEGATIVE_HORIZONTAL_BIAS;
  horizontal_offset >>= VEHICLE_ROAD_RENDER_SPRITE_HORIZONTAL_SHIFT;

  vertical_offset = (integer_scale * ((int32) ((uint32) sprite_header
                                               << VEHICLE_ROAD_RENDER_SPRITE_HEADER_LOW_SHIFT) >>
                                      VEHICLE_ROAD_RENDER_SPRITE_HEADER_SIGN_SHIFT));
  if(vertical_offset < 0)
    vertical_offset += VEHICLE_ROAD_RENDER_SPRITE_NEGATIVE_VERTICAL_BIAS;
  vertical_offset >>= VEHICLE_ROAD_RENDER_SPRITE_VERTICAL_SHIFT;

  vector_z_adjustment = g_SIMULATION_VECTOR_Z_ADJUSTMENT;
  if(vector_z_adjustment != 0)
    {
      sine                = SinF16(vector_z_adjustment);
      vector_z_adjustment = g_SIMULATION_VECTOR_Z_ADJUSTMENT;
      cosine              = CosF16(vector_z_adjustment);
      ccb                 = g_CCB_MANAGER_STATE.allocation_cursor;
      screen_point        = g_VEHICLE_ROAD_ANIMATION_STATE.quad_screen_points;
      (void) draw_scaled_frame(
        scale,
        cel_header,
        (screen_point->x >> VEHICLE_ROAD_RENDER_FIXED16_SHIFT) - horizontal_offset,
        (screen_point->y >> VEHICLE_ROAD_RENDER_FIXED16_SHIFT) - vertical_offset);
      ccb->fields.x_position +=
        ((cosine >> VEHICLE_ROAD_RENDER_SPRITE_ROTATION_COMPONENT_SHIFT) *
         (horizontal_offset >> VEHICLE_ROAD_RENDER_SPRITE_ROTATION_COMPONENT_SHIFT)) -
        ((sine >> VEHICLE_ROAD_RENDER_SPRITE_ROTATION_COMPONENT_SHIFT) *
         (vertical_offset >> VEHICLE_ROAD_RENDER_SPRITE_ROTATION_COMPONENT_SHIFT));
      ccb->fields.y_position +=
        ((sine >> VEHICLE_ROAD_RENDER_SPRITE_ROTATION_COMPONENT_SHIFT) *
         (horizontal_offset >> VEHICLE_ROAD_RENDER_SPRITE_ROTATION_COMPONENT_SHIFT)) +
        ((cosine >> VEHICLE_ROAD_RENDER_SPRITE_ROTATION_COMPONENT_SHIFT) *
         (vertical_offset >> VEHICLE_ROAD_RENDER_SPRITE_ROTATION_COMPONENT_SHIFT));
      ccb->fields.horizontal_dx =
        (int32) ((uint32) cosine << VEHICLE_ROAD_RENDER_SPRITE_ROTATION_CCB_SHIFT);
      ccb->fields.horizontal_dy =
        (int32) ((uint32) sine << VEHICLE_ROAD_RENDER_SPRITE_ROTATION_CCB_SHIFT);
      ccb->fields.vertical_dx = -sine;
      ccb->fields.vertical_dy = cosine;
      return;
    }

  screen_point = g_VEHICLE_ROAD_ANIMATION_STATE.quad_screen_points;
  (void) draw_scaled_frame(
    scale,
    cel_header,
    (screen_point->x >> VEHICLE_ROAD_RENDER_FIXED16_SHIFT) - horizontal_offset,
    (screen_point->y >> VEHICLE_ROAD_RENDER_FIXED16_SHIFT) - vertical_offset);
}

int32
render_vehicle_road_animation(VehicleSimulationState *state_, int32 direction_)
{
  const PolygonDepthRecord       *polygons;
  ThreeDObject                   *object;
  VehicleRoadPolygonBatch        *batches;
  VehicleRoadRenderResourceState *resource;
  VehicleRuntimeObjectState      *runtime_object;
  uint32                          saved_pixel_control;
  int32                           detail_level;
  int32                           track_geometry_type;

  if(prepare_vehicle_road_render(state_, direction_) == 0)
    return 0;

  resource                       = state_->render_resource_state;
  object                         = (ThreeDObject *) state_->render_object;
  detail_level                   = state_->render_detail_level;
  g_PRIMARY_RENDER_DEPTH_LIMIT   = VEHICLE_ROAD_RENDER_PRIMARY_DEPTH_LIMIT;
  g_POLYGON_DEPTH_SORT_THRESHOLD = VEHICLE_ROAD_RENDER_POLYGON_DEPTH_THRESHOLD;

  if((state_ == g_VEHICLE_SIMULATION_STATES[VEHICLE_ROAD_RENDER_SPECIAL_STATE_INDEX]) &&
     (detail_level >= VEHICLE_ROAD_RENDER_DETAIL_MINIMUM))
    {
      update_special_vehicle_road_render_shapes(state_, detail_level);
    }

  if(detail_level == VEHICLE_ROAD_RENDER_DETAIL_HIGHEST)
    {
      update_vehicle_road_render_brake_lights(state_);
      if(state_ == (const VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE)
        update_active_vehicle_high_detail_shapes(state_);
    }

  resource->sort_refresh_countdown--;
  runtime_object = (VehicleRuntimeObjectState *) state_;
  if((resource->sort_refresh_countdown == 0) ||
     (runtime_object->render_update_callback == (void *) update_player_vehicle_state))
    {
      if(detail_level == VEHICLE_ROAD_RENDER_DETAIL_MINIMUM)
        {
          update_vehicle_road_polygon_depth_keys(
            object->shape_count,
            (const PolygonDepthRecord *) object->shape_table,
            (const Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
            resource->sort_keys,
            g_VEHICLE_ROAD_ANIMATION_STATE.polygon_depth_sort_scratch);
        }
      else if(detail_level > VEHICLE_ROAD_RENDER_DETAIL_MINIMUM)
        {
          if(detail_level == VEHICLE_ROAD_RENDER_DETAIL_HIGHEST)
            batches = resource->high_detail_batches;
          else
            batches = resource->medium_detail_batches;

          if(batches[1].polygon_count < 1)
            {
              update_vehicle_road_polygon_depth_keys(
                object->shape_count,
                (const PolygonDepthRecord *) object->shape_table,
                (const Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
                resource->sort_keys,
                g_VEHICLE_ROAD_ANIMATION_STATE.polygon_depth_sort_scratch);
            }
          else
            {
              update_vehicle_road_polygon_batch_depth_keys(
                batches[0].polygon_count,
                (const PolygonDepthRecord *) object->shape_table,
                (const Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
                batches[0].sort_keys,
                g_VEHICLE_ROAD_ANIMATION_STATE.polygon_depth_sort_scratch);
              update_vehicle_road_polygon_batch_depth_keys(
                batches[1].polygon_count,
                (const PolygonDepthRecord *) object->shape_table,
                (const Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
                batches[1].sort_keys,
                g_VEHICLE_ROAD_ANIMATION_STATE.polygon_depth_sort_scratch);
              if(0 < batches[VEHICLE_ROAD_RENDER_OPTIONAL_BATCH_INDEX].polygon_count)
                {
                  update_vehicle_road_polygon_batch_depth_keys(
                    batches[VEHICLE_ROAD_RENDER_OPTIONAL_BATCH_INDEX].polygon_count,
                    (const PolygonDepthRecord *) object->shape_table,
                    (const Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
                    batches[VEHICLE_ROAD_RENDER_OPTIONAL_BATCH_INDEX].sort_keys,
                    g_VEHICLE_ROAD_ANIMATION_STATE.polygon_depth_sort_scratch);
                }
            }
        }
      resource->sort_refresh_countdown = VEHICLE_ROAD_RENDER_SORT_REFRESH_PERIOD;
    }

  saved_pixel_control = get_ccb_pixel_control();
  track_geometry_type =
    g_TRACK_SEGMENTS[state_->simulation_source.alternate_track_segment_index].road_geometry_type;
  if((track_geometry_type == VEHICLE_ROAD_RENDER_SPECIAL_TRACK_TYPE_A) ||
     (track_geometry_type == VEHICLE_ROAD_RENDER_SPECIAL_TRACK_TYPE_B) ||
     (track_geometry_type == VEHICLE_ROAD_RENDER_SPECIAL_TRACK_TYPE_C))
    {
      set_ccb_pixel_control(VEHICLE_ROAD_RENDER_SPECIAL_CCB_PIXEL_CONTROL);
    }

  if(detail_level >= VEHICLE_ROAD_RENDER_DETAIL_MINIMUM)
    {
      polygons = (const PolygonDepthRecord *) object->shape_table;
      if(detail_level == VEHICLE_ROAD_RENDER_DETAIL_HIGHEST)
        batches = resource->high_detail_batches;
      else
        batches = resource->medium_detail_batches;

      if((detail_level == VEHICLE_ROAD_RENDER_DETAIL_MINIMUM) || (batches[1].polygon_count <= 0))
        {
          render_unextended_projected_object_polygons(
            object->shape_count,
            polygons,
            (const Point2 *) g_SCREEN_COORDINATE_BUFFER,
            resource->sort_keys,
            (const Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
            (const uint8 *const *) state_->render_shape_pointers);
        }
      else if(detail_level == VEHICLE_ROAD_RENDER_DETAIL_HIGHEST)
        {
          if(0 < batches[0].polygon_count)
            {
              if(state_ == (const VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE)
                {
                  render_vehicle_road_collision_effects(state_, 1);
                }
              render_projected_object_polygons(
                batches[0].polygon_count,
                polygons,
                (const Point2 *) g_SCREEN_COORDINATE_BUFFER,
                batches[0].sort_keys,
                (const Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
                (const uint8 *const *) state_->render_shape_pointers);
            }
          render_projected_object_polygons(batches[1].polygon_count,
                                           polygons,
                                           (const Point2 *) g_SCREEN_COORDINATE_BUFFER,
                                           batches[1].sort_keys,
                                           (const Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
                                           (const uint8 *const *) state_->render_shape_pointers);
          if(0 < batches[VEHICLE_ROAD_RENDER_OPTIONAL_BATCH_INDEX].polygon_count)
            {
              render_projected_object_polygons(
                batches[VEHICLE_ROAD_RENDER_OPTIONAL_BATCH_INDEX].polygon_count,
                polygons,
                (const Point2 *) g_SCREEN_COORDINATE_BUFFER,
                batches[VEHICLE_ROAD_RENDER_OPTIONAL_BATCH_INDEX].sort_keys,
                (const Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
                (const uint8 *const *) state_->render_shape_pointers);
              if(state_ == (const VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE)
                {
                  render_vehicle_road_collision_effects(state_, 0);
                }
            }
        }
      else
        {
          if(0 < batches[0].polygon_count)
            {
              render_unextended_projected_object_polygons(
                batches[0].polygon_count,
                polygons,
                (const Point2 *) g_SCREEN_COORDINATE_BUFFER,
                batches[0].sort_keys,
                (const Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
                (const uint8 *const *) state_->render_shape_pointers);
            }
          render_unextended_projected_object_polygons(
            batches[1].polygon_count,
            polygons,
            (const Point2 *) g_SCREEN_COORDINATE_BUFFER,
            batches[1].sort_keys,
            (const Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
            (const uint8 *const *) state_->render_shape_pointers);
          if(0 < batches[VEHICLE_ROAD_RENDER_OPTIONAL_BATCH_INDEX].polygon_count)
            {
              render_unextended_projected_object_polygons(
                batches[VEHICLE_ROAD_RENDER_OPTIONAL_BATCH_INDEX].polygon_count,
                polygons,
                (const Point2 *) g_SCREEN_COORDINATE_BUFFER,
                batches[VEHICLE_ROAD_RENDER_OPTIONAL_BATCH_INDEX].sort_keys,
                (const Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
                (const uint8 *const *) state_->render_shape_pointers);
            }
        }
    }
  else
    {
      render_distant_vehicle_road_sprite(state_, resource, direction_);
    }
  set_ccb_pixel_control(saved_pixel_control);
  _render_vehicle_road_tire_smoke(state_, resource);
  return 1;
}

static int32
_truncate_vehicle_road_effect_value(int32 value_, int32 shift_, int32 negative_bias_)
{
  if(value_ < 0)
    value_ += negative_bias_;

  return value_ >> shift_;
}

static int32
_calculate_vehicle_road_effect_center_offset(int32 scale_, int32 dimension_log2_)
{
  int32 dimension;
  int32 integer_scale;
  int32 scaled_dimension;

  integer_scale =
    _truncate_vehicle_road_effect_value(scale_,
                                        VEHICLE_ROAD_RENDER_EFFECT_SCALE_SHIFT,
                                        VEHICLE_ROAD_RENDER_EFFECT_NEGATIVE_SCALE_BIAS);
  dimension = (int32) (1U << (uint32) dimension_log2_);
  scaled_dimension =
    (int32) ((uint32) (integer_scale * dimension) << VEHICLE_ROAD_RENDER_EFFECT_POSITION_SHIFT);
  return _truncate_vehicle_road_effect_value(scaled_dimension,
                                             VEHICLE_ROAD_RENDER_EFFECT_CENTER_SHIFT,
                                             VEHICLE_ROAD_RENDER_EFFECT_NEGATIVE_CENTER_BIAS);
}

static int32
_calculate_vehicle_road_effect_intensity(int32 value_)
{
  int32 intensity;
  int32 random_scale;

  random_scale = scale_by_random_fraction(VEHICLE_ROAD_RENDER_EFFECT_RANDOM_RANGE) +
                 VEHICLE_ROAD_RENDER_EFFECT_RANDOM_BIAS;
  intensity    = (int32) ((uint32) value_ * (uint32) random_scale);
  intensity =
    _truncate_vehicle_road_effect_value(intensity,
                                        VEHICLE_ROAD_RENDER_EFFECT_INTENSITY_SHIFT,
                                        VEHICLE_ROAD_RENDER_EFFECT_NEGATIVE_INTENSITY_BIAS);
  if(intensity < 1)
    intensity = 1;
  else if(intensity > VEHICLE_ROAD_RENDER_EFFECT_MAX_INTENSITY)
    intensity = VEHICLE_ROAD_RENDER_EFFECT_MAX_INTENSITY;

  return intensity;
}

static int32
_is_vehicle_road_effect_side_visible(int32 heading_, int32 layer_, int32 rear_side_)
{
  if(layer_ != 0)
    return 1;
  if(rear_side_ != 0)
    {
      return ((heading_ < -VEHICLE_ROAD_RENDER_EFFECT_HEADING_MINIMUM) &&
              (heading_ > -VEHICLE_ROAD_RENDER_EFFECT_HEADING_MAXIMUM));
    }

  return ((heading_ > VEHICLE_ROAD_RENDER_EFFECT_HEADING_MINIMUM) &&
          (heading_ < VEHICLE_ROAD_RENDER_EFFECT_HEADING_MAXIMUM));
}

static void
_render_vehicle_road_collision_effect(int32                            value_,
                                      int32                            horizontal_base_scale_,
                                      int32                            vertical_base_scale_,
                                      int32                            heading_,
                                      int32                            layer_,
                                      const VehicleRoadEffectGeometry *geometry_)
{
  const Point2 *screen_points;
  Point2        midpoint;
  int32         front_first_vertex_index;
  int32         front_second_vertex_index;
  int32         horizontal_scale;
  int32         intensity;
  int32         rear_first_vertex_index;
  int32         rear_second_vertex_index;
  int32         sum;
  int32         vertical_scale;

  if(value_ == 0)
    return;

  intensity = _calculate_vehicle_road_effect_intensity(value_);
  horizontal_scale =
    intensity * _truncate_vehicle_road_effect_value(horizontal_base_scale_,
                                                    VEHICLE_ROAD_RENDER_EFFECT_SCALE_SHIFT,
                                                    VEHICLE_ROAD_RENDER_EFFECT_NEGATIVE_SCALE_BIAS);
  vertical_scale =
    intensity * _truncate_vehicle_road_effect_value(vertical_base_scale_,
                                                    VEHICLE_ROAD_RENDER_EFFECT_SCALE_SHIFT,
                                                    VEHICLE_ROAD_RENDER_EFFECT_NEGATIVE_SCALE_BIAS);

  if(_is_vehicle_road_effect_side_visible(heading_, layer_, 0))
    {
      screen_points             = g_VEHICLE_ROAD_ANIMATION_STATE.quad_screen_points;
      front_first_vertex_index  =
        g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[geometry_->front_first_vertex_index];
      front_second_vertex_index =
        g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[geometry_->front_second_vertex_index];
      if(geometry_->uses_fixed_midpoint != 0)
        {
          sum = (int32) ((uint32) screen_points[front_first_vertex_index].x +
                         (uint32) screen_points[front_second_vertex_index].x);
          midpoint.x = sum >> VEHICLE_ROAD_RENDER_EFFECT_FIXED_MIDPOINT_SHIFT;
          sum = (int32) ((uint32) screen_points[front_first_vertex_index].y +
                         (uint32) screen_points[front_second_vertex_index].y);
          midpoint.y = sum >> VEHICLE_ROAD_RENDER_EFFECT_FIXED_MIDPOINT_SHIFT;
        }
      else
        {
          sum = (screen_points[front_first_vertex_index].x >>
                 VEHICLE_ROAD_RENDER_EFFECT_POSITION_SHIFT) +
                (screen_points[front_second_vertex_index].x >>
                 VEHICLE_ROAD_RENDER_EFFECT_POSITION_SHIFT);
          midpoint.x = _truncate_vehicle_road_effect_value(sum, 1, 1);
          sum = (screen_points[front_first_vertex_index].y >>
                 VEHICLE_ROAD_RENDER_EFFECT_POSITION_SHIFT) +
                (screen_points[front_second_vertex_index].y >>
                 VEHICLE_ROAD_RENDER_EFFECT_POSITION_SHIFT);
          midpoint.y = _truncate_vehicle_road_effect_value(sum, 1, 1);
        }
      render_vehicle_road_effect_sprite(midpoint.x, midpoint.y, horizontal_scale, vertical_scale);
    }

  if(_is_vehicle_road_effect_side_visible(heading_, layer_, 1))
    {
      screen_points            = g_VEHICLE_ROAD_ANIMATION_STATE.quad_screen_points;
      rear_first_vertex_index  =
        g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[geometry_->rear_first_vertex_index];
      rear_second_vertex_index =
        g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.values[geometry_->rear_second_vertex_index];
      if(geometry_->uses_fixed_midpoint != 0)
        {
          sum = (int32) ((uint32) screen_points[rear_first_vertex_index].x +
                         (uint32) screen_points[rear_second_vertex_index].x);
          midpoint.x = sum >> VEHICLE_ROAD_RENDER_EFFECT_FIXED_MIDPOINT_SHIFT;
          sum = (int32) ((uint32) screen_points[rear_first_vertex_index].y +
                         (uint32) screen_points[rear_second_vertex_index].y);
          midpoint.y = sum >> VEHICLE_ROAD_RENDER_EFFECT_FIXED_MIDPOINT_SHIFT;
        }
      else
        {
          sum = (screen_points[rear_first_vertex_index].x >>
                 VEHICLE_ROAD_RENDER_EFFECT_POSITION_SHIFT) +
                (screen_points[rear_second_vertex_index].x >>
                 VEHICLE_ROAD_RENDER_EFFECT_POSITION_SHIFT);
          midpoint.x = _truncate_vehicle_road_effect_value(sum, 1, 1);
          sum = (screen_points[rear_first_vertex_index].y >>
                 VEHICLE_ROAD_RENDER_EFFECT_POSITION_SHIFT) +
                (screen_points[rear_second_vertex_index].y >>
                 VEHICLE_ROAD_RENDER_EFFECT_POSITION_SHIFT);
          midpoint.y = _truncate_vehicle_road_effect_value(sum, 1, 1);
        }
      render_vehicle_road_effect_sprite(midpoint.x, midpoint.y, horizontal_scale, vertical_scale);
    }
}

static void
_render_vehicle_road_tire_smoke(VehicleSimulationState         *state_,
                                VehicleRoadRenderResourceState *resource_)
{
  VehicleRuntimeObjectState *runtime_object;
  int32                      detail_index;
  int32                      vertex_index;

  runtime_object = (VehicleRuntimeObjectState *) state_;
  if((runtime_object->status_flags & VEHICLE_ROAD_RENDER_TIRE_SMOKE_STATUS_FLAG) == 0)
    {
      return;
    }

  detail_index = VEHICLE_ROAD_RENDER_DETAIL_HIGHEST - state_->render_detail_level;
  vertex_index = resource_->tire_smoke_vertex_indices[detail_index];
  if(vertex_index < 0)
    {
      g_VEHICLE_ROAD_ANIMATION_STATE.tire_smoke_screen_offset.x = 0;
      g_VEHICLE_ROAD_ANIMATION_STATE.tire_smoke_screen_offset.y = 0;
    }
  else
    {
      transform_project_vector3_array(1,
                                      &g_VEHICLE_ROAD_ANIMATION_STATE.vehicle_model_view_matrix,
                                      &g_VEHICLE_ROAD_ANIMATION_STATE.low_detail_projection_vertex,
                                      &state_->simulation_source.road_animation_view_position,
                                      &g_VEHICLE_ROAD_ANIMATION_STATE.tire_smoke_view_position,
                                      &g_VEHICLE_ROAD_ANIMATION_STATE.tire_smoke_screen_offset,
                                      0);
      g_VEHICLE_ROAD_ANIMATION_STATE.tire_smoke_screen_offset.x =
        (g_VEHICLE_ROAD_ANIMATION_STATE.quad_screen_points[vertex_index].x -
         g_VEHICLE_ROAD_ANIMATION_STATE.tire_smoke_screen_offset.x);
      g_VEHICLE_ROAD_ANIMATION_STATE.tire_smoke_screen_offset.y =
        (g_VEHICLE_ROAD_ANIMATION_STATE.quad_screen_points[vertex_index].y -
         g_VEHICLE_ROAD_ANIMATION_STATE.tire_smoke_screen_offset.y);
    }

  render_tire_smoke_particles(&g_TIRE_SMOKE_PARTICLES,
                              &g_VEHICLE_ROAD_ANIMATION_STATE.tire_smoke_screen_offset);
}
