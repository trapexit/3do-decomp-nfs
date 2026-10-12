/* Renders primary road cels while synchronizing their object animation. */

#include "fixed_math.h"
#include "frontend_render_state.h"
#include "frontend_transition.h"
#include "render_bounds.h"
#include "render_thread_signal.h"
#include "road_data.h"
#include "road_object_table.h"
#include "road_render.h"
#include "track_processing_cache.h"
#include "vehicle_render_resource_selection.h"

#define PRIMARY_ROAD_SCENE_RANGE_LIMIT_INITIAL 5
#define PRIMARY_ROAD_SCENE_SEGMENT_COUNT 4
#define PRIMARY_ROAD_SCENE_SEGMENT_MASK 3
#define PRIMARY_ROAD_SCENE_ALIGNMENT_MASK -4
#define PRIMARY_ROAD_SCENE_MODEL_POINT_COUNT 11
#define PRIMARY_ROAD_SCENE_DESCRIPTOR_GROUP_OFFSET 2
#define PRIMARY_ROAD_SCENE_DETAIL_GROUP_BASE 0x20
#define PRIMARY_ROAD_SCENE_DETAIL_GROUP_MASK 0x1F
#define PRIMARY_ROAD_SCENE_DETAIL_FIRST_FLAG 0x80
#define PRIMARY_ROAD_SCENE_DETAIL_SECOND_FLAG 0x40
#define PRIMARY_ROAD_SCENE_DETAIL_SECOND_INDICES_FLAG 0x20
#define PRIMARY_ROAD_SCENE_NEAR_CLIP_MINIMUM (-0x30000)
#define PRIMARY_ROAD_SCENE_PROJECTED_ALIGNMENT_MASK 0xFFFFF000UL
#define PRIMARY_ROAD_SCENE_FIRST_CLIPPED_CEL_OFFSET 4
#define PRIMARY_ROAD_SCENE_SECOND_CLIPPED_CEL_OFFSET 8
#define READ_SIGNED_BIG_ENDIAN_INT16(value_)                                                       \
  ((int32) (int16) ((((uint32) ((const uint8 *) (value_))[0]) << 8) |                              \
                    (uint32) ((const uint8 *) (value_))[1]))

enum
{
  PRIMARY_ROAD_SCENE_TOP_LEFT_INDEX     = 0,
  PRIMARY_ROAD_SCENE_TOP_RIGHT_INDEX    = 1,
  PRIMARY_ROAD_SCENE_BOTTOM_RIGHT_INDEX = 2,
  PRIMARY_ROAD_SCENE_BOTTOM_LEFT_INDEX  = 3
};

typedef struct PrimaryRoadSceneGeometryModel
{
  int32 mesh_vertex_count;
  int32 quad_count;
  uint8 reserved_08[0x40];
  int32 quad_start_index;
} PrimaryRoadSceneGeometryModel;

typedef struct PrimaryRoadSceneQuadDefinition
{
  uint8 point_indices[ROAD_RENDER_REFERENCE_POINT_COUNT];
  uint8 render_type;
  uint8 use_clipped_cels;
  uint8 cel_index;
  uint8 animation_step_count;
} PrimaryRoadSceneQuadDefinition;

static void
_clip_primary_road_midpoint(Vector3 *midpoint_,
                            int32    projection_depth_,
                            int32    clipping_slope_x_,
                            int32    clipping_slope_y_,
                            int32    clipping_slope_z_);

static void
_project_primary_road_midpoint(const Vector3 *first_,
                               const Vector3 *second_,
                               Vector3       *midpoint_,
                               Point2        *projected_,
                               int32          projection_depth_,
                               int32          clipping_slope_x_,
                               int32          clipping_slope_y_,
                               int32          clipping_slope_z_);

static void
_render_clipped_primary_road_quad(const PrimaryRoadSceneQuadDefinition *quad_,
                                  const RoadCelTemplate *const         *cel_templates_,
                                  const Vector3                        *transformed_,
                                  const Point2                         *projected_);

static void
_render_primary_road_scene_details(const PrimaryRoadSceneGeometryModel  *model_,
                                   const PrimaryRoadSceneQuadDefinition *quad_,
                                   const uint8                          *road_slice_,
                                   int32                                 animation_frame_,
                                   int32                                 mesh_vertex_index_,
                                   int32                                 cel_frame_offset_);

static void
_advance_primary_road_scene_objects(int32 animation_frame_,
                                    int32 cel_frame_index_,
                                    int32 step_count_);


int32 g_PRIMARY_ROAD_RANGE_LIMIT = PRIMARY_ROAD_SCENE_RANGE_LIMIT_INITIAL;

void
render_road_scene_range(int32 section_count_, int32 track_section_position_)
{
  const PrimaryRoadSceneGeometryModel  *model;
  const PrimaryRoadSceneQuadDefinition *quad;
  const RoadCelTemplate *const *const  *cel_groups;
  const RoadCelTemplate *const         *cel_templates;
  const uint8                          *road_slice;
  const Vector3                        *transformed;
  Point2                                projected[ROAD_RENDER_REFERENCE_POINT_COUNT];
  int32                                 animation_frame;
  int32                                 cel_frame_index;
  int32                                 cel_frame_offset;
  int32                                 mesh_vertex_index;
  int32                                 quad_count;
  int32                                 section_index;
  int32                                 section_limit;
  int32                                 section_offset;

  g_RENDER_THREAD_STATE.road_scene_render_state = 0;
  cel_groups = (const RoadCelTemplate *const *const *)
    g_RENDER_THREAD_STATE.road_resource_packet_zero;
  update_frontend_menu_coordinates();
  section_limit = g_PRIMARY_ROAD_RANGE_LIMIT;
  render_primary_road_range(section_count_, track_section_position_, section_limit);

  mesh_vertex_index = g_ROAD_MESH_TRIANGLE_COUNT - PRIMARY_ROAD_SCENE_MODEL_POINT_COUNT -
                      ((section_count_ - section_limit) * PRIMARY_ROAD_SCENE_MODEL_POINT_COUNT);
  cel_frame_index   = (section_limit * PRIMARY_ROAD_SCENE_SEGMENT_COUNT) -
                      (track_section_position_ & PRIMARY_ROAD_SCENE_SEGMENT_MASK) +
                      (PRIMARY_ROAD_SCENE_SEGMENT_COUNT - 1);
  cel_frame_offset  = g_PRIMARY_ROAD_CEL_FRAME_OFFSETS[cel_frame_index];
  section_index = section_limit + (track_section_position_ / PRIMARY_ROAD_SCENE_SEGMENT_COUNT) - 1;
  animation_frame = (track_section_position_ & PRIMARY_ROAD_SCENE_ALIGNMENT_MASK) +
                    (section_limit * PRIMARY_ROAD_SCENE_SEGMENT_COUNT) - 1;

  for(section_offset = section_limit - 1; section_offset >= 0; section_offset--)
    {
      model =
        (const PrimaryRoadSceneGeometryModel
           *) &g_ROAD_GEOMETRY_MODELS[g_ROAD_SECTION_METADATA[section_index].primary_model_index];
      mesh_vertex_index -=
        READ_SIGNED_BIG_ENDIAN_INT16(&g_ROAD_SECTION_METADATA[section_index].mesh_vertex_count);
      transformed = &g_ROAD_MESH_RENDER_WORKSPACE.transformed[mesh_vertex_index];
      quad        = (const PrimaryRoadSceneQuadDefinition *) g_SECONDARY_ROAD_QUAD_DATA;
      quad += model->quad_start_index + model->quad_count;
      quad_count = model->quad_count;
      road_slice = get_road_slice_address(section_index);
      while(quad_count > 0)
        {
          quad--;
          if(transformed[quad->point_indices[PRIMARY_ROAD_SCENE_BOTTOM_LEFT_INDEX]].z >=
             g_FRONTEND_RENDER_STATE.projection.projection->projection_scale)
            {
              projected[PRIMARY_ROAD_SCENE_TOP_LEFT_INDEX] =
                g_ROAD_MESH_RENDER_WORKSPACE
                  .projected[mesh_vertex_index +
                             quad->point_indices[PRIMARY_ROAD_SCENE_TOP_LEFT_INDEX]];
              projected[PRIMARY_ROAD_SCENE_TOP_RIGHT_INDEX] =
                g_ROAD_MESH_RENDER_WORKSPACE
                  .projected[mesh_vertex_index +
                             quad->point_indices[PRIMARY_ROAD_SCENE_TOP_RIGHT_INDEX]];
              projected[PRIMARY_ROAD_SCENE_BOTTOM_RIGHT_INDEX] =
                g_ROAD_MESH_RENDER_WORKSPACE
                  .projected[mesh_vertex_index +
                             quad->point_indices[PRIMARY_ROAD_SCENE_BOTTOM_RIGHT_INDEX]];
              projected[PRIMARY_ROAD_SCENE_BOTTOM_LEFT_INDEX] =
                g_ROAD_MESH_RENDER_WORKSPACE
                  .projected[mesh_vertex_index +
                             quad->point_indices[PRIMARY_ROAD_SCENE_BOTTOM_LEFT_INDEX]];
              cel_templates = cel_groups[road_slice[quad->render_type +
                                                    PRIMARY_ROAD_SCENE_DESCRIPTOR_GROUP_OFFSET]];
              if((quad->use_clipped_cels == 0) || (cel_frame_offset != 0))
                {
                  if(is_primary_road_quad_visible(projected, quad->render_type) != 0)
                    {
                      render_extended_road_quad(cel_templates[quad->cel_index + cel_frame_offset],
                                                projected,
                                                quad->render_type);
                    }
                }
              else
                {
                  _render_clipped_primary_road_quad(quad, cel_templates, transformed, projected);
                }
            }

          if(quad->animation_step_count != 0)
            {
              _render_primary_road_scene_details(model,
                                                 quad,
                                                 road_slice,
                                                 animation_frame,
                                                 mesh_vertex_index,
                                                 cel_frame_offset);
              _advance_primary_road_scene_objects(animation_frame,
                                                  cel_frame_index,
                                                  (int32) quad->animation_step_count);
              animation_frame -= (int32) quad->animation_step_count;
              cel_frame_index -= (int32) quad->animation_step_count;
              cel_frame_offset = g_PRIMARY_ROAD_CEL_FRAME_OFFSETS[cel_frame_index];
            }

          quad_count--;
        }

      section_index--;
    }

  advance_secondary_road_animation(animation_frame);
  advance_secondary_road_animation(animation_frame + 1);
}

static void
_clip_primary_road_midpoint(Vector3 *midpoint_,
                            int32    projection_depth_,
                            int32    clipping_slope_x_,
                            int32    clipping_slope_y_,
                            int32    clipping_slope_z_)
{
  int32 clip_factor;

  if(midpoint_->z < PRIMARY_ROAD_SCENE_NEAR_CLIP_MINIMUM)
    {
      midpoint_->z = projection_depth_;
      return;
    }
  if(midpoint_->z >= projection_depth_)
    return;

  clip_factor =
    divide_fixed16_saturating(projection_depth_ - midpoint_->z, clipping_slope_z_);
  midpoint_->x += multiply_fixed16(clip_factor, clipping_slope_x_);
  midpoint_->y += multiply_fixed16(clip_factor, clipping_slope_y_);
  midpoint_->z = projection_depth_;
}

static void
_project_primary_road_midpoint(const Vector3 *first_,
                               const Vector3 *second_,
                               Vector3       *midpoint_,
                               Point2        *projected_,
                               int32          projection_depth_,
                               int32          clipping_slope_x_,
                               int32          clipping_slope_y_,
                               int32          clipping_slope_z_)
{
  midpoint_->x = (first_->x + second_->x) >> 1;
  midpoint_->y = (first_->y + second_->y) >> 1;
  midpoint_->z = (first_->z + second_->z) >> 1;
  _clip_primary_road_midpoint(midpoint_,
                              projection_depth_,
                              clipping_slope_x_,
                              clipping_slope_y_,
                              clipping_slope_z_);
  project_vector3_array_with_depth_table(1, midpoint_, projected_, 0);
  projected_->x &= (int32) PRIMARY_ROAD_SCENE_PROJECTED_ALIGNMENT_MASK;
  projected_->y &= (int32) PRIMARY_ROAD_SCENE_PROJECTED_ALIGNMENT_MASK;
}

static void
_render_clipped_primary_road_quad(const PrimaryRoadSceneQuadDefinition *quad_,
                                  const RoadCelTemplate *const         *cel_templates_,
                                  const Vector3                        *transformed_,
                                  const Point2                         *projected_)
{
  Point2  first_midpoint_projected;
  Point2  second_midpoint_projected;
  Vector3 first_midpoint;
  Vector3 second_midpoint;

  _project_primary_road_midpoint(
    &transformed_[quad_->point_indices[PRIMARY_ROAD_SCENE_TOP_LEFT_INDEX]],
    &transformed_[quad_->point_indices[PRIMARY_ROAD_SCENE_TOP_RIGHT_INDEX]],
    &first_midpoint,
    &first_midpoint_projected,
    g_FRONTEND_RENDER_STATE.projection.projection->projection_scale,
    g_ROAD_CLIPPING_SLOPE.x,
    g_ROAD_CLIPPING_SLOPE.y,
    g_ROAD_CLIPPING_SLOPE.z);
  _project_primary_road_midpoint(
    &transformed_[quad_->point_indices[PRIMARY_ROAD_SCENE_BOTTOM_RIGHT_INDEX]],
    &transformed_[quad_->point_indices[PRIMARY_ROAD_SCENE_BOTTOM_LEFT_INDEX]],
    &second_midpoint,
    &second_midpoint_projected,
    g_FRONTEND_RENDER_STATE.projection.projection->projection_scale,
    g_ROAD_CLIPPING_SLOPE.x,
    g_ROAD_CLIPPING_SLOPE.y,
    g_ROAD_CLIPPING_SLOPE.z);

  if(is_primary_road_quad_visible_by_corners(&projected_[PRIMARY_ROAD_SCENE_TOP_LEFT_INDEX],
                                             &first_midpoint_projected,
                                             &second_midpoint_projected,
                                             &projected_[PRIMARY_ROAD_SCENE_BOTTOM_LEFT_INDEX],
                                             quad_->render_type) != 0)
    {
      render_road_quad_corners(
        cel_templates_[quad_->cel_index + PRIMARY_ROAD_SCENE_FIRST_CLIPPED_CEL_OFFSET],
        &projected_[PRIMARY_ROAD_SCENE_TOP_LEFT_INDEX],
        &first_midpoint_projected,
        &second_midpoint_projected,
        &projected_[PRIMARY_ROAD_SCENE_BOTTOM_LEFT_INDEX]);
    }

  if((second_midpoint.z >= g_FRONTEND_RENDER_STATE.projection.projection->projection_scale) &&
     (is_primary_road_quad_visible_by_corners(&first_midpoint_projected,
                                              &projected_[PRIMARY_ROAD_SCENE_TOP_RIGHT_INDEX],
                                              &projected_[PRIMARY_ROAD_SCENE_BOTTOM_RIGHT_INDEX],
                                              &second_midpoint_projected,
                                              quad_->render_type) != 0))
    {
      render_road_quad_corners(
        cel_templates_[quad_->cel_index + PRIMARY_ROAD_SCENE_SECOND_CLIPPED_CEL_OFFSET],
        &first_midpoint_projected,
        &projected_[PRIMARY_ROAD_SCENE_TOP_RIGHT_INDEX],
        &projected_[PRIMARY_ROAD_SCENE_BOTTOM_RIGHT_INDEX],
        &second_midpoint_projected);
    }
}

static void
_render_primary_road_scene_details(const PrimaryRoadSceneGeometryModel  *model_,
                                   const PrimaryRoadSceneQuadDefinition *quad_,
                                   const uint8                          *road_slice_,
                                   int32                                 animation_frame_,
                                   int32                                 mesh_vertex_index_,
                                   int32                                 cel_frame_offset_)
{
  int32 detail_cel_group;
  int32 tall_detail;

  detail_cel_group =
    (road_slice_[1] & PRIMARY_ROAD_SCENE_DETAIL_GROUP_MASK) + PRIMARY_ROAD_SCENE_DETAIL_GROUP_BASE;
  tall_detail = ((road_slice_[1] & PRIMARY_ROAD_SCENE_DETAIL_SECOND_INDICES_FLAG) == 0);
  if((road_slice_[1] & PRIMARY_ROAD_SCENE_DETAIL_FIRST_FLAG) != 0)
    {
      render_primary_road_detail((const RoadGeometryModel *) model_,
                                 (uint32) animation_frame_,
                                 mesh_vertex_index_,
                                 (int32) quad_->cel_index + cel_frame_offset_,
                                 detail_cel_group,
                                 1,
                                 tall_detail);
    }
  if((road_slice_[1] & PRIMARY_ROAD_SCENE_DETAIL_SECOND_FLAG) != 0)
    {
      render_primary_road_detail((const RoadGeometryModel *) model_,
                                 (uint32) animation_frame_,
                                 mesh_vertex_index_,
                                 (int32) quad_->cel_index + cel_frame_offset_,
                                 detail_cel_group,
                                 0,
                                 tall_detail);
    }
}

static void
_advance_primary_road_scene_objects(int32 animation_frame_,
                                    int32 cel_frame_index_,
                                    int32 step_count_)
{
  while(step_count_ > 0)
    {
      while(g_ROAD_OBJECT_PROCESSING_STATE.current_record_id == animation_frame_)
        {
          render_primary_road_object(cel_frame_index_);
        }

      advance_frontend_animation_step(animation_frame_);
      animation_frame_--;
      while((g_ROAD_OBJECT_PROCESSING_STATE.current_record_id == animation_frame_) &&
            ((&G_ROAD_OBJECT_RECORDS[g_ROAD_OBJECT_PROCESSING_STATE.current_record_index])
               ->cel_frame_table_select == 0))
        {
          render_primary_road_object(cel_frame_index_);
        }

      step_count_--;
    }
}
