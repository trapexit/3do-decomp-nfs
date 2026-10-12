#include "road_processing.h"

#include "road_data.h"
#include "road_object_table.h"
#include "road_render.h"
#include "track_disk_stream.h"
#include "track_position.h"
#include "track_processing_cache.h"
#include "trig_lookup.h"

#include "stddef.h"

#define ROAD_SECTION_SEGMENT_COUNT 4
#define ROAD_SLICE_SEGMENT_BYTES 0x90
#define ROAD_SLICE_SEGMENT_POINTS_OFFSET 0x24
#define ROAD_SLICE_FLAGS_OFFSET 1
#define ROAD_SLICE_LEFT_BOUNDARY_FLAG 0x80
#define ROAD_SLICE_RIGHT_BOUNDARY_FLAG 0x40
#define ROAD_BOUNDARY_TRIANGLES_PER_SIDE 2
#define ROAD_MODEL_POINT_LIST_END 0xFF
#define ROAD_GEOMETRY_LOOKBEHIND_SECTIONS 3
#define ROAD_GEOMETRY_REBUILD_FORCE 1
#define ROAD_OBJECT_POINT_MESH_TYPE 1
#define ROAD_OBJECT_EXTENDED_MESH_TYPE 6
#define ROAD_OBJECT_HALF_WIDTH_DIVISOR 2
#define ROAD_OBJECT_QUAD_VERTEX_COUNT 4
#define ROAD_OBJECT_EXTENSION_VERTEX_COUNT 2
#define ROAD_OBJECT_DISPLAY_STATE_COUNT_OFFSET 0x16C40
#define ROAD_OBJECT_POSITION_SCALE 0x100
#define ROAD_OBJECT_HEADING_SCALE 0x10000
#define ROAD_SEGMENT_HEADING_SCALE 0x400
#define ROAD_BIG_ENDIAN_WORD_SHIFT 8
#define ROAD_TRIG_ANGLE_SHIFT 14
#define ROAD_TRIG_VALUE_SHIFT 8
#define ROAD_QUARTER_TURN_ANGLE 0x400000
#define ROAD_FULL_TURN_ANGLE 0x800000

typedef char SizeAssertRoadGeometryModelSize[(sizeof(RoadGeometryModel) == 0x4C) ? 1 : -1];
typedef char SizeAssertRoadGeometryModelIndicesOffset
  [(offsetof(RoadGeometryModel, point_indices) == 0x18) ? 1 : -1];
typedef char SizeAssertRoadObjectRecordSize[(sizeof(RoadObjectRecord) == 0x10) ? 1 : -1];
typedef char
  SizeAssertRoadObjectRecordOffsetX[(offsetof(RoadObjectRecord, offset_x) == 0x0A) ? 1 : -1];
typedef char SizeAssertRoadObjectDefinitionSize[(sizeof(RoadObjectDefinition) == 0x10) ? 1 : -1];
typedef char
  SizeAssertRoadObjectDefinitionWidthOffset[(offsetof(RoadObjectDefinition, width) == 4) ? 1 : -1];
typedef char SizeAssertRoadSectionMetadataSize[(sizeof(RoadSectionMetadata) == 8) ? 1 : -1];
typedef char SizeAssertTrackProcessingCacheSize[(sizeof(TrackProcessingCache) == 0x10) ? 1 : -1];
typedef char
  SizeAssertTrackSegmentGeometryTypeOffset[(offsetof(TrackSegment, road_geometry_type) == 7) ? 1 :
                                                                                               -1];
typedef char SizeAssertTrackSegmentSize[(sizeof(TrackSegment) == 0x24) ? 1 : -1];
typedef char SizeAssertProjectionRoadUpdateStateOffset
  [(offsetof(SimulationProjectionSource, road_update_state) == 0x3C) ? 1 : -1];
typedef char SizeAssertProjectionRoadRefreshOffset
  [(offsetof(SimulationProjectionSource, road_geometry_refresh_pending) == 0xFC) ? 1 : -1];
typedef char
  SizeAssertRoadMeshRenderWorkspaceSize[(sizeof(RoadMeshRenderWorkspace) == 0x2560) ? 1 : -1];
typedef char
  SizeAssertRoadGeometryRenderWorkspaceSize[(sizeof(RoadGeometryRenderWorkspace) == 0x580) ? 1 :
                                                                                             -1];
typedef char
  SizeAssertRoadObjectRenderWorkspaceSize[(sizeof(RoadObjectRenderWorkspace) == 0x3200) ? 1 : -1];
typedef char
  SizeAssertRoadObjectOrientationStateSize[(sizeof(RoadObjectOrientationState) == 4) ? 1 : -1];

typedef struct RoadObjectDisplayState
{
  uint8 reserved_00000[ROAD_OBJECT_DISPLAY_STATE_COUNT_OFFSET];
  int32 orientation_state_count;
} RoadObjectDisplayState;

typedef char SizeAssertRoadObjectDisplayStateCountOffset[(offsetof(RoadObjectDisplayState,
                                                                   orientation_state_count) ==
                                                          ROAD_OBJECT_DISPLAY_STATE_COUNT_OFFSET) ?
                                                           1 :
                                                           -1];

void                       *g_ROAD_PROCESSING_CONTEXT;
int32                       g_ROAD_UPDATE_PENDING;
RoadSectionMetadata         g_ROAD_SECTION_METADATA[TRACK_SECTION_COUNT];
RoadMeshRenderWorkspace     g_ROAD_MESH_RENDER_WORKSPACE;
RoadGeometryRenderWorkspace g_ROAD_GEOMETRY_RENDER_WORKSPACE;
RoadObjectRenderWorkspace   g_ROAD_OBJECT_RENDER_WORKSPACE;
int32                       g_ROAD_MESH_VERTEX_COUNT;
int32                       g_ROAD_MESH_TRIANGLE_COUNT;
int32                       g_ROAD_GEOMETRY_POINT_COUNT;
RoadObjectOrientationState *g_ROAD_OBJECT_ORIENTATION_STATES;
uint8                       g_ROAD_BOUNDARY_RESERVED_PREFIX[0x300];
int32                       g_ROAD_BOUNDARY_TRIANGLE_COUNT;
uint8                       g_ROAD_BOUNDARY_RESERVED_SUFFIX[8];

void
clear_road_object_orientation_states_at_limit(int32 state_limit_)
{
  const RoadObjectDisplayState *display_state;
  int32                         state_index;

  display_state = (const RoadObjectDisplayState *) G_TRACK_DATA;
  state_index   = 0;
  while(state_index < display_state->orientation_state_count)
    {
      if(((int32) g_ROAD_OBJECT_ORIENTATION_STATES[state_index].current - 1) >= state_limit_)
        {
          g_ROAD_OBJECT_ORIENTATION_STATES[state_index].current = 0;
        }
      state_index++;
    }
}

void
save_road_object_orientation_states(void)
{
  const RoadObjectDisplayState *display_state;
  int32                         state_index;

  display_state = (const RoadObjectDisplayState *) G_TRACK_DATA;
  state_index   = 0;
  while(state_index < display_state->orientation_state_count)
    {
      g_ROAD_OBJECT_ORIENTATION_STATES[state_index].saved =
        g_ROAD_OBJECT_ORIENTATION_STATES[state_index].current;
      state_index++;
    }
}

void
restore_road_object_orientation_states(void)
{
  const RoadObjectDisplayState *display_state;
  int32                         state_index;

  display_state = (const RoadObjectDisplayState *) G_TRACK_DATA;
  state_index   = 0;
  while(state_index < display_state->orientation_state_count)
    {
      g_ROAD_OBJECT_ORIENTATION_STATES[state_index].current =
        g_ROAD_OBJECT_ORIENTATION_STATES[state_index].saved;
      state_index++;
    }
}

typedef struct RoadObjectMeshSpans
{
  int32 width_x;
  int32 width_z;
  int32 height_x;
  int32 height_y;
  int32 height_z;
  int32 extension_angle;
} RoadObjectMeshSpans;

void
cache_road_section_control_points(int32 base_section_index_, int32 section_index_)
{
  RoadSectionGeometry *geometry;
  RoadGeometryModel   *model;
  int32                geometry_type;
  int32                model_index;
  int32                point_index;
  int32                selected_index;

  geometry      = get_road_section_geometry_address(section_index_);
  geometry_type = g_TRACK_SEGMENTS[section_index_ * ROAD_SECTION_SEGMENT_COUNT].road_geometry_type;
  model_index = g_ROAD_GEOMETRY_MODEL_INDICES[(geometry_type * ROAD_GEOMETRY_MODEL_NEIGHBOR_COUNT) +
                                              (section_index_ - base_section_index_)];
  model       = &g_ROAD_GEOMETRY_MODELS[model_index];
  selected_index = 0;
  for(point_index = 0; point_index < ROAD_GEOMETRY_MODEL_POINT_COUNT; point_index++)
    {
      if(model->point_indices[selected_index] == point_index)
        {
          G_ROAD_GEOMETRY_POINTS[g_ROAD_GEOMETRY_POINT_COUNT] = geometry->points[point_index];
          g_ROAD_GEOMETRY_POINT_COUNT++;
          selected_index++;
        }
    }

  g_ROAD_SECTION_METADATA[section_index_].secondary_model_index = (uint8) model_index;
}

void
initialize_road_geometry(void *context_)
{
  g_ROAD_PROCESSING_CONTEXT = context_;
  invalidate_track_processing_cache();
  update_road_geometry(g_FRONTEND_RENDER_STATE.projection.source);
}

void
rebuild_road_geometry_range(int32 track_section_position_,
                            int32 first_section_index_,
                            int32 section_count_)
{
  int32 base_section_index;
  int32 section_offset;

  base_section_index = (track_section_position_ / ROAD_SECTION_SEGMENT_COUNT);
  g_TRACK_PROCESSING_CACHE.current_section_index  = base_section_index;
  g_TRACK_PROCESSING_CACHE.track_section_position = track_section_position_;
  g_ROAD_MESH_VERTEX_COUNT                        = 0;
  g_ROAD_MESH_TRIANGLE_COUNT                      = 0;
  initialize_road_object_processing(base_section_index);

  for(section_offset = 0; section_offset < (section_count_ - 1); section_offset++)
    {
      build_road_section_mesh(base_section_index,
                              first_section_index_,
                              ROAD_SECTION_SEGMENT_COUNT,
                              1);
      first_section_index_++;
    }

  build_road_section_mesh(base_section_index, first_section_index_, ROAD_SECTION_SEGMENT_COUNT, 0);
  build_road_section_mesh(base_section_index, first_section_index_ + 1, 1, 0);
}

void
update_road_geometry(SimulationProjectionSource *source_)
{
  int32 cached_track_position;
  int32 first_geometry_section;
  int32 section_index;
  int32 section_offset;
  int32 track_section_position;

  track_section_position = source_->track_section_position;
  section_index          = (track_section_position / ROAD_SECTION_SEGMENT_COUNT);
  synchronize_road_section_stream(section_index);
  if(g_ROAD_UPDATE_PENDING != 0)
    g_ROAD_UPDATE_PENDING = 0;

  cached_track_position = g_TRACK_PROCESSING_CACHE.track_section_position;
  if(((cached_track_position / ROAD_SECTION_SEGMENT_COUNT) != section_index) ||
     (cached_track_position < 0))
    {
      rebuild_road_geometry_range(track_section_position,
                                  section_index,
                                  g_TRACK_PROCESSING_CACHE.section_batch_count);
      update_road_render_cels(ROAD_GEOMETRY_REBUILD_FORCE);
    }

  if(source_->road_geometry_refresh_pending == 0)
    return;

  first_geometry_section =
    ((track_section_position + (ROAD_SECTION_SEGMENT_COUNT - 1)) / ROAD_SECTION_SEGMENT_COUNT) -
    ROAD_GEOMETRY_LOOKBEHIND_SECTIONS;
  if(first_geometry_section < 0)
    first_geometry_section = 0;
  if(first_geometry_section != g_TRACK_PROCESSING_CACHE.geometry_start_section)
    {
      g_TRACK_PROCESSING_CACHE.geometry_start_section = first_geometry_section;
      g_ROAD_GEOMETRY_POINT_COUNT                     = 0;
      for(section_offset = 0; section_offset < ROAD_GEOMETRY_CACHE_SECTION_COUNT; section_offset++)
        {
          cache_road_section_control_points(first_geometry_section, first_geometry_section);
          first_geometry_section++;
        }
    }

  source_->road_update_state             = 0;
  source_->road_geometry_refresh_pending = 0;
}

void
build_road_section_mesh(int32 base_section_index_,
                        int32 section_index_,
                        int32 segment_count_,
                        int32 continue_object_run_)
{
  RoadGeometryModel                *model;
  TrackSegment                     *segment;
  Vector3                          *vertex;
  const Vector3                    *base_vertex;
  const Vector3                    *points;
  const RoadObjectRecord           *record;
  const RoadObjectOrientationState *orientation_state;
  const RoadObjectDefinition       *definition;
  RoadObjectMeshSpans               spans;
  Vector3                           position;
  const uint8                      *segment_bytes;
  const uint8                      *point_indices;
  uint8                            *road_slice;
  uint8                             flags;
  int32                             first_vertex_index;
  int32                             geometry_type;
  int32                             model_index;
  int32                             segment_index;
  int32                             segment_offset;
  int32                             point_index;
  int32                             record_index;
  int32                             angle;
  int32                             cosine;
  int32                             half_width;
  int32                             height;
  int32                             object_heading;
  int32                             segment_heading;
  int32                             sine;
  int32                             extension;
  int32                             extension_x;
  int32                             extension_z;

  first_vertex_index = g_ROAD_MESH_VERTEX_COUNT;
  road_slice         = get_road_slice_address(section_index_);
  segment_index      = (section_index_ * ROAD_SECTION_SEGMENT_COUNT);
  geometry_type      = g_TRACK_SEGMENTS[segment_index].road_geometry_type;
  model_index        = g_ROAD_MESH_MODEL_INDICES[(geometry_type * ROAD_MESH_MODEL_NEIGHBOR_COUNT) +
                                                 (section_index_ - base_section_index_)];
  model              = &g_ROAD_GEOMETRY_MODELS[model_index];
  point_indices      = model->point_indices;

  for(segment_offset = 0; segment_offset < segment_count_; segment_offset++)
    {
      segment     = (TrackSegment *) (road_slice + (segment_offset * ROAD_SLICE_SEGMENT_BYTES));
      points      = (const Vector3 *) ((const uint8 *) segment + ROAD_SLICE_SEGMENT_POINTS_OFFSET);
      point_index = 0;
      while(*point_indices != ROAD_MODEL_POINT_LIST_END)
        {
          if(*point_indices == point_index)
            {
              G_ROAD_MESH_VERTICES[g_ROAD_MESH_VERTEX_COUNT] = points[point_index];
              g_ROAD_MESH_VERTEX_COUNT++;
              point_indices++;
            }
          point_index++;
        }
      point_indices++;

      if(continue_object_run_ != 0)
        {
          flags = road_slice[ROAD_SLICE_FLAGS_OFFSET];
          if((flags & ROAD_SLICE_LEFT_BOUNDARY_FLAG) != 0)
            {
              g_ROAD_BOUNDARY_TRIANGLE_COUNT =
                (g_ROAD_BOUNDARY_TRIANGLE_COUNT + ROAD_BOUNDARY_TRIANGLES_PER_SIDE);
            }
          if((flags & ROAD_SLICE_RIGHT_BOUNDARY_FLAG) != 0)
            {
              g_ROAD_BOUNDARY_TRIANGLE_COUNT =
                (g_ROAD_BOUNDARY_TRIANGLE_COUNT + ROAD_BOUNDARY_TRIANGLES_PER_SIDE);
            }

          segment       = &g_TRACK_SEGMENTS[segment_index];
          segment_bytes = (const uint8 *) segment;
          segment_heading =
            (int32) (int16) (((uint16) segment_bytes[offsetof(TrackSegment, heading)]
                              << ROAD_BIG_ENDIAN_WORD_SHIFT) |
                             segment_bytes[offsetof(TrackSegment, heading) + 1]);
          while(g_ROAD_OBJECT_PROCESSING_STATE.current_record_id == segment_index)
            {
              record_index      = g_ROAD_OBJECT_PROCESSING_STATE.current_record_index;
              record            = &G_ROAD_OBJECT_RECORDS[record_index];
              orientation_state = &g_ROAD_OBJECT_ORIENTATION_STATES[record_index];
              definition        = &G_ROAD_OBJECT_DEFINITIONS[record->definition_index];
              position.x        = (segment->surface_reference.x +
                                   ((int32) record->offset_x * ROAD_OBJECT_POSITION_SCALE));
              position.y        = (segment->surface_reference.y +
                                   ((int32) record->offset_y * ROAD_OBJECT_POSITION_SCALE));
              position.z        = (segment->surface_reference.z +
                                   ((int32) record->offset_z * ROAD_OBJECT_POSITION_SCALE));

              if(definition->mesh_type == ROAD_OBJECT_POINT_MESH_TYPE)
                {
                  vertex =
                    &G_ROAD_OBJECT_MESH_VERTICES[g_ROAD_OBJECT_PROCESSING_STATE.mesh_vertex_count];
                  *vertex = position;
                  g_ROAD_OBJECT_PROCESSING_STATE.mesh_vertex_count++;
                  g_ROAD_OBJECT_PROCESSING_STATE.projected_vertex_count++;
                }
              else
                {
                  object_heading = -((int32) record->heading * ROAD_OBJECT_HEADING_SCALE);
                  angle = (object_heading - (segment_heading * ROAD_SEGMENT_HEADING_SCALE));
                  /* ARM SDT implements signed right shifts arithmetically. */
                  sine   = sine_1024(angle >> ROAD_TRIG_ANGLE_SHIFT);
                  cosine = cosine_1024(angle >> ROAD_TRIG_ANGLE_SHIFT);
                  half_width =
                    ((definition->width / ROAD_OBJECT_HALF_WIDTH_DIVISOR) >> ROAD_TRIG_VALUE_SHIFT);
                  spans.width_x         = ((cosine >> ROAD_TRIG_VALUE_SHIFT) * half_width);
                  spans.width_z         = ((sine >> ROAD_TRIG_VALUE_SHIFT) * half_width);
                  spans.height_x        = 0;
                  spans.height_z        = 0;
                  spans.extension_angle = angle;

                  if(orientation_state->current == 0)
                    {
                      spans.height_y = definition->height;
                    }
                  else
                    {
                      spans.height_y = 0;
                      if(object_heading < -ROAD_QUARTER_TURN_ANGLE)
                        object_heading = (object_heading + ROAD_FULL_TURN_ANGLE);
                      angle                 = (object_heading + ROAD_QUARTER_TURN_ANGLE -
                                               (segment_heading * ROAD_SEGMENT_HEADING_SCALE));
                      spans.extension_angle = angle;
                      sine                  = sine_1024(angle >> ROAD_TRIG_ANGLE_SHIFT);
                      cosine                = cosine_1024(angle >> ROAD_TRIG_ANGLE_SHIFT);
                      height                = (definition->height >> ROAD_TRIG_VALUE_SHIFT);
                      spans.height_x        = ((cosine >> ROAD_TRIG_VALUE_SHIFT) * height);
                      spans.height_z        = ((sine >> ROAD_TRIG_VALUE_SHIFT) * height);
                    }

                  vertex =
                    &G_ROAD_OBJECT_MESH_VERTICES[g_ROAD_OBJECT_PROCESSING_STATE.mesh_vertex_count];
                  vertex->x = (position.x - spans.width_x + spans.height_x);
                  vertex->y = (position.y + spans.height_y);
                  vertex->z = (position.z - spans.width_z + spans.height_z);
                  g_ROAD_OBJECT_PROCESSING_STATE.mesh_vertex_count++;
                  vertex =
                    &G_ROAD_OBJECT_MESH_VERTICES[g_ROAD_OBJECT_PROCESSING_STATE.mesh_vertex_count];
                  vertex->x = (position.x + spans.width_x + spans.height_x);
                  vertex->y = (position.y + spans.height_y);
                  vertex->z = (position.z + spans.width_z + spans.height_z);
                  g_ROAD_OBJECT_PROCESSING_STATE.mesh_vertex_count++;
                  vertex =
                    &G_ROAD_OBJECT_MESH_VERTICES[g_ROAD_OBJECT_PROCESSING_STATE.mesh_vertex_count];
                  vertex->x = (position.x + spans.width_x);
                  vertex->y = position.y;
                  vertex->z = (position.z + spans.width_z);
                  g_ROAD_OBJECT_PROCESSING_STATE.mesh_vertex_count++;
                  vertex =
                    &G_ROAD_OBJECT_MESH_VERTICES[g_ROAD_OBJECT_PROCESSING_STATE.mesh_vertex_count];
                  vertex->x = (position.x - spans.width_x);
                  vertex->y = position.y;
                  vertex->z = (position.z - spans.width_z);
                  g_ROAD_OBJECT_PROCESSING_STATE.mesh_vertex_count++;
                  g_ROAD_OBJECT_PROCESSING_STATE.projected_vertex_count =
                    (g_ROAD_OBJECT_PROCESSING_STATE.projected_vertex_count +
                     ROAD_OBJECT_QUAD_VERTEX_COUNT);

                  if(definition->mesh_type == ROAD_OBJECT_EXTENDED_MESH_TYPE)
                    {
                      angle       = (spans.extension_angle + ROAD_QUARTER_TURN_ANGLE);
                      sine        = sine_1024(angle >> ROAD_TRIG_ANGLE_SHIFT);
                      cosine      = cosine_1024(angle >> ROAD_TRIG_ANGLE_SHIFT);
                      extension   = (definition->length >> ROAD_TRIG_VALUE_SHIFT);
                      extension_x = ((cosine >> ROAD_TRIG_VALUE_SHIFT) * extension);
                      extension_z = ((sine >> ROAD_TRIG_VALUE_SHIFT) * extension);
                      base_vertex =
                        &G_ROAD_OBJECT_MESH_VERTICES[g_ROAD_OBJECT_PROCESSING_STATE
                                                       .mesh_vertex_count -
                                                     ROAD_OBJECT_EXTENSION_VERTEX_COUNT];
                      vertex    = &G_ROAD_OBJECT_MESH_VERTICES[g_ROAD_OBJECT_PROCESSING_STATE
                                                                 .mesh_vertex_count];
                      vertex->x = (base_vertex->x + extension_x);
                      vertex->y = (base_vertex->y + spans.height_y);
                      vertex->z = (base_vertex->z + extension_z);
                      g_ROAD_OBJECT_PROCESSING_STATE.mesh_vertex_count++;
                      vertex    = &G_ROAD_OBJECT_MESH_VERTICES[g_ROAD_OBJECT_PROCESSING_STATE
                                                                 .mesh_vertex_count];
                      vertex->x = (base_vertex->x + extension_x);
                      vertex->y = base_vertex->y;
                      vertex->z = (base_vertex->z + extension_z);
                      g_ROAD_OBJECT_PROCESSING_STATE.mesh_vertex_count++;
                      g_ROAD_OBJECT_PROCESSING_STATE.projected_vertex_count =
                        (g_ROAD_OBJECT_PROCESSING_STATE.projected_vertex_count +
                         ROAD_OBJECT_EXTENSION_VERTEX_COUNT);
                    }
                }

              g_ROAD_OBJECT_PROCESSING_STATE.current_record_index++;
              g_ROAD_OBJECT_PROCESSING_STATE.processed_record_count++;
              g_ROAD_OBJECT_PROCESSING_STATE.current_record_id =
                G_ROAD_OBJECT_RECORDS[g_ROAD_OBJECT_PROCESSING_STATE.current_record_index]
                  .record_id;
            }
        }

      segment_index++;
    }

  g_ROAD_SECTION_METADATA[section_index_].primary_model_index = (uint8) model_index;
  g_ROAD_SECTION_METADATA[section_index_].mesh_vertex_start   = (int16) first_vertex_index;
  g_ROAD_SECTION_METADATA[section_index_].mesh_vertex_count =
    (int16) (g_ROAD_MESH_VERTEX_COUNT - first_vertex_index);
  g_ROAD_MESH_TRIANGLE_COUNT =
    (g_ROAD_MESH_TRIANGLE_COUNT + g_ROAD_SECTION_METADATA[section_index_].mesh_vertex_count);
}
