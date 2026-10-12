#pragma once

#include "ccb_manager.h"
#include "geometry.h"
#include "road_processing.h"

#include "graphics.h"
#include "types.h"

#define ROAD_RENDER_REFERENCE_POINT_COUNT 4
#define ROAD_RENDER_SCREEN_OFFSET_COUNT 3
#define SECONDARY_ROAD_CEL_FRAME_OFFSET_COUNT 4
#define SECONDARY_ROAD_QUAD_DATA_WORD_COUNT 938
#define PRIMARY_ROAD_CEL_FRAME_OFFSET_COUNT 120

typedef struct RoadRenderScale
{
  int16 horizontal;
  int16 vertical;
} RoadRenderScale;

typedef union RoadRenderScaleWord
{
  RoadRenderScale values;
  int32           parse_options;
} RoadRenderScaleWord;

typedef struct RoadRenderDescriptor
{
  Vector3             reference_points[ROAD_RENDER_REFERENCE_POINT_COUNT];
  Point2              screen_offsets[ROAD_RENDER_SCREEN_OFFSET_COUNT];
  RoadRenderScaleWord scale;
  uint32              cel_data_bytes;
} RoadRenderDescriptor;

typedef struct RoadCelTemplate
{
  CcbTemplate fields;
  int32       width;
  int32       height;
} RoadCelTemplate;

extern CCB                 *g_ROAD_SECTION_CEL;
extern RoadRenderDescriptor g_ROAD_RENDER_DESCRIPTOR;
extern int32                g_ROAD_RENDER_SECTION_INDEX;
extern Point2               g_ROAD_RENDER_SCREEN_POSITION;
extern int32                g_ROAD_RENDER_REFERENCE_INDEX;
extern int32                g_ROAD_RENDER_SCALE_RESPONSE;
extern Vector3              g_ROAD_CLIPPING_SLOPE;
extern int32                g_ROAD_PROJECTION_DEPTH;
extern Vector3              g_ROAD_TRANSLATED_MESH_VERTICES[ROAD_MESH_VERTEX_CAPACITY];
extern Vector3              g_ROAD_TRANSLATED_GEOMETRY_POINTS[ROAD_GEOMETRY_POINT_CAPACITY];
extern int32                g_SECONDARY_ROAD_PROJECTION_SECTION_COUNT;
extern uint32               g_SECONDARY_ROAD_VIEW_FILL_VALUE;
extern int32  g_SECONDARY_ROAD_CEL_FRAME_OFFSETS[SECONDARY_ROAD_CEL_FRAME_OFFSET_COUNT];
extern uint32 g_SECONDARY_ROAD_QUAD_DATA[SECONDARY_ROAD_QUAD_DATA_WORD_COUNT];
extern uint8  g_PRIMARY_ROAD_CEL_FRAME_OFFSETS[PRIMARY_ROAD_CEL_FRAME_OFFSET_COUNT];
extern uint8  g_ROAD_OBJECT_CEL_FRAME_OFFSETS[PRIMARY_ROAD_CEL_FRAME_OFFSET_COUNT];
extern int32  g_PRIMARY_ROAD_RANGE_LIMIT;

void
update_road_render_cels(int32 force_update_);
int32
calculate_road_render_planar_distance(const Vector3 *first_, const Vector3 *second_);
void
render_road_section_cel(void);
int32
project_world_point_to_screen(const Vector3 *point_, Point2 *screen_position_);
int32
project_world_point_to_screen_with_near_clip(const Vector3 *point_, Point2 *screen_position_);
void
update_road_clipping_slope(int32 section_index_);
void
project_road_vertex_batch(int32 vertex_count_, const Vector3 *vertices_, Point2 *projected_);
void
render_road_mesh_geometry(int32 section_batch_count_);
void
render_road_geometry_points(int32 point_count_);
void
render_secondary_road_projection(void);
void
complete_secondary_road_projection(void);
void
prepare_secondary_road_view(void);
void
render_secondary_road_geometry(int32 section_count_);
int32
is_secondary_road_quad_visible(Point2 *quad_, int32 render_type_);
void
render_secondary_road_quad(const RoadCelTemplate *source_ccb_, const Point2 *quad_);
void
render_road_quad_corners(const RoadCelTemplate *source_ccb_,
                         const Point2          *top_left_,
                         const Point2          *top_right_,
                         const Point2          *bottom_right_,
                         const Point2          *bottom_left_);
void
render_extended_road_quad(const RoadCelTemplate *source_ccb_, Point2 *quad_, int32 render_type_);
int32
is_primary_road_quad_visible_by_corners(const Point2 *top_left_,
                                        const Point2 *top_right_,
                                        const Point2 *bottom_right_,
                                        const Point2 *bottom_left_,
                                        int32         render_type_);
int32
is_primary_road_quad_visible(const Point2 *quad_, int32 render_type_);
int32
is_primary_road_quad_winding_visible(const Point2 *quad_, int32 render_type_);
void
render_primary_road_detail(const RoadGeometryModel *model_,
                           uint32                   frame_index_,
                           int32                    mesh_vertex_index_,
                           int32                    cel_index_,
                           int32                    cel_group_index_,
                           int32                    use_second_indices_,
                           int32                    tall_detail_);
void
render_primary_road_object(int32 frame_index_);
void
render_primary_road_range(int32 section_count_,
                          int32 track_section_position_,
                          int32 section_limit_);
void
advance_secondary_road_animation(int32 frame_index_);
void
render_road_scene_range(int32 section_count_, int32 track_section_position_);
