/* Prepares the projected vehicle surface used by road-animation rendering. */

#include "ccb_manager.h"
#include "frontend_render_state.h"
#include "quad_mapping.h"
#include "quad_render.h"
#include "render_bounds.h"
#include "simulation_snapshot.h"
#include "vehicle_render_resource_selection.h"

#include "operamath.h"

#define VEHICLE_ROAD_ANIMATION_TRACK_RANGE 0x2A
#define VEHICLE_ROAD_ANIMATION_POSITION_SHIFT 9
#define VEHICLE_ROAD_ANIMATION_NEAR_DEPTH_OFFSET 10
#define VEHICLE_ROAD_ANIMATION_FAR_DEPTH_LIMIT 0x1680
#define VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT 10
#define VEHICLE_ROAD_ANIMATION_QUAD_POINT_COUNT 4
#define VEHICLE_ROAD_ANIMATION_QUAD_RESERVED_BYTES 0x30
#define VEHICLE_ROAD_ANIMATION_SOLID_COLOR 1
#define VEHICLE_ROAD_ANIMATION_CCB_PIXEL_CONTROL 0x1F811F81

enum
{
  VEHICLE_ROAD_ANIMATION_TOP_LEFT_INDEX     = 0,
  VEHICLE_ROAD_ANIMATION_TOP_RIGHT_INDEX    = 1,
  VEHICLE_ROAD_ANIMATION_BOTTOM_RIGHT_INDEX = 2,
  VEHICLE_ROAD_ANIMATION_BOTTOM_LEFT_INDEX  = 3
};

Vector3 g_VEHICLE_ROAD_SURFACE_QUAD_POINTS[VEHICLE_ROAD_ANIMATION_QUAD_POINT_COUNT];
uint8   g_VEHICLE_ROAD_SURFACE_QUAD_RESERVED[VEHICLE_ROAD_ANIMATION_QUAD_RESERVED_BYTES];

static int32
_is_vehicle_road_animation_depth_visible(int32 vehicle_depth_, int32 surface_depth_);

int32
prepare_vehicle_road_projection(VehicleSimulationState *state_, int32 direction_)
{
  const Matrix3x3       *view_matrix;
  SimulationRenderState *render_state;
  SimulationSource      *source;
  Vector3                camera_relative_position;
  Vector3                surface_view_position;
  int32                  track_distance;
  int32                  surface_depth;

  source       = &state_->simulation_source;
  render_state = (SimulationRenderState *) g_FRONTEND_RENDER_STATE.projection.source;
  track_distance =
    source->alternate_track_segment_index - (int32) render_state->render_track_segment_index;
  if(track_distance < 0)
    track_distance = -track_distance;
  if(track_distance > VEHICLE_ROAD_ANIMATION_TRACK_RANGE)
    return 0;

  camera_relative_position.x =
    (source->alternate_surface_origin.x - render_state->render_position.x) >>
    VEHICLE_ROAD_ANIMATION_POSITION_SHIFT;
  camera_relative_position.y =
    (source->alternate_surface_origin.y - render_state->render_position.y) >>
    VEHICLE_ROAD_ANIMATION_POSITION_SHIFT;
  camera_relative_position.z =
    (source->alternate_surface_origin.z - render_state->render_position.z) >>
    VEHICLE_ROAD_ANIMATION_POSITION_SHIFT;
  view_matrix = (const Matrix3x3 *) g_FRONTEND_RENDER_STATE.projection.view_state;
  multiply_vector3_matrix3x3(&camera_relative_position,
                             view_matrix,
                             &source->road_animation_view_position);

  if(direction_ != 0)
    return 0;

  surface_depth = source->road_animation_view_position.z;
  if(_is_vehicle_road_animation_depth_visible(
       (source->longitudinal_axle_span >> VEHICLE_ROAD_ANIMATION_POSITION_SHIFT) +
         VEHICLE_ROAD_ANIMATION_NEAR_DEPTH_OFFSET,
       surface_depth) == 0)
    {
      return 0;
    }

  project_vehicle_surface_vectors(source, 1);
  render_state = (SimulationRenderState *) g_FRONTEND_RENDER_STATE.projection.source;
  camera_relative_position.x =
    (source->alternate_surface_projection_origin.x - render_state->render_position.x) >>
    VEHICLE_ROAD_ANIMATION_POSITION_SHIFT;
  camera_relative_position.y =
    (source->alternate_surface_projection_origin.y - render_state->render_position.y) >>
    VEHICLE_ROAD_ANIMATION_POSITION_SHIFT;
  camera_relative_position.z =
    (source->alternate_surface_projection_origin.z - render_state->render_position.z) >>
    VEHICLE_ROAD_ANIMATION_POSITION_SHIFT;
  view_matrix = (const Matrix3x3 *) g_FRONTEND_RENDER_STATE.projection.view_state;
  multiply_vector3_matrix3x3(&camera_relative_position, view_matrix, &surface_view_position);

  g_VEHICLE_ROAD_SURFACE_QUAD_POINTS[VEHICLE_ROAD_ANIMATION_TOP_LEFT_INDEX].x =
    -(source->surface_longitudinal_vector.x >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT) +
    (source->surface_lateral_vector.x >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT);
  g_VEHICLE_ROAD_SURFACE_QUAD_POINTS[VEHICLE_ROAD_ANIMATION_TOP_LEFT_INDEX].y =
    -(source->surface_longitudinal_vector.y >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT) +
    (source->surface_lateral_vector.y >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT);
  g_VEHICLE_ROAD_SURFACE_QUAD_POINTS[VEHICLE_ROAD_ANIMATION_TOP_LEFT_INDEX].z =
    -(source->surface_longitudinal_vector.z >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT) +
    (source->surface_lateral_vector.z >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT);
  g_VEHICLE_ROAD_SURFACE_QUAD_POINTS[VEHICLE_ROAD_ANIMATION_TOP_RIGHT_INDEX].x =
    -(source->surface_longitudinal_vector.x >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT) -
    (source->surface_lateral_vector.x >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT);
  g_VEHICLE_ROAD_SURFACE_QUAD_POINTS[VEHICLE_ROAD_ANIMATION_TOP_RIGHT_INDEX].y =
    -(source->surface_longitudinal_vector.y >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT) -
    (source->surface_lateral_vector.y >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT);
  g_VEHICLE_ROAD_SURFACE_QUAD_POINTS[VEHICLE_ROAD_ANIMATION_TOP_RIGHT_INDEX].z =
    -(source->surface_longitudinal_vector.z >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT) -
    (source->surface_lateral_vector.z >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT);
  g_VEHICLE_ROAD_SURFACE_QUAD_POINTS[VEHICLE_ROAD_ANIMATION_BOTTOM_RIGHT_INDEX].x =
    (source->surface_longitudinal_vector.x >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT) -
    (source->surface_lateral_vector.x >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT);
  g_VEHICLE_ROAD_SURFACE_QUAD_POINTS[VEHICLE_ROAD_ANIMATION_BOTTOM_RIGHT_INDEX].y =
    (source->surface_longitudinal_vector.y >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT) -
    (source->surface_lateral_vector.y >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT);
  g_VEHICLE_ROAD_SURFACE_QUAD_POINTS[VEHICLE_ROAD_ANIMATION_BOTTOM_RIGHT_INDEX].z =
    (source->surface_longitudinal_vector.z >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT) -
    (source->surface_lateral_vector.z >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT);
  g_VEHICLE_ROAD_SURFACE_QUAD_POINTS[VEHICLE_ROAD_ANIMATION_BOTTOM_LEFT_INDEX].x =
    (source->surface_longitudinal_vector.x >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT) +
    (source->surface_lateral_vector.x >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT);
  g_VEHICLE_ROAD_SURFACE_QUAD_POINTS[VEHICLE_ROAD_ANIMATION_BOTTOM_LEFT_INDEX].y =
    (source->surface_longitudinal_vector.y >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT) +
    (source->surface_lateral_vector.y >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT);
  g_VEHICLE_ROAD_SURFACE_QUAD_POINTS[VEHICLE_ROAD_ANIMATION_BOTTOM_LEFT_INDEX].z =
    (source->surface_longitudinal_vector.z >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT) +
    (source->surface_lateral_vector.z >> VEHICLE_ROAD_ANIMATION_SURFACE_VECTOR_SHIFT);
  view_matrix = (const Matrix3x3 *) g_FRONTEND_RENDER_STATE.projection.view_state;
  transform_project_vector3_array(VEHICLE_ROAD_ANIMATION_QUAD_POINT_COUNT,
                                  view_matrix,
                                  &surface_view_position,
                                  g_VEHICLE_ROAD_SURFACE_QUAD_POINTS,
                                  g_VEHICLE_ROAD_ANIMATION_STATE.quad_view_points,
                                  g_VEHICLE_ROAD_ANIMATION_STATE.quad_screen_points,
                                  0);
  return 1;
}

int32
prepare_vehicle_road_animation(VehicleSimulationState *state_, int32 direction_)
{
  RenderCcb        *ccb;
  SimulationSource *source;

  source = &state_->simulation_source;
  if(source->alternate_heading_refresh_pending != 0)
    {
      source->alternate_heading =
        Atan2F16(source->alternate_heading_reference_z, source->alternate_heading_reference_x);
    }

  if(prepare_vehicle_road_projection(state_, direction_) == 0)
    return 0;

  ccb = g_CCB_MANAGER_STATE.allocation_cursor;
  (void) draw_solid_quad_points(VEHICLE_ROAD_ANIMATION_SOLID_COLOR,
                                g_VEHICLE_ROAD_ANIMATION_STATE.quad_screen_points);
  map_fixed16_quad_to_ccb(&ccb->fields, g_VEHICLE_ROAD_ANIMATION_STATE.quad_screen_points, 0, 0);
  ccb->fields.pixel_control = VEHICLE_ROAD_ANIMATION_CCB_PIXEL_CONTROL;
  return 1;
}

static int32
_is_vehicle_road_animation_depth_visible(int32 vehicle_depth_, int32 surface_depth_)
{
  if((vehicle_depth_ <= surface_depth_) &&
     (surface_depth_ <= VEHICLE_ROAD_ANIMATION_FAR_DEPTH_LIMIT))
    {
      return 1;
    }

  return 0;
}
