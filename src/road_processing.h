#pragma once

#include "geometry.h"
#include "simulation_snapshot.h"
#include "track_disk_stream.h"

#include "types.h"

#define ROAD_GEOMETRY_MODEL_TYPE_COUNT 10
#define ROAD_GEOMETRY_MODEL_NEIGHBOR_COUNT 5
#define ROAD_GEOMETRY_MODEL_INDEX_COUNT                                                            \
  (ROAD_GEOMETRY_MODEL_TYPE_COUNT * ROAD_GEOMETRY_MODEL_NEIGHBOR_COUNT)
#define ROAD_GEOMETRY_MODEL_POINT_COUNT 11
#define ROAD_GEOMETRY_MODEL_COUNT 23
#define ROAD_MESH_MODEL_NEIGHBOR_COUNT 32
#define ROAD_GEOMETRY_CACHE_SECTION_COUNT 4
#define ROAD_GEOMETRY_POINT_CAPACITY                                                               \
  (ROAD_GEOMETRY_CACHE_SECTION_COUNT * ROAD_GEOMETRY_MODEL_POINT_COUNT)
#define ROAD_MESH_VERTEX_CAPACITY 299
#define ROAD_OBJECT_MESH_VERTEX_CAPACITY 400

typedef struct RoadGeometryModel
{
  uint8 reserved_00[0x18];
  uint8 point_indices[12];
  uint8 reserved_24[0x28];
} RoadGeometryModel;

typedef struct RoadSectionMetadata
{
  uint8 primary_model_index;
  uint8 secondary_model_index;
  int16 mesh_vertex_start;
  int16 mesh_vertex_count;
  uint8 reserved_06[2];
} RoadSectionMetadata;

typedef struct RoadObjectOrientationState
{
  uint8 current;
  uint8 saved;
  uint8 reserved_02[2];
} RoadObjectOrientationState;

typedef struct RoadMeshRenderWorkspace
{
  Vector3 vertices[ROAD_MESH_VERTEX_CAPACITY];
  Vector3 transformed[ROAD_MESH_VERTEX_CAPACITY];
  Point2  projected[ROAD_MESH_VERTEX_CAPACITY];
} RoadMeshRenderWorkspace;

typedef struct RoadGeometryRenderWorkspace
{
  Vector3 points[ROAD_GEOMETRY_POINT_CAPACITY];
  Vector3 transformed[ROAD_GEOMETRY_POINT_CAPACITY];
  Point2  projected[ROAD_GEOMETRY_POINT_CAPACITY];
} RoadGeometryRenderWorkspace;

typedef struct RoadObjectRenderWorkspace
{
  Vector3 vertices[ROAD_OBJECT_MESH_VERTEX_CAPACITY];
  Vector3 transformed[ROAD_OBJECT_MESH_VERTEX_CAPACITY];
  Point2  projected[ROAD_OBJECT_MESH_VERTEX_CAPACITY];
} RoadObjectRenderWorkspace;

extern int32 g_ROAD_GEOMETRY_MODEL_INDICES[ROAD_GEOMETRY_MODEL_INDEX_COUNT];
extern int32
  g_ROAD_MESH_MODEL_INDICES[ROAD_GEOMETRY_MODEL_TYPE_COUNT * ROAD_MESH_MODEL_NEIGHBOR_COUNT];
extern RoadGeometryModel           g_ROAD_GEOMETRY_MODELS[ROAD_GEOMETRY_MODEL_COUNT];
extern RoadObjectOrientationState *g_ROAD_OBJECT_ORIENTATION_STATES;
extern int32                       g_ROAD_BOUNDARY_TRIANGLE_COUNT;
extern void                       *g_ROAD_PROCESSING_CONTEXT;
extern int32                       g_ROAD_UPDATE_PENDING;
extern RoadSectionMetadata         g_ROAD_SECTION_METADATA[TRACK_SECTION_COUNT];
extern RoadMeshRenderWorkspace     g_ROAD_MESH_RENDER_WORKSPACE;
extern RoadGeometryRenderWorkspace g_ROAD_GEOMETRY_RENDER_WORKSPACE;
extern RoadObjectRenderWorkspace   g_ROAD_OBJECT_RENDER_WORKSPACE;
extern int32                       g_ROAD_MESH_VERTEX_COUNT;
extern int32                       g_ROAD_MESH_TRIANGLE_COUNT;
extern int32                       g_ROAD_GEOMETRY_POINT_COUNT;

#define G_ROAD_MESH_VERTICES (g_ROAD_MESH_RENDER_WORKSPACE.vertices)
#define G_ROAD_GEOMETRY_POINTS (g_ROAD_GEOMETRY_RENDER_WORKSPACE.points)
#define G_ROAD_OBJECT_MESH_VERTICES (g_ROAD_OBJECT_RENDER_WORKSPACE.vertices)

void
cache_road_section_control_points(int32 base_section_index_, int32 section_index_);
void
initialize_road_geometry(void *context_);
void
rebuild_road_geometry_range(int32 track_section_position_,
                            int32 first_section_index_,
                            int32 section_count_);
void
update_road_geometry(SimulationProjectionSource *source_);
void
clear_road_object_orientation_states_at_limit(int32 state_limit_);
void
save_road_object_orientation_states(void);
void
restore_road_object_orientation_states(void);
void
build_road_section_mesh(int32 base_section_index_,
                        int32 section_index_,
                        int32 segment_count_,
                        int32 continue_object_run_);
