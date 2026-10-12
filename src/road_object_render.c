/* Renders projected point-mesh road objects from their shape resources. */

#include "ccb_manager.h"
#include "frontend_render_state.h"
#include "geometry.h"
#include "line_extension.h"
#include "quad_mapping.h"
#include "radix_sort.h"
#include "render_bounds.h"
#include "simulation_resource_cleanup.h"
#include "simulation_snapshot.h"
#include "vehicle_render_resource_selection.h"

#define ROAD_OBJECT_RENDER_POSITION_SHIFT 12
#define ROAD_OBJECT_RENDER_HEADING_SHIFT 14
#define ROAD_OBJECT_RENDER_MINIMUM_DEPTH 1
#define ROAD_OBJECT_RENDER_PRIMARY_DEPTH_LIMIT 40
#define ROAD_OBJECT_RENDER_SORT_DEPTH_THRESHOLD 20
#define ROAD_OBJECT_RENDER_DEPTH_KEY_SHIFT 16
#define ROAD_OBJECT_RENDER_POLYGON_FLAG_VISIBLE 1
#define ROAD_OBJECT_RENDER_POLYGON_FLAG_REVERSE_WINDING 2
#define ROAD_OBJECT_RENDER_TRIANGLE_SHIFT 16
#define ROAD_OBJECT_RENDER_SHAPE_WIDTH_SHIFT 28
#define ROAD_OBJECT_RENDER_SHAPE_HEIGHT_SHIFT 12
#define ROAD_OBJECT_RENDER_SHAPE_DIMENSION_MASK 0x0F
#define ROAD_OBJECT_RENDER_POLYGON_FIRST_VERTEX 0
#define ROAD_OBJECT_RENDER_POLYGON_SECOND_VERTEX 1
#define ROAD_OBJECT_RENDER_POLYGON_THIRD_VERTEX 2
#define ROAD_OBJECT_RENDER_POLYGON_FOURTH_VERTEX 3
#define ROAD_OBJECT_RENDER_POLYGON_VERTEX_COUNT 4

typedef struct RoadObjectPolygon
{
  uint32 flags;
  int32  reserved_04;
  int32  vertex_indices[ROAD_OBJECT_RENDER_POLYGON_VERTEX_COUNT];
} RoadObjectPolygon;

int32
is_projected_road_object_polygon_visible(const Point2            *projected_,
                                         const RoadObjectPolygon *polygon_,
                                         int32                    reverse_winding_);

int32
prepare_road_object_resource_render(const RenderResourceRecord *resource_,
                                    const Vector3              *position_,
                                    int32                       heading_,
                                    int32                      *object_index_);

void
render_projected_object_polygons(int32               polygon_count_,
                                 const void         *polygons_,
                                 const Point2       *projected_,
                                 const uint32       *sort_order_,
                                 const Vector3      *transformed_,
                                 const uint8 *const *shape_pointers_)
{
  const uint32            *shape_header;
  const RoadObjectPolygon *polygon;
  const RoadObjectPolygon *polygons;
  Point2                   quad[ROAD_OBJECT_RENDER_POLYGON_VERTEX_COUNT];
  RenderCcb               *ccb;
  uint32                   sort_key;
  int32                    polygon_index;
  int32                    polygon_offset;
  int32                    shape_height;
  int32                    shape_width;

  (void) transformed_;
  polygons = (const RoadObjectPolygon *) polygons_;
  for(polygon_offset = 0; polygon_offset < polygon_count_; polygon_offset++)
    {
      sort_key      = sort_order_[polygon_offset];
      polygon_index = (int32) (sort_key >> ROAD_OBJECT_RENDER_DEPTH_KEY_SHIFT);
      polygon       = &polygons[polygon_index];
      if((g_PRIMARY_RENDER_DEPTH_LIMIT < (int32) (int16) sort_key) &&
         (((polygon->flags & ROAD_OBJECT_RENDER_POLYGON_FLAG_VISIBLE) != 0) ||
          (is_projected_road_object_polygon_visible(
             projected_,
             polygon,
             (int32) (polygon->flags & ROAD_OBJECT_RENDER_POLYGON_FLAG_REVERSE_WINDING)) != 0)))
        {
          shape_header = (const uint32 *) shape_pointers_[polygon_index];
          ccb          = allocate_cel_header_ccb(shape_header, 0, 0);
          extend_fixed16_line_segment(
            &projected_[polygon->vertex_indices[ROAD_OBJECT_RENDER_POLYGON_FIRST_VERTEX]],
            &projected_[polygon->vertex_indices[ROAD_OBJECT_RENDER_POLYGON_THIRD_VERTEX]],
            &quad[ROAD_OBJECT_RENDER_POLYGON_FIRST_VERTEX],
            &quad[ROAD_OBJECT_RENDER_POLYGON_THIRD_VERTEX]);
          extend_fixed16_line_segment(
            &projected_[polygon->vertex_indices[ROAD_OBJECT_RENDER_POLYGON_SECOND_VERTEX]],
            &projected_[polygon->vertex_indices[ROAD_OBJECT_RENDER_POLYGON_FOURTH_VERTEX]],
            &quad[ROAD_OBJECT_RENDER_POLYGON_SECOND_VERTEX],
            &quad[ROAD_OBJECT_RENDER_POLYGON_FOURTH_VERTEX]);
          shape_width  = (int32) (shape_header[1] >> ROAD_OBJECT_RENDER_SHAPE_WIDTH_SHIFT);
          shape_height = (int32) ((shape_header[1] >> ROAD_OBJECT_RENDER_SHAPE_HEIGHT_SHIFT) &
                                  ROAD_OBJECT_RENDER_SHAPE_DIMENSION_MASK);
          map_fixed16_quad_to_ccb(&ccb->fields, quad, shape_width, shape_height);
        }
    }
}

void
render_unextended_projected_object_polygons(int32               polygon_count_,
                                            const void         *polygons_,
                                            const Point2       *projected_,
                                            const uint32       *sort_order_,
                                            const Vector3      *transformed_,
                                            const uint8 *const *shape_pointers_)
{
  const uint32            *shape_header;
  const RoadObjectPolygon *polygon;
  const RoadObjectPolygon *polygons;
  Point2                   quad[ROAD_OBJECT_RENDER_POLYGON_VERTEX_COUNT];
  RenderCcb               *ccb;
  uint32                   sort_key;
  int32                    polygon_index;
  int32                    polygon_offset;
  int32                    shape_height;
  int32                    shape_width;

  (void) transformed_;
  polygons = (const RoadObjectPolygon *) polygons_;
  for(polygon_offset = 0; polygon_offset < polygon_count_; polygon_offset++)
    {
      sort_key      = sort_order_[polygon_offset];
      polygon_index = (int32) (sort_key >> ROAD_OBJECT_RENDER_DEPTH_KEY_SHIFT);
      polygon       = &polygons[polygon_index];
      if((g_PRIMARY_RENDER_DEPTH_LIMIT < (int32) (int16) sort_key) &&
         (((polygon->flags & ROAD_OBJECT_RENDER_POLYGON_FLAG_VISIBLE) != 0) ||
          (is_projected_road_object_polygon_visible(
             projected_,
             polygon,
             (int32) (polygon->flags & ROAD_OBJECT_RENDER_POLYGON_FLAG_REVERSE_WINDING)) != 0)))
        {
          shape_header = (const uint32 *) shape_pointers_[polygon_index];
          ccb          = allocate_cel_header_ccb(shape_header, 0, 0);
          quad[ROAD_OBJECT_RENDER_POLYGON_FIRST_VERTEX] =
            projected_[polygon->vertex_indices[ROAD_OBJECT_RENDER_POLYGON_FIRST_VERTEX]];
          quad[ROAD_OBJECT_RENDER_POLYGON_SECOND_VERTEX] =
            projected_[polygon->vertex_indices[ROAD_OBJECT_RENDER_POLYGON_SECOND_VERTEX]];
          quad[ROAD_OBJECT_RENDER_POLYGON_THIRD_VERTEX] =
            projected_[polygon->vertex_indices[ROAD_OBJECT_RENDER_POLYGON_THIRD_VERTEX]];
          quad[ROAD_OBJECT_RENDER_POLYGON_FOURTH_VERTEX] =
            projected_[polygon->vertex_indices[ROAD_OBJECT_RENDER_POLYGON_FOURTH_VERTEX]];
          shape_width  = (int32) (shape_header[1] >> ROAD_OBJECT_RENDER_SHAPE_WIDTH_SHIFT);
          shape_height = (int32) ((shape_header[1] >> ROAD_OBJECT_RENDER_SHAPE_HEIGHT_SHIFT) &
                                  ROAD_OBJECT_RENDER_SHAPE_DIMENSION_MASK);
          map_fixed16_quad_to_ccb(&ccb->fields, quad, shape_width, shape_height);
        }
    }
}

int32
render_road_object_resource(const RenderResourceRecord *resource_,
                            const Vector3              *position_,
                            int32                       heading_)
{
  const ThreeDObject *object;
  int32               object_index;

  if(prepare_road_object_resource_render(resource_, position_, heading_, &object_index) == 0)
    {
      return 0;
    }

  g_PRIMARY_RENDER_DEPTH_LIMIT   = ROAD_OBJECT_RENDER_PRIMARY_DEPTH_LIMIT;
  g_POLYGON_DEPTH_SORT_THRESHOLD = ROAD_OBJECT_RENDER_SORT_DEPTH_THRESHOLD;
  object                         = resource_->objects[object_index];
  build_sorted_polygon_depth_keys(object->shape_count,
                                  (const PolygonDepthRecord *) object->shape_table,
                                  (const Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
                                  (uint32 *) g_RENDER_SORT_ORDER_BUFFER,
                                  (uint32 *) g_RENDER_DISTANCE_BUFFER);
  render_projected_object_polygons(
    object->shape_count,
    (const RoadObjectPolygon *) object->shape_table,
    (const Point2 *) g_SCREEN_COORDINATE_BUFFER,
    (const uint32 *) g_RENDER_SORT_ORDER_BUFFER,
    (const Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER,
    (const uint8 *const *) resource_->shape_pointer_allocations[object_index]);
  return 1;
}

int32
is_projected_road_object_polygon_visible(const Point2            *projected_,
                                         const RoadObjectPolygon *polygon_,
                                         int32                    reverse_winding_)
{
  int32 first_cross_product;
  int32 second_cross_product;

  first_cross_product = calculate_triangle_cross_product(
    &projected_[polygon_->vertex_indices[ROAD_OBJECT_RENDER_POLYGON_FIRST_VERTEX]],
    &projected_[polygon_->vertex_indices[ROAD_OBJECT_RENDER_POLYGON_SECOND_VERTEX]],
    &projected_[polygon_->vertex_indices[ROAD_OBJECT_RENDER_POLYGON_THIRD_VERTEX]],
    ROAD_OBJECT_RENDER_TRIANGLE_SHIFT);
  second_cross_product = calculate_triangle_cross_product(
    &projected_[polygon_->vertex_indices[ROAD_OBJECT_RENDER_POLYGON_THIRD_VERTEX]],
    &projected_[polygon_->vertex_indices[ROAD_OBJECT_RENDER_POLYGON_FOURTH_VERTEX]],
    &projected_[polygon_->vertex_indices[ROAD_OBJECT_RENDER_POLYGON_FIRST_VERTEX]],
    ROAD_OBJECT_RENDER_TRIANGLE_SHIFT);
  if(reverse_winding_ == 0)
    return (first_cross_product > 0) || (second_cross_product > 0);

  return (first_cross_product < 0) || (second_cross_product < 0);
}

int32
prepare_road_object_resource_render(const RenderResourceRecord *resource_,
                                    const Vector3              *position_,
                                    int32                       heading_,
                                    int32                      *object_index_)
{
  const SimulationRenderState *render_state;
  const Matrix3x3             *view_matrix;
  const ThreeDObject          *object;
  Matrix3x3                    model_matrix;
  Matrix3x3                    rotation_matrix;
  Vector3                      relative_position;
  Vector3                      view_position;
  int32                        object_index;

  initialize_matrix3x3_rotation_y_1024(&rotation_matrix,
                                       heading_ >> ROAD_OBJECT_RENDER_HEADING_SHIFT);
  render_state = (const SimulationRenderState *) g_FRONTEND_RENDER_STATE.projection.source;
  relative_position.x =
    (position_->x - render_state->render_position.x) >> ROAD_OBJECT_RENDER_POSITION_SHIFT;
  relative_position.y =
    (position_->y - render_state->render_position.y) >> ROAD_OBJECT_RENDER_POSITION_SHIFT;
  relative_position.z =
    (position_->z - render_state->render_position.z) >> ROAD_OBJECT_RENDER_POSITION_SHIFT;
  view_matrix = (const Matrix3x3 *) g_FRONTEND_RENDER_STATE.projection.view_state;
  multiply_vector3_matrix3x3(&relative_position, view_matrix, &view_position);
  if((view_position.z < ROAD_OBJECT_RENDER_MINIMUM_DEPTH) ||
     (g_OBJECT_RENDER_DISTANCE_THRESHOLDS[0] < view_position.z))
    {
      return 0;
    }

  object_index = 0;
  for(object_index = OBJECT_RENDER_DISTANCE_THRESHOLD_COUNT - 1; object_index >= 0; object_index--)
    {
      if(view_position.z <= g_OBJECT_RENDER_DISTANCE_THRESHOLDS[object_index])
        {
          *object_index_ = object_index;
          break;
        }
    }

  multiply_matrix_3x3(&rotation_matrix, view_matrix, &model_matrix);
  object = resource_->objects[*object_index_];
  transform_project_vector3_array(object->material_count,
                                  &model_matrix,
                                  &view_position,
                                  (const Vector3 *) object->material_table,
                                  g_VEHICLE_ROAD_ANIMATION_STATE.quad_view_points,
                                  g_VEHICLE_ROAD_ANIMATION_STATE.quad_screen_points,
                                  0);
  return 1;
}
