/* Renders the road cels used by the secondary projection viewport. */

#include "frontend_render_state.h"
#include "frontend_transition.h"
#include "render_thread_signal.h"
#include "road_data.h"
#include "road_processing.h"
#include "road_render.h"
#include "simulation_snapshot.h"

#define SECONDARY_ROAD_SECTION_SEGMENT_COUNT 4
#define SECONDARY_ROAD_SECTION_LOOKBEHIND 3
#define SECONDARY_ROAD_GEOMETRY_POINT_COUNT 11
#define SECONDARY_ROAD_GEOMETRY_DESCRIPTOR_OFFSET 2
#define SECONDARY_ROAD_ANIMATION_STEP_COUNT 5
#define SECONDARY_ROAD_QUAD_FIRST_POINT_INDEX 0
#define SECONDARY_ROAD_QUAD_SECOND_POINT_INDEX 1
#define SECONDARY_ROAD_QUAD_THIRD_POINT_INDEX 2
#define SECONDARY_ROAD_QUAD_FOURTH_POINT_INDEX 3
#define SECONDARY_ROAD_CEL_FRAME_OFFSET_FIRST 2
#define SECONDARY_ROAD_CEL_FRAME_OFFSET_SECOND 2
#define SECONDARY_ROAD_CEL_FRAME_OFFSET_THIRD 3
#define SECONDARY_ROAD_CEL_FRAME_OFFSET_FOURTH 3

typedef struct SecondaryRoadGeometryModel
{
  uint8 reserved_00[4];
  int32 quad_count;
  uint8 reserved_08[0x40];
  int32 quad_start_index;
} SecondaryRoadGeometryModel;

typedef struct SecondaryRoadQuadDefinition
{
  uint8 point_indices[4];
  uint8 render_type;
  uint8 reserved_05;
  uint8 cel_index;
  uint8 animation_step;
} SecondaryRoadQuadDefinition;

typedef char SecondaryRoadGeometryModelSize
  [(sizeof(SecondaryRoadGeometryModel) == sizeof(RoadGeometryModel)) ? 1 : -1];
typedef char SecondaryRoadQuadDefinitionSize[(sizeof(SecondaryRoadQuadDefinition) == 8) ? 1 : -1];

int32 g_SECONDARY_ROAD_CEL_FRAME_OFFSETS[SECONDARY_ROAD_CEL_FRAME_OFFSET_COUNT] = {
  SECONDARY_ROAD_CEL_FRAME_OFFSET_FIRST,
  SECONDARY_ROAD_CEL_FRAME_OFFSET_SECOND,
  SECONDARY_ROAD_CEL_FRAME_OFFSET_THIRD,
  SECONDARY_ROAD_CEL_FRAME_OFFSET_FOURTH
};

void
render_secondary_road_geometry(int32 section_count_)
{
  const SecondaryRoadGeometryModel    *model;
  const SecondaryRoadQuadDefinition   *quad;
  const uint8                         *section_descriptor;
  const SimulationProjectionSource    *source;
  const RoadCelTemplate *const *const *cel_groups;
  Point2                               points[ROAD_RENDER_REFERENCE_POINT_COUNT];
  Point2                              *section_points;
  int32                                animation_index;
  int32                                cel_frame_offset;
  int32                                quad_count;
  int32                                section_index;
  int32                                section_offset;

  update_frontend_menu_coordinates();
  source        = g_FRONTEND_RENDER_STATE.projection.source;
  section_index = (source->track_section_position + (SECONDARY_ROAD_SECTION_SEGMENT_COUNT - 1)) /
                  SECONDARY_ROAD_SECTION_SEGMENT_COUNT;
  section_index -= SECONDARY_ROAD_SECTION_LOOKBEHIND;
  if(section_index < 0)
    section_index = 0;

  animation_index = section_index * SECONDARY_ROAD_SECTION_SEGMENT_COUNT;
  cel_groups      = (const RoadCelTemplate *const *const *)
    g_RENDER_THREAD_STATE.road_resource_packet_zero;
  for(section_offset = 0; section_offset < section_count_; section_offset++)
    {
      model =
        (const SecondaryRoadGeometryModel
           *) &g_ROAD_GEOMETRY_MODELS[g_ROAD_SECTION_METADATA[section_index].secondary_model_index];
      quad_count = model->quad_count;
      quad       = (const SecondaryRoadQuadDefinition *) g_SECONDARY_ROAD_QUAD_DATA;
      quad += model->quad_start_index + quad_count;
      cel_frame_offset = g_SECONDARY_ROAD_CEL_FRAME_OFFSETS[(section_count_ - 1) - section_offset];
      section_descriptor = (const uint8 *) get_road_section_geometry_address(section_index);
      section_points     = &g_ROAD_GEOMETRY_RENDER_WORKSPACE
                              .projected[section_offset * SECONDARY_ROAD_GEOMETRY_POINT_COUNT];

      while(quad_count > 0)
        {
          quad--;
          points[SECONDARY_ROAD_QUAD_FIRST_POINT_INDEX] =
            section_points[quad->point_indices[SECONDARY_ROAD_QUAD_FIRST_POINT_INDEX]];
          points[SECONDARY_ROAD_QUAD_SECOND_POINT_INDEX] =
            section_points[quad->point_indices[SECONDARY_ROAD_QUAD_SECOND_POINT_INDEX]];
          points[SECONDARY_ROAD_QUAD_THIRD_POINT_INDEX] =
            section_points[quad->point_indices[SECONDARY_ROAD_QUAD_THIRD_POINT_INDEX]];
          points[SECONDARY_ROAD_QUAD_FOURTH_POINT_INDEX] =
            section_points[quad->point_indices[SECONDARY_ROAD_QUAD_FOURTH_POINT_INDEX]];
          if(is_secondary_road_quad_visible(points, quad->render_type) != 0)
            {
              render_secondary_road_quad(
                cel_groups[section_descriptor[quad->render_type +
                                              SECONDARY_ROAD_GEOMETRY_DESCRIPTOR_OFFSET]]
                          [quad->cel_index + cel_frame_offset],
                points);
            }
          if(quad->animation_step != 0)
            {
              int32 animation_step;

              for(animation_step = 0; animation_step < SECONDARY_ROAD_ANIMATION_STEP_COUNT;
                  animation_step++)
                {
                  advance_secondary_road_animation(animation_index);
                  animation_index++;
                }
            }
          quad_count--;
        }
      section_index++;
    }
}
