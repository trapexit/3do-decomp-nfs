#include "road_render.h"

#include "ccb_manager.h"
#include "cel_access.h"
#include "cel_render.h"
#include "fixed_math.h"
#include "memory.h"
#include "render_bounds.h"
#include "render_thread_signal.h"
#include "road_data.h"
#include "road_object_table.h"
#include "road_processing.h"
#include "simulation_snapshot.h"
#include "track_disk_stream.h"
#include "track_position.h"
#include "track_processing_cache.h"

#include "operamath.h"

#include "stddef.h"

#define ROAD_RENDER_SECTION_DIVISOR 4
#define ROAD_RENDER_DESCRIPTOR_OFFSET 0x2184
#define ROAD_RENDER_CEL_DATA_OFFSET 0x21D4
#define ROAD_RENDER_DESCRIPTOR_BYTES 0x50
#define ROAD_RENDER_ENABLED_FLAG 0x00000001
#define ROAD_RENDER_DISTANCE_SHIFT 2
#define ROAD_RENDER_FIXED_SHIFT 8
#define ROAD_RENDER_FIXED_ONE 0x00010000
#define ROAD_RENDER_DEFAULT_RESPONSE 0xA0
#define ROAD_RENDER_VERTICAL_OFFSET 0x0000A666
#define ROAD_RENDER_TRACK_INDEX_MASK (~(ROAD_RENDER_SECTION_DIVISOR - 1))
#define ROAD_RENDER_CAMERA_SHIFT 10
#define ROAD_RENDER_PROJECTION_DEPTH_DEFAULT 0x0001E666
#define ROAD_RENDER_NEAR_X_LIMIT 0x00004200
#define ROAD_RENDER_FAR_DEPTH_LIMIT (-0x00030000)
#define ROAD_RENDER_WORLD_SHIFT 12
#define ROAD_RENDER_SCREEN_MASK 0xFFFFF000
#define ROAD_RENDER_NEAR_SCALE_SHIFT 2
#define ROAD_RENDER_DEPTH_BAND_COUNT 63
#define ROAD_RENDER_DEPTH_BAND_STEP 0x00000180
#define ROAD_RENDER_GEOMETRY_NEAR_Z 0x00000080
#define ROAD_RENDER_COORDINATE_MASK 0xFFFFF000
#define READ_SIGNED_BIG_ENDIAN_INT16(value_)                                                       \
  ((int32) (int16) ((((uint32) ((const uint8 *) (value_))[0]) << 8) |                              \
                    (uint32) ((const uint8 *) (value_))[1]))

typedef char SizeAssertRoadRenderDescriptorSize
  [(sizeof(RoadRenderDescriptor) == ROAD_RENDER_DESCRIPTOR_BYTES) ? 1 : -1];
typedef char SizeAssertRoadRenderScreenOffsetsOffset
  [(offsetof(RoadRenderDescriptor, screen_offsets) == 0x30) ? 1 : -1];
typedef char
  SizeAssertRoadRenderScaleOffset[(offsetof(RoadRenderDescriptor, scale) == 0x48) ? 1 : -1];
typedef char SizeAssertRoadRenderCelDataBytesOffset
  [(offsetof(RoadRenderDescriptor, cel_data_bytes) == 0x4C) ? 1 : -1];
typedef char
  SizeAssertSimulationProjectionTransformSize[(sizeof(SimulationProjectionTransform) == 0x44) ? 1 :
                                                                                                -1];
typedef char SizeAssertSimulationProjectionCameraPositionOffset
  [(offsetof(SimulationProjectionTransform, camera_position) == 0x28) ? 1 : -1];
typedef char SizeAssertSimulationProjectionZRotationOffset
  [(offsetof(SimulationProjectionTransform, z_rotation) == 0x3C) ? 1 : -1];
typedef char
  SizeAssertRenderThreadFlagsOffset[(offsetof(RenderThreadState, render_flags) == 0x20) ? 1 : -1];
typedef char SizeAssertProjectionDescriptorScaleOffset
  [(offsetof(SimulationProjectionDescriptor, projection_scale) == 0x18) ? 1 : -1];
typedef char SizeAssertProjectionSourceTrackSegmentOffset
  [(offsetof(SimulationProjectionSource, track_segment_index) == 0x38) ? 1 : -1];

CCB                 *g_ROAD_SECTION_CEL;
RoadRenderDescriptor g_ROAD_RENDER_DESCRIPTOR;
int32                g_ROAD_RENDER_SECTION_INDEX = -1;
Point2               g_ROAD_RENDER_SCREEN_POSITION;
int32                g_ROAD_RENDER_REFERENCE_INDEX;
int32                g_ROAD_RENDER_SCALE_RESPONSE = ROAD_RENDER_DEFAULT_RESPONSE;
Vector3              g_ROAD_CLIPPING_SLOPE;
int32                g_ROAD_PROJECTION_DEPTH = ROAD_RENDER_PROJECTION_DEPTH_DEFAULT;
Vector3              g_ROAD_TRANSLATED_MESH_VERTICES[ROAD_MESH_VERTEX_CAPACITY];
Vector3              g_ROAD_TRANSLATED_GEOMETRY_POINTS[ROAD_GEOMETRY_POINT_CAPACITY];

void
update_road_render_cels(int32 force_update_)
{
  const RoadRenderDescriptor *descriptor;
  uint8                      *road_section;
  int32                       section_index;

  if(G_TRACK_DATA->stream_layout_mode == 0)
    {
      g_ROAD_SECTION_CEL = NULL;
      return;
    }

  section_index = (g_TRACK_PROCESSING_CACHE.track_section_position / ROAD_RENDER_SECTION_DIVISOR);
  if((section_index == g_ROAD_RENDER_SECTION_INDEX) && (force_update_ == 0) &&
     (g_ROAD_SECTION_CEL != NULL) && (g_ROAD_SECTION_CEL->ccb_SourcePtr != NULL))
    {
      return;
    }

  g_ROAD_RENDER_SECTION_INDEX = section_index;
  road_section                = get_road_section_address(section_index);
  descriptor = (const RoadRenderDescriptor *) (road_section + ROAD_RENDER_DESCRIPTOR_OFFSET);
  g_ROAD_RENDER_DESCRIPTOR = *descriptor;
  road_section             = get_road_section_address(g_ROAD_RENDER_SECTION_INDEX);
  g_ROAD_SECTION_CEL       = parse_cel_data(road_section + ROAD_RENDER_CEL_DATA_OFFSET,
                                            g_ROAD_RENDER_DESCRIPTOR.cel_data_bytes,
                                            g_ROAD_RENDER_DESCRIPTOR.scale.parse_options);
}

int32
calculate_road_render_planar_distance(const Vector3 *first_, const Vector3 *second_)
{
  int32   x_difference;
  int32   z_difference;
  ufrac16 squared_distance;

  x_difference = (int32) ((uint32) first_->x - (uint32) second_->x);
  x_difference >>= ROAD_RENDER_DISTANCE_SHIFT;
  z_difference = (int32) ((uint32) first_->z - (uint32) second_->z);
  z_difference >>= ROAD_RENDER_DISTANCE_SHIFT;
  squared_distance = (ufrac16) MulSF16(x_difference, x_difference);
  squared_distance += (ufrac16) MulSF16(z_difference, z_difference);

  return (int32) ((uint32) SqrtF16(squared_distance) << ROAD_RENDER_DISTANCE_SHIFT);
}

int32
project_world_point_to_screen(const Vector3 *point_, Point2 *screen_position_)
{
  const SimulationProjectionSource     *source;
  const SimulationProjectionDescriptor *projection;
  Vector3                               transformed;

  source     = g_FRONTEND_RENDER_STATE.projection.source;
  projection = g_FRONTEND_RENDER_STATE.projection.projection;
  transformed.x =
    ((int32) ((uint32) point_->x - (uint32) source->projection_transform.camera_position.x) >>
     ROAD_RENDER_WORLD_SHIFT);
  transformed.y =
    ((int32) ((uint32) point_->y - (uint32) source->projection_transform.camera_position.y) >>
     ROAD_RENDER_WORLD_SHIFT);
  transformed.z =
    ((int32) ((uint32) point_->z - (uint32) source->projection_transform.camera_position.z) >>
     ROAD_RENDER_WORLD_SHIFT);
  multiply_vector3_matrix3x3(&transformed,
                             (const Matrix3x3 *) g_FRONTEND_RENDER_STATE.projection.view_state,
                             &transformed);

  if(transformed.z < (projection->projection_scale >> ROAD_RENDER_NEAR_SCALE_SHIFT))
    return 1;

  project_vector3_array_with_depth_table(1, &transformed, screen_position_, 0);
  screen_position_->x = (int32) ((uint32) screen_position_->x & ROAD_RENDER_SCREEN_MASK);
  screen_position_->y = (int32) ((uint32) screen_position_->y & ROAD_RENDER_SCREEN_MASK);
  return 0;
}

int32
project_world_point_to_screen_with_near_clip(const Vector3 *point_, Point2 *screen_position_)
{
  const SimulationProjectionSource     *source;
  const SimulationProjectionDescriptor *projection;
  int32                                 clipping_scale;
  int32                                 near_depth;
  Vector3                               transformed;

  source     = g_FRONTEND_RENDER_STATE.projection.source;
  projection = g_FRONTEND_RENDER_STATE.projection.projection;
  transformed.x =
    ((int32) ((uint32) point_->x - (uint32) source->projection_transform.camera_position.x) >>
     ROAD_RENDER_WORLD_SHIFT);
  transformed.y =
    ((int32) ((uint32) point_->y - (uint32) source->projection_transform.camera_position.y) >>
     ROAD_RENDER_WORLD_SHIFT);
  transformed.z =
    ((int32) ((uint32) point_->z - (uint32) source->projection_transform.camera_position.z) >>
     ROAD_RENDER_WORLD_SHIFT);
  multiply_vector3_matrix3x3(&transformed,
                             (const Matrix3x3 *) g_FRONTEND_RENDER_STATE.projection.view_state,
                             &transformed);

  near_depth = projection->projection_scale >> ROAD_RENDER_NEAR_SCALE_SHIFT;
  if(transformed.z < near_depth)
    {
      clipping_scale =
        divide_fixed16_saturating((int32) ((uint32) near_depth - (uint32) transformed.z),
                                  g_ROAD_CLIPPING_SLOPE.z);
      transformed.x = (int32) ((uint32) transformed.x +
                               (uint32) multiply_fixed16(clipping_scale, g_ROAD_CLIPPING_SLOPE.x));
      transformed.y = (int32) ((uint32) transformed.y +
                               (uint32) multiply_fixed16(clipping_scale, g_ROAD_CLIPPING_SLOPE.y));
      transformed.z = near_depth;
      project_vector3_array_with_depth_table(1, &transformed, screen_position_, 0);
      screen_position_->x = (int32) ((uint32) screen_position_->x & ROAD_RENDER_SCREEN_MASK);
      screen_position_->y = (int32) ((uint32) screen_position_->y & ROAD_RENDER_SCREEN_MASK);
      return 1;
    }

  project_vector3_array_with_depth_table(1, &transformed, screen_position_, 0);
  screen_position_->x = (int32) ((uint32) screen_position_->x & ROAD_RENDER_SCREEN_MASK);
  screen_position_->y = (int32) ((uint32) screen_position_->y & ROAD_RENDER_SCREEN_MASK);
  return 0;
}

void
render_road_section_cel(void)
{
  const SimulationProjectionSource *source;
  const Vector3                    *reference_point;
  const Point2                     *screen_offset;
  int32                             track_reference_distance;
  int32                             camera_reference_distance;
  int32                             distance_ratio;
  int32                             scale_adjustment;
  int32                             horizontal_scale;
  int32                             vertical_scale;
  int32                             track_segment_index;

  source          = g_FRONTEND_RENDER_STATE.projection.source;
  reference_point = &g_ROAD_RENDER_DESCRIPTOR.reference_points[g_ROAD_RENDER_REFERENCE_INDEX];
  (void) project_world_point_to_screen(reference_point, &g_ROAD_RENDER_SCREEN_POSITION);
  if(((g_RENDER_THREAD_STATE.render_flags & ROAD_RENDER_ENABLED_FLAG) == 0) ||
     (READ_SIGNED_BIG_ENDIAN_INT16(&g_ROAD_RENDER_DESCRIPTOR.scale.values.horizontal) == 0))
    {
      return;
    }

  track_segment_index = (source->track_section_position & ROAD_RENDER_TRACK_INDEX_MASK);
  track_reference_distance =
    calculate_road_render_planar_distance(&g_TRACK_SEGMENTS[track_segment_index].surface_reference,
                                          reference_point);
  camera_reference_distance =
    calculate_road_render_planar_distance(&source->projection_transform.camera_position,
                                          reference_point);
  distance_ratio   = DivSF16(track_reference_distance, camera_reference_distance);
  scale_adjustment = ((distance_ratio - ROAD_RENDER_FIXED_ONE) >> ROAD_RENDER_FIXED_SHIFT);
  scale_adjustment = (int32) ((uint32) g_ROAD_RENDER_SCALE_RESPONSE * (uint32) scale_adjustment);
  scale_adjustment >>= ROAD_RENDER_FIXED_SHIFT;
  horizontal_scale = (int32) ((uint32) READ_SIGNED_BIG_ENDIAN_INT16(
                                &g_ROAD_RENDER_DESCRIPTOR.scale.values.horizontal) *
                              (uint32) scale_adjustment);
  horizontal_scale = (int32) ((uint32) horizontal_scale + ROAD_RENDER_FIXED_ONE);
  vertical_scale   = (int32) ((uint32) READ_SIGNED_BIG_ENDIAN_INT16(
                                &g_ROAD_RENDER_DESCRIPTOR.scale.values.vertical) *
                              (uint32) scale_adjustment);
  vertical_scale   = (int32) ((uint32) vertical_scale + ROAD_RENDER_FIXED_ONE);
  if(g_ROAD_SECTION_CEL == NULL)
    return;

  screen_offset = &g_ROAD_RENDER_DESCRIPTOR.screen_offsets[g_ROAD_RENDER_REFERENCE_INDEX];
  draw_transformed_ccb(g_ROAD_SECTION_CEL,
                       g_ROAD_RENDER_SCREEN_POSITION.x,
                       g_ROAD_RENDER_SCREEN_POSITION.y + ROAD_RENDER_VERTICAL_OFFSET,
                       screen_offset->x,
                       screen_offset->y,
                       source->projection_transform.z_rotation,
                       horizontal_scale,
                       vertical_scale);
}

void
update_road_clipping_slope(int32 section_index_)
{
  const Vector3 *first;
  const Vector3 *last;
  int32          vertex_count;

  vertex_count =
    READ_SIGNED_BIG_ENDIAN_INT16(&g_ROAD_SECTION_METADATA[section_index_].mesh_vertex_count);
  first                   = &g_ROAD_MESH_RENDER_WORKSPACE.transformed[0];
  last                    = &g_ROAD_MESH_RENDER_WORKSPACE.transformed[vertex_count];
  g_ROAD_CLIPPING_SLOPE.x = (int32) ((uint32) last->x - (uint32) first->x);
  g_ROAD_CLIPPING_SLOPE.y = (int32) ((uint32) last->y - (uint32) first->y);
  g_ROAD_CLIPPING_SLOPE.z = (int32) ((uint32) last->z - (uint32) first->z);
}

void
project_road_vertex_batch(int32 vertex_count_, const Vector3 *vertices_, Point2 *projected_)
{
  project_vector3_array_with_depth_table(vertex_count_, vertices_, projected_, 0);
  mask_shifted_point2_array(vertex_count_,
                            projected_,
                            0,
                            &g_PROJECTED_POINT_COORDINATE_MASKS,
                            projected_);
}

/* This oracle boundary couples batch transforms with its near-plane pass. */
void
render_road_mesh_geometry(int32 section_batch_count_)
{
  SimulationProjectionSource *source;
  const Matrix3x3            *view_matrix;
  Vector3                    *vertex;
  Vector3                     camera_translation;
  int32                       clipping_scale;
  int32                       index;
  int32                       near_depth;
  int32                       depth_band_remaining;
  int32                       primary_vertex_count;
  int32                       secondary_vertex_count;

  (void) section_batch_count_;
  source     = g_FRONTEND_RENDER_STATE.projection.source;
  near_depth = (g_ROAD_PROJECTION_DEPTH >> ROAD_RENDER_CAMERA_SHIFT);
  g_FRONTEND_RENDER_STATE.projection.projection->projection_scale = near_depth;
  camera_translation.x = ((int32) (0U - (uint32) source->projection_transform.camera_position.x) >>
                          ROAD_RENDER_CAMERA_SHIFT);
  camera_translation.y = ((int32) (0U - (uint32) source->projection_transform.camera_position.y) >>
                          ROAD_RENDER_CAMERA_SHIFT);
  camera_translation.z = ((int32) (0U - (uint32) source->projection_transform.camera_position.z) >>
                          ROAD_RENDER_CAMERA_SHIFT);

  primary_vertex_count = g_ROAD_MESH_TRIANGLE_COUNT;
  translate_shifted_vector3_array(primary_vertex_count,
                                  g_ROAD_MESH_RENDER_WORKSPACE.vertices,
                                  ROAD_RENDER_CAMERA_SHIFT,
                                  &camera_translation,
                                  g_ROAD_TRANSLATED_MESH_VERTICES);
  secondary_vertex_count = g_ROAD_OBJECT_PROCESSING_STATE.projected_vertex_count;
  if(g_ROAD_OBJECT_PROCESSING_STATE.processed_record_count > 0)
    {
      translate_shifted_vector3_array(secondary_vertex_count,
                                      g_ROAD_OBJECT_RENDER_WORKSPACE.vertices,
                                      ROAD_RENDER_CAMERA_SHIFT,
                                      &camera_translation,
                                      g_ROAD_OBJECT_RENDER_WORKSPACE.transformed);
    }

  view_matrix = (const Matrix3x3 *) g_FRONTEND_RENDER_STATE.projection.view_state;
  MulManyVec3Mat33_F16((vec3f16 *) g_ROAD_MESH_RENDER_WORKSPACE.transformed,
                       (vec3f16 *) g_ROAD_TRANSLATED_MESH_VERTICES,
                       *(mat33f16 *) view_matrix,
                       primary_vertex_count);
  if(g_ROAD_OBJECT_PROCESSING_STATE.processed_record_count > 0)
    {
      MulManyVec3Mat33_F16((vec3f16 *) g_ROAD_OBJECT_RENDER_WORKSPACE.transformed,
                           (vec3f16 *) g_ROAD_OBJECT_RENDER_WORKSPACE.transformed,
                           *(mat33f16 *) view_matrix,
                           secondary_vertex_count);
    }

  move_bytes((const uint8 *) g_ROAD_MESH_RENDER_WORKSPACE.transformed,
             (uint8 *) g_ROAD_TRANSLATED_MESH_VERTICES,
             (uint32) primary_vertex_count * sizeof(Vector3));
  update_road_clipping_slope(source->track_segment_index / ROAD_RENDER_SECTION_DIVISOR);

  vertex = g_ROAD_TRANSLATED_MESH_VERTICES;
  for(index = 0; index < ROAD_RENDER_DEPTH_BAND_COUNT; index++)
    {
      if(vertex->x > ROAD_RENDER_NEAR_X_LIMIT)
        vertex->x = ROAD_RENDER_NEAR_X_LIMIT;
      if(vertex->x < -ROAD_RENDER_NEAR_X_LIMIT)
        vertex->x = -ROAD_RENDER_NEAR_X_LIMIT;
      if(vertex->z < ROAD_RENDER_FAR_DEPTH_LIMIT)
        vertex->z = near_depth;
      else if(vertex->z < near_depth)
        {
          clipping_scale =
            divide_fixed16_saturating((int32) ((uint32) near_depth - (uint32) vertex->z),
                                      g_ROAD_CLIPPING_SLOPE.z);
          vertex->x = (int32) ((uint32) vertex->x +
                               (uint32) multiply_fixed16(clipping_scale, g_ROAD_CLIPPING_SLOPE.x));
          vertex->y = (int32) ((uint32) vertex->y +
                               (uint32) multiply_fixed16(clipping_scale, g_ROAD_CLIPPING_SLOPE.y));
          vertex->z = near_depth;
        }

      vertex++;
    }

  depth_band_remaining = 0;
  for(; index < primary_vertex_count; index++)
    {
      if(vertex->z < ROAD_RENDER_FAR_DEPTH_LIMIT)
        vertex->z = near_depth;
      else if(vertex->z < near_depth)
        {
          clipping_scale =
            divide_fixed16_saturating((int32) ((uint32) near_depth - (uint32) vertex->z),
                                      g_ROAD_CLIPPING_SLOPE.z);
          vertex->x = (int32) ((uint32) vertex->x +
                               (uint32) multiply_fixed16(clipping_scale, g_ROAD_CLIPPING_SLOPE.x));
          vertex->y = (int32) ((uint32) vertex->y +
                               (uint32) multiply_fixed16(clipping_scale, g_ROAD_CLIPPING_SLOPE.y));
          vertex->z = near_depth;
        }

      if(depth_band_remaining < 1)
        {
          near_depth           = (int32) ((uint32) near_depth + ROAD_RENDER_DEPTH_BAND_STEP);
          depth_band_remaining = ROAD_RENDER_DEPTH_BAND_COUNT;
        }
      else
        depth_band_remaining--;
      vertex++;
    }

  project_road_vertex_batch(primary_vertex_count,
                            g_ROAD_TRANSLATED_MESH_VERTICES,
                            g_ROAD_MESH_RENDER_WORKSPACE.projected);
  if(g_ROAD_OBJECT_PROCESSING_STATE.processed_record_count > 0)
    {
      project_road_vertex_batch(secondary_vertex_count,
                                g_ROAD_OBJECT_RENDER_WORKSPACE.transformed,
                                g_ROAD_OBJECT_RENDER_WORKSPACE.projected);
    }
}

void
render_road_geometry_points(int32 point_count_)
{
  SimulationProjectionSource *source;
  const Matrix3x3            *view_matrix;
  Vector3                     camera_translation;
  Vector3                     clamped_point;
  int32                       index;
  int32                       near_depth;

  (void) point_count_;
  source     = g_FRONTEND_RENDER_STATE.projection.source;
  near_depth = (g_ROAD_PROJECTION_DEPTH >> ROAD_RENDER_CAMERA_SHIFT);
  g_FRONTEND_RENDER_STATE.projection.projection->projection_scale = near_depth;
  camera_translation.x = ((int32) (0U - (uint32) source->projection_transform.camera_position.x) >>
                          ROAD_RENDER_CAMERA_SHIFT);
  camera_translation.y = ((int32) (0U - (uint32) source->projection_transform.camera_position.y) >>
                          ROAD_RENDER_CAMERA_SHIFT);
  camera_translation.z = ((int32) (0U - (uint32) source->projection_transform.camera_position.z) >>
                          ROAD_RENDER_CAMERA_SHIFT);

  translate_shifted_vector3_array(g_ROAD_GEOMETRY_POINT_COUNT,
                                  g_ROAD_GEOMETRY_RENDER_WORKSPACE.points,
                                  ROAD_RENDER_CAMERA_SHIFT,
                                  &camera_translation,
                                  g_ROAD_TRANSLATED_GEOMETRY_POINTS);
  view_matrix = (const Matrix3x3 *) g_FRONTEND_RENDER_STATE.projection.view_state;
  MulManyVec3Mat33_F16((vec3f16 *) g_ROAD_GEOMETRY_RENDER_WORKSPACE.transformed,
                       (vec3f16 *) g_ROAD_TRANSLATED_GEOMETRY_POINTS,
                       *(mat33f16 *) view_matrix,
                       g_ROAD_GEOMETRY_POINT_COUNT);
  project_road_vertex_batch(g_ROAD_GEOMETRY_POINT_COUNT,
                            g_ROAD_GEOMETRY_RENDER_WORKSPACE.transformed,
                            g_ROAD_GEOMETRY_RENDER_WORKSPACE.projected);

  for(index = 0; index < g_ROAD_GEOMETRY_POINT_COUNT; index++)
    {
      if(g_ROAD_GEOMETRY_RENDER_WORKSPACE.transformed[index].z < ROAD_RENDER_GEOMETRY_NEAR_Z)
        {
          clamped_point.x = g_ROAD_GEOMETRY_RENDER_WORKSPACE.transformed[index].x;
          clamped_point.y = g_ROAD_GEOMETRY_RENDER_WORKSPACE.transformed[index].y;
          clamped_point.z = ROAD_RENDER_GEOMETRY_NEAR_Z;
          project_vector3_array_with_depth_table(1,
                                                 &clamped_point,
                                                 &g_ROAD_GEOMETRY_RENDER_WORKSPACE.projected[index],
                                                 0);
          g_ROAD_GEOMETRY_RENDER_WORKSPACE.projected[index].x &= ROAD_RENDER_COORDINATE_MASK;
          g_ROAD_GEOMETRY_RENDER_WORKSPACE.projected[index].y &= ROAD_RENDER_COORDINATE_MASK;
        }
    }
}
