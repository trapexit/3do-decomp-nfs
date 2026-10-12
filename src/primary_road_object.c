/* Renders one primary-road object while walking the pending records backward. */

#include "ccb_manager.h"
#include "quad_mapping.h"
#include "render_thread_signal.h"
#include "road_object_table.h"
#include "road_render.h"
#include "simulation_resource_cleanup.h"
#include "track_position.h"

#include "stddef.h"

#define PRIMARY_ROAD_OBJECT_POINT_MESH_TYPE 1
#define PRIMARY_ROAD_OBJECT_QUAD_MESH_TYPE 4
#define PRIMARY_ROAD_OBJECT_EXTENDED_MESH_TYPE 6
#define PRIMARY_ROAD_OBJECT_VISIBLE_DEPTH 0xFF
#define PRIMARY_ROAD_OBJECT_CCB_NEXT_OFFSET 0x38
#define PRIMARY_ROAD_OBJECT_RECORD_HEADING_SHIFT 16
#define PRIMARY_ROAD_OBJECT_TRACK_HEADING_SHIFT 10
#define PRIMARY_ROAD_OBJECT_FIRST_QUAD_POINT 0
#define PRIMARY_ROAD_OBJECT_SECOND_QUAD_POINT 1
#define PRIMARY_ROAD_OBJECT_THIRD_QUAD_POINT 2
#define PRIMARY_ROAD_OBJECT_FOURTH_QUAD_POINT 3
#define PRIMARY_ROAD_OBJECT_EXTENDED_FIFTH_POINT 4
#define PRIMARY_ROAD_OBJECT_EXTENDED_SIXTH_POINT 5
#define PRIMARY_ROAD_OBJECT_FIRST_REORDER_FLAG 1
#define PRIMARY_ROAD_OBJECT_SECOND_REORDER_FLAG 2

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

#define READ_SIGNED_BIG_ENDIAN_INT16(value_)                                                       \
  ((int32) (int16) (((uint16) ((const uint8 *) (value_))[0] << 8) | ((const uint8 *) (value_))[1]))

typedef struct PrimaryRoadObjectRenderState
{
  uint8                         reserved_00[0x74];
  const RoadCelTemplate *const *cel_templates;
} PrimaryRoadObjectRenderState;

COMPILE_TIME_SIZE_ASSERT(road_object_cel_frame_offset_byte_size, sizeof(uint8) == 1);

uint8 g_ROAD_OBJECT_CEL_FRAME_OFFSETS[PRIMARY_ROAD_CEL_FRAME_OFFSET_COUNT] = {
  2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
};

COMPILE_TIME_SIZE_ASSERT(road_object_cel_frame_offsets_size,
                         sizeof(g_ROAD_OBJECT_CEL_FRAME_OFFSETS) == 0x78);

void
render_primary_road_object(int32 frame_index_)
{
  const RoadObjectDefinition         *definition;
  const PrimaryRoadObjectRenderState *render_state;
  const RoadObjectRecord             *record;
  const RoadCelTemplate              *cel_template;
  const TrackSegment                 *segment;
  const Point2                       *first_projected;
  const Vector3                      *transformed;
  const Point2                       *projected;
  RenderCcb                          *ccb;
  Point2                              first_quad[ROAD_RENDER_REFERENCE_POINT_COUNT];
  Point2                              second_quad[ROAD_RENDER_REFERENCE_POINT_COUNT];
  const uint8                        *cel_frame_offsets;
  int32                               mesh_vertex_index;

  record       = &G_ROAD_OBJECT_RECORDS[g_ROAD_OBJECT_PROCESSING_STATE.current_record_index];
  definition   = &G_ROAD_OBJECT_DEFINITIONS[record->definition_index];
  render_state = (const PrimaryRoadObjectRenderState *) &g_RENDER_THREAD_STATE;

  if(definition->mesh_type == PRIMARY_ROAD_OBJECT_POINT_MESH_TYPE)
    {
      g_ROAD_OBJECT_PROCESSING_STATE.render_vertex_cursor--;
      mesh_vertex_index = g_ROAD_OBJECT_PROCESSING_STATE.render_vertex_cursor;
      segment           = &g_TRACK_SEGMENTS[g_ROAD_OBJECT_PROCESSING_STATE.current_record_id];
      render_road_object_resource(
        &g_OBJECT_RENDER_RESOURCE_RECORDS[definition->render_resource_index],
        &g_ROAD_OBJECT_RENDER_WORKSPACE.vertices[mesh_vertex_index],
        ((int32) record->heading << PRIMARY_ROAD_OBJECT_RECORD_HEADING_SHIFT) +
          (READ_SIGNED_BIG_ENDIAN_INT16(&segment->heading)
           << PRIMARY_ROAD_OBJECT_TRACK_HEADING_SHIFT));
    }
  else
    {
      g_ROAD_OBJECT_PROCESSING_STATE.render_vertex_cursor -= (int32) definition->mesh_type;
      mesh_vertex_index = g_ROAD_OBJECT_PROCESSING_STATE.render_vertex_cursor;
      transformed       = &g_ROAD_OBJECT_RENDER_WORKSPACE.transformed[mesh_vertex_index];
      if((transformed[PRIMARY_ROAD_OBJECT_FIRST_QUAD_POINT].z >
          PRIMARY_ROAD_OBJECT_VISIBLE_DEPTH) &&
         (transformed[PRIMARY_ROAD_OBJECT_SECOND_QUAD_POINT].z >
          PRIMARY_ROAD_OBJECT_VISIBLE_DEPTH) &&
         (transformed[PRIMARY_ROAD_OBJECT_THIRD_QUAD_POINT].z >
          PRIMARY_ROAD_OBJECT_VISIBLE_DEPTH) &&
         (transformed[PRIMARY_ROAD_OBJECT_FOURTH_QUAD_POINT].z > PRIMARY_ROAD_OBJECT_VISIBLE_DEPTH))
        {
          projected = &g_ROAD_OBJECT_RENDER_WORKSPACE.projected[mesh_vertex_index];
          if(definition->mesh_type == PRIMARY_ROAD_OBJECT_QUAD_MESH_TYPE)
            {
              cel_frame_offsets = g_ROAD_OBJECT_CEL_FRAME_OFFSETS;
              if(record->cel_frame_table_select != 0)
                cel_frame_offsets =
                  g_RENDER_THREAD_STATE_RESOURCE_STORAGE.road_object_cel_frame_offsets;
              cel_template = render_state->cel_templates[definition->render_resource_index +
                                                         cel_frame_offsets[frame_index_]];
              ccb          = allocate_partial_copy_ccb((const RenderCcb *) cel_template);
              ccb->fields.next_offset = PRIMARY_ROAD_OBJECT_CCB_NEXT_OFFSET;
              map_fixed16_quad_to_ccb(&ccb->fields,
                                      projected,
                                      cel_template->width,
                                      cel_template->height);
            }
          else if((definition->mesh_type == PRIMARY_ROAD_OBJECT_EXTENDED_MESH_TYPE) &&
                  (transformed[PRIMARY_ROAD_OBJECT_EXTENDED_FIFTH_POINT].z >
                   PRIMARY_ROAD_OBJECT_VISIBLE_DEPTH) &&
                  (transformed[PRIMARY_ROAD_OBJECT_EXTENDED_SIXTH_POINT].z >
                   PRIMARY_ROAD_OBJECT_VISIBLE_DEPTH))
            {
              first_projected = projected;
              if((definition->reserved_00 & PRIMARY_ROAD_OBJECT_FIRST_REORDER_FLAG) != 0)
                {
                  first_quad[PRIMARY_ROAD_OBJECT_FIRST_QUAD_POINT] =
                    projected[PRIMARY_ROAD_OBJECT_SECOND_QUAD_POINT];
                  first_quad[PRIMARY_ROAD_OBJECT_SECOND_QUAD_POINT] =
                    projected[PRIMARY_ROAD_OBJECT_THIRD_QUAD_POINT];
                  first_quad[PRIMARY_ROAD_OBJECT_THIRD_QUAD_POINT] =
                    projected[PRIMARY_ROAD_OBJECT_FOURTH_QUAD_POINT];
                  first_quad[PRIMARY_ROAD_OBJECT_FOURTH_QUAD_POINT] =
                    projected[PRIMARY_ROAD_OBJECT_FIRST_QUAD_POINT];
                  first_projected = first_quad;
                }

              cel_template =
                render_state->cel_templates[definition->render_resource_index +
                                            g_ROAD_OBJECT_CEL_FRAME_OFFSETS[frame_index_]];
              ccb                     = allocate_partial_copy_ccb((const RenderCcb *) cel_template);
              ccb->fields.next_offset = PRIMARY_ROAD_OBJECT_CCB_NEXT_OFFSET;
              map_fixed16_quad_to_ccb(&ccb->fields,
                                      first_projected,
                                      cel_template->width,
                                      cel_template->height);

              if((definition->reserved_00 & PRIMARY_ROAD_OBJECT_SECOND_REORDER_FLAG) == 0)
                {
                  second_quad[PRIMARY_ROAD_OBJECT_FIRST_QUAD_POINT] =
                    projected[PRIMARY_ROAD_OBJECT_SECOND_QUAD_POINT];
                  second_quad[PRIMARY_ROAD_OBJECT_SECOND_QUAD_POINT] =
                    projected[PRIMARY_ROAD_OBJECT_EXTENDED_FIFTH_POINT];
                  second_quad[PRIMARY_ROAD_OBJECT_THIRD_QUAD_POINT] =
                    projected[PRIMARY_ROAD_OBJECT_EXTENDED_SIXTH_POINT];
                  second_quad[PRIMARY_ROAD_OBJECT_FOURTH_QUAD_POINT] =
                    projected[PRIMARY_ROAD_OBJECT_THIRD_QUAD_POINT];
                }
              else
                {
                  second_quad[PRIMARY_ROAD_OBJECT_FIRST_QUAD_POINT] =
                    projected[PRIMARY_ROAD_OBJECT_EXTENDED_FIFTH_POINT];
                  second_quad[PRIMARY_ROAD_OBJECT_SECOND_QUAD_POINT] =
                    projected[PRIMARY_ROAD_OBJECT_EXTENDED_SIXTH_POINT];
                  second_quad[PRIMARY_ROAD_OBJECT_THIRD_QUAD_POINT] =
                    projected[PRIMARY_ROAD_OBJECT_THIRD_QUAD_POINT];
                  second_quad[PRIMARY_ROAD_OBJECT_FOURTH_QUAD_POINT] =
                    projected[PRIMARY_ROAD_OBJECT_SECOND_QUAD_POINT];
                }

              cel_template =
                render_state->cel_templates[definition->extended_cel_index +
                                            g_ROAD_OBJECT_CEL_FRAME_OFFSETS[frame_index_]];
              ccb                     = allocate_partial_copy_ccb((const RenderCcb *) cel_template);
              ccb->fields.next_offset = PRIMARY_ROAD_OBJECT_CCB_NEXT_OFFSET;
              map_fixed16_quad_to_ccb(&ccb->fields,
                                      second_quad,
                                      cel_template->width,
                                      cel_template->height);
            }
        }
    }

  g_ROAD_OBJECT_PROCESSING_STATE.current_record_index--;
  if(g_ROAD_OBJECT_PROCESSING_STATE.current_record_index <
     g_ROAD_OBJECT_PROCESSING_STATE.first_record_index)
    {
      g_ROAD_OBJECT_PROCESSING_STATE.current_record_id = -1;
    }
  else
    {
      g_ROAD_OBJECT_PROCESSING_STATE.current_record_id =
        G_ROAD_OBJECT_RECORDS[g_ROAD_OBJECT_PROCESSING_STATE.current_record_index].record_id;
    }
}
