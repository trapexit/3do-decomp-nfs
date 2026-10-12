/* Renders primary road cels and advances their object-animation range. */

#include "frontend_animation_steps.h"
#include "render_thread_signal.h"
#include "road_data.h"
#include "road_object_table.h"
#include "road_render.h"
#include "track_position.h"
#include "track_processing_cache.h"

#define PRIMARY_ROAD_SECTION_SEGMENT_COUNT 4
#define PRIMARY_ROAD_SECTION_SEGMENT_MASK 3
#define PRIMARY_ROAD_SECTION_ALIGNMENT_MASK -4
#define PRIMARY_ROAD_MESH_VERTEX_LOOKBEHIND 11
#define PRIMARY_ROAD_DETAIL_CEL_GROUP_BASE 0x20
#define PRIMARY_ROAD_DETAIL_CEL_GROUP_MASK 0x1F
#define PRIMARY_ROAD_DETAIL_TALL_FLAG 0x20
#define PRIMARY_ROAD_DETAIL_FIRST_FLAG 0x40
#define PRIMARY_ROAD_DETAIL_SECOND_FLAG 0x80
#define PRIMARY_ROAD_DESCRIPTOR_GROUP_OFFSET 2
#define PRIMARY_ROAD_QUAD_FIRST_POINT_INDEX 0
#define PRIMARY_ROAD_QUAD_SECOND_POINT_INDEX 1
#define PRIMARY_ROAD_QUAD_THIRD_POINT_INDEX 2
#define PRIMARY_ROAD_QUAD_FOURTH_POINT_INDEX 3
#define PRIMARY_ROAD_CEL_FRAME_INDEX_ADJUSTMENT 3

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

typedef struct PrimaryRoadGeometryModel
{
  int32 mesh_vertex_count;
  int32 quad_count;
  uint8 reserved_08[0x40];
  int32 quad_start_index;
} PrimaryRoadGeometryModel;

typedef struct PrimaryRoadQuadDefinition
{
  uint8 point_indices[4];
  uint8 render_type;
  uint8 reserved_05;
  uint8 cel_index;
  uint8 animation_step_count;
} PrimaryRoadQuadDefinition;

COMPILE_TIME_SIZE_ASSERT(primary_road_geometry_model_size,
                         sizeof(PrimaryRoadGeometryModel) == sizeof(RoadGeometryModel));
COMPILE_TIME_SIZE_ASSERT(primary_road_quad_definition_size, sizeof(PrimaryRoadQuadDefinition) == 8);
COMPILE_TIME_SIZE_ASSERT(primary_road_cel_frame_offset_size, sizeof(uint8) == 1);

uint8 g_PRIMARY_ROAD_CEL_FRAME_OFFSETS[PRIMARY_ROAD_CEL_FRAME_OFFSET_COUNT] = {
  3, 3, 3, 3, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
};

COMPILE_TIME_SIZE_ASSERT(primary_road_cel_frame_offsets_size,
                         sizeof(g_PRIMARY_ROAD_CEL_FRAME_OFFSETS) == 0x78);

void
render_primary_road_range(int32 section_count_, int32 track_section_position_, int32 section_limit_)
{
  const PrimaryRoadGeometryModel      *model;
  const PrimaryRoadQuadDefinition     *quad;
  const RoadCelTemplate *const *const *cel_groups;
  const uint8                         *road_slice;
  Point2                               points[ROAD_RENDER_REFERENCE_POINT_COUNT];
  int32                                animation_step_count;
  int32                                cel_frame_index;
  int32                                cel_frame_offset;
  int32                                detail_cel_group_index;
  int32                                mesh_vertex_index;
  int32                                quad_count;
  int32                                remaining_section_offset;
  int32                                track_section_index;

  mesh_vertex_index = (g_ROAD_MESH_TRIANGLE_COUNT - PRIMARY_ROAD_MESH_VERTEX_LOOKBEHIND);
  cel_frame_index   = ((section_count_ * PRIMARY_ROAD_SECTION_SEGMENT_COUNT) -
                       (track_section_position_ & PRIMARY_ROAD_SECTION_SEGMENT_MASK)) +
                      PRIMARY_ROAD_CEL_FRAME_INDEX_ADJUSTMENT;
  cel_frame_offset  = g_PRIMARY_ROAD_CEL_FRAME_OFFSETS[cel_frame_index];
  if(track_section_position_ < 0)
    track_section_position_ += (PRIMARY_ROAD_SECTION_SEGMENT_COUNT - 1);
  track_section_index =
    (section_count_ + (track_section_position_ / PRIMARY_ROAD_SECTION_SEGMENT_COUNT)) - 1;
  track_section_position_  = ((track_section_position_ & PRIMARY_ROAD_SECTION_ALIGNMENT_MASK) +
                              (section_count_ * PRIMARY_ROAD_SECTION_SEGMENT_COUNT)) -
                             1;
  remaining_section_offset = (section_count_ - 1) * PRIMARY_ROAD_SECTION_SEGMENT_COUNT;
  if((section_limit_ * PRIMARY_ROAD_SECTION_SEGMENT_COUNT) > remaining_section_offset)
    return;

  cel_groups = (const RoadCelTemplate *const *const *)
    g_RENDER_THREAD_STATE.road_resource_packet_zero;
  do
    {
      model = (const PrimaryRoadGeometryModel *) &g_ROAD_GEOMETRY_MODELS
        [g_ROAD_SECTION_METADATA[track_section_index].primary_model_index];
      mesh_vertex_index -= model->mesh_vertex_count;
      quad_count = model->quad_count;
      quad       = (const PrimaryRoadQuadDefinition *) g_SECONDARY_ROAD_QUAD_DATA;
      quad += model->quad_start_index + quad_count;
      road_slice = get_road_slice_address(track_section_index);

      while(quad_count > 0)
        {
          const RoadCelTemplate *const *cel_templates;
          uint8                         road_flags;

          quad--;
          points[PRIMARY_ROAD_QUAD_FIRST_POINT_INDEX] =
            g_ROAD_MESH_RENDER_WORKSPACE
              .projected[mesh_vertex_index +
                         quad->point_indices[PRIMARY_ROAD_QUAD_FIRST_POINT_INDEX]];
          points[PRIMARY_ROAD_QUAD_SECOND_POINT_INDEX] =
            g_ROAD_MESH_RENDER_WORKSPACE
              .projected[mesh_vertex_index +
                         quad->point_indices[PRIMARY_ROAD_QUAD_SECOND_POINT_INDEX]];
          points[PRIMARY_ROAD_QUAD_THIRD_POINT_INDEX] =
            g_ROAD_MESH_RENDER_WORKSPACE
              .projected[mesh_vertex_index +
                         quad->point_indices[PRIMARY_ROAD_QUAD_THIRD_POINT_INDEX]];
          points[PRIMARY_ROAD_QUAD_FOURTH_POINT_INDEX] =
            g_ROAD_MESH_RENDER_WORKSPACE
              .projected[mesh_vertex_index +
                         quad->point_indices[PRIMARY_ROAD_QUAD_FOURTH_POINT_INDEX]];
          if(is_primary_road_quad_winding_visible(points, quad->render_type) != 0)
            {
              cel_templates =
                cel_groups[road_slice[quad->render_type + PRIMARY_ROAD_DESCRIPTOR_GROUP_OFFSET]];
              render_extended_road_quad(cel_templates[quad->cel_index + cel_frame_offset],
                                        points,
                                        quad->render_type);
            }

          if(quad->animation_step_count != 0)
            {
              road_flags             = road_slice[1];
              detail_cel_group_index = (road_flags & PRIMARY_ROAD_DETAIL_CEL_GROUP_MASK) +
                                       PRIMARY_ROAD_DETAIL_CEL_GROUP_BASE;
              if((road_flags & PRIMARY_ROAD_DETAIL_SECOND_FLAG) != 0)
                {
                  render_primary_road_detail((const RoadGeometryModel *) model,
                                             (uint32) track_section_position_,
                                             mesh_vertex_index,
                                             quad->cel_index + cel_frame_offset,
                                             detail_cel_group_index,
                                             1,
                                             (road_flags & PRIMARY_ROAD_DETAIL_TALL_FLAG) == 0);
                }
              if((road_flags & PRIMARY_ROAD_DETAIL_FIRST_FLAG) != 0)
                {
                  render_primary_road_detail((const RoadGeometryModel *) model,
                                             (uint32) track_section_position_,
                                             mesh_vertex_index,
                                             quad->cel_index + cel_frame_offset,
                                             detail_cel_group_index,
                                             0,
                                             (road_flags & PRIMARY_ROAD_DETAIL_TALL_FLAG) == 0);
                }

              animation_step_count = quad->animation_step_count;
              while(animation_step_count > 0)
                {
                  while(g_ROAD_OBJECT_PROCESSING_STATE.current_record_id == track_section_position_)
                    {
                      render_primary_road_object(cel_frame_index);
                    }
                  advance_frontend_animation_step(track_section_position_);
                  track_section_position_--;
                  animation_step_count--;
                }

              cel_frame_index -= quad->animation_step_count;
              remaining_section_offset -= quad->animation_step_count;
              cel_frame_offset = g_PRIMARY_ROAD_CEL_FRAME_OFFSETS[cel_frame_index];
            }

          quad_count--;
        }

      track_section_index = track_section_position_ / PRIMARY_ROAD_SECTION_SEGMENT_COUNT;
    }
  while((section_limit_ * PRIMARY_ROAD_SECTION_SEGMENT_COUNT) <= remaining_section_offset);
}
