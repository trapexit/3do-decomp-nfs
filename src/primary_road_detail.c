/* Renders raised road-detail cels from primary mesh vertices. */

#include "frontend_render_state.h"
#include "render_thread_signal.h"
#include "road_data.h"
#include "road_render.h"

#define PRIMARY_ROAD_DETAIL_FRAME_MASK 3
#define PRIMARY_ROAD_DETAIL_FRAME_BYTES 2
#define PRIMARY_ROAD_DETAIL_FIRST_INDEX_OFFSET 8
#define PRIMARY_ROAD_DETAIL_SECOND_INDEX_OFFSET 16
#define PRIMARY_ROAD_DETAIL_MIDPOINT_DIVISOR 2
#define PRIMARY_ROAD_DETAIL_NEAR_DEPTH 0xC0
#define PRIMARY_ROAD_DETAIL_SCREEN_X_MAXIMUM 0x1400000
#define PRIMARY_ROAD_DETAIL_SHORT_HEIGHT 0xE666
#define PRIMARY_ROAD_DETAIL_TALL_HEIGHT 0x1CCCC
#define PRIMARY_ROAD_DETAIL_UPPER_CEL_OFFSET 8
#define PRIMARY_ROAD_DETAIL_LOWER_CEL_OFFSET 4
#define PRIMARY_ROAD_DETAIL_QUAD_POINT_COUNT 4
#define PRIMARY_ROAD_DETAIL_FIRST_QUAD_POINT 0
#define PRIMARY_ROAD_DETAIL_SECOND_QUAD_POINT 1
#define PRIMARY_ROAD_DETAIL_THIRD_QUAD_POINT 2
#define PRIMARY_ROAD_DETAIL_FOURTH_QUAD_POINT 3

void
render_primary_road_detail(const RoadGeometryModel *model_,
                           uint32                   frame_index_,
                           int32                    mesh_vertex_index_,
                           int32                    cel_index_,
                           int32                    cel_group_index_,
                           int32                    use_second_indices_,
                           int32                    tall_detail_)
{
  const RoadCelTemplate *const *cel_templates;
  const uint8                  *model_bytes;
  const uint8                  *vertex_indices;
  int32                         height;
  int32                         first_vertex_index;
  int32                         middle_was_clipped;
  int32                         second_vertex_index;
  Point2                        detail_quad[PRIMARY_ROAD_DETAIL_QUAD_POINT_COUNT];
  Point2                        first_offset_screen;
  Point2                        middle_base_screen;
  Point2                        middle_offset_screen;
  Point2                        second_offset_screen;
  Point2                        first_screen;
  Point2                        second_screen;
  Vector3                       first_offset_vertex;
  Vector3                       middle_base_vertex;
  Vector3                       middle_offset_vertex;
  Vector3                       second_offset_vertex;

  model_bytes    = (const uint8 *) model_;
  vertex_indices = model_bytes + ((frame_index_ & PRIMARY_ROAD_DETAIL_FRAME_MASK) *
                                  PRIMARY_ROAD_DETAIL_FRAME_BYTES);
  if(use_second_indices_ == 0)
    vertex_indices += PRIMARY_ROAD_DETAIL_FIRST_INDEX_OFFSET;
  else
    vertex_indices += PRIMARY_ROAD_DETAIL_SECOND_INDEX_OFFSET;

  first_vertex_index  = (int32) vertex_indices[0];
  second_vertex_index = (int32) vertex_indices[1];
  first_screen  = g_ROAD_MESH_RENDER_WORKSPACE.projected[mesh_vertex_index_ + first_vertex_index];
  second_screen = g_ROAD_MESH_RENDER_WORKSPACE.projected[mesh_vertex_index_ + second_vertex_index];
  if(g_ROAD_MESH_RENDER_WORKSPACE.transformed[mesh_vertex_index_ + second_vertex_index].z <
     g_FRONTEND_RENDER_STATE.projection.projection->projection_scale)
    return;

  height = PRIMARY_ROAD_DETAIL_SHORT_HEIGHT;
  if(tall_detail_ != 0)
    height = PRIMARY_ROAD_DETAIL_TALL_HEIGHT;

  first_offset_vertex =
    g_ROAD_MESH_RENDER_WORKSPACE.vertices[mesh_vertex_index_ + first_vertex_index];
  second_offset_vertex =
    g_ROAD_MESH_RENDER_WORKSPACE.vertices[mesh_vertex_index_ + second_vertex_index];
  first_offset_vertex.y += height;
  second_offset_vertex.y += height;

  cel_templates =
    ((const RoadCelTemplate *const *const *)
       g_RENDER_THREAD_STATE.road_resource_packet_zero)[cel_group_index_];
  if(g_ROAD_MESH_RENDER_WORKSPACE.transformed[mesh_vertex_index_ + first_vertex_index].z <
     PRIMARY_ROAD_DETAIL_NEAR_DEPTH)
    {
      if((second_screen.x > PRIMARY_ROAD_DETAIL_SCREEN_X_MAXIMUM) || (second_screen.x < 0))
        return;

      middle_base_vertex.x =
        (first_offset_vertex.x + second_offset_vertex.x) / PRIMARY_ROAD_DETAIL_MIDPOINT_DIVISOR;
      middle_base_vertex.y =
        ((first_offset_vertex.y + second_offset_vertex.y) / PRIMARY_ROAD_DETAIL_MIDPOINT_DIVISOR) -
        height;
      middle_base_vertex.z =
        (first_offset_vertex.z + second_offset_vertex.z) / PRIMARY_ROAD_DETAIL_MIDPOINT_DIVISOR;
      middle_offset_vertex = middle_base_vertex;
      middle_offset_vertex.y += height;

      middle_was_clipped =
        project_world_point_to_screen_with_near_clip(&middle_base_vertex, &middle_base_screen);
      (void) project_world_point_to_screen_with_near_clip(&second_offset_vertex,
                                                          &second_offset_screen);
      (void) project_world_point_to_screen_with_near_clip(&middle_offset_vertex,
                                                          &middle_offset_screen);
      render_road_quad_corners(cel_templates[cel_index_ + PRIMARY_ROAD_DETAIL_UPPER_CEL_OFFSET],
                               &second_offset_screen,
                               &middle_offset_screen,
                               &middle_base_screen,
                               &second_screen);
      if(middle_was_clipped != 0)
        return;

      (void) project_world_point_to_screen_with_near_clip(&first_offset_vertex,
                                                          &first_offset_screen);
      render_road_quad_corners(cel_templates[cel_index_ + PRIMARY_ROAD_DETAIL_LOWER_CEL_OFFSET],
                               &middle_offset_screen,
                               &first_offset_screen,
                               &first_screen,
                               &middle_base_screen);
      return;
    }

  if(project_world_point_to_screen(&second_offset_vertex, &second_offset_screen) != 0)
    return;
  if(project_world_point_to_screen(&first_offset_vertex, &first_offset_screen) != 0)
    return;

  detail_quad[PRIMARY_ROAD_DETAIL_FIRST_QUAD_POINT]  = second_offset_screen;
  detail_quad[PRIMARY_ROAD_DETAIL_SECOND_QUAD_POINT] = first_offset_screen;
  detail_quad[PRIMARY_ROAD_DETAIL_THIRD_QUAD_POINT]  = first_screen;
  detail_quad[PRIMARY_ROAD_DETAIL_FOURTH_QUAD_POINT] = second_screen;
  render_secondary_road_quad(cel_templates[cel_index_], detail_quad);
}
