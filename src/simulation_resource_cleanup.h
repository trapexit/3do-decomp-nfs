#pragma once

#include "geometry.h"
#include "three_d_object.h"

#include "types.h"

#define OBJECT_RENDER_RESOURCE_RECORD_COUNT 30
#define CAR_RENDER_RESOURCE_RECORD_COUNT 10
#define RENDER_SHAPE_POINTER_COUNT 3
#define CAR_RENDER_DISTANCE_THRESHOLD_COUNT 5
#define OBJECT_RENDER_DISTANCE_THRESHOLD_COUNT 3

typedef struct RenderResourcePolygonBatch
{
  uint32 *sort_keys;
  int32   polygon_count;
} RenderResourcePolygonBatch;

typedef struct RenderResourceRecord
{
  void                      *object_sources[RENDER_SHAPE_POINTER_COUNT];
  ThreeDObject              *objects[RENDER_SHAPE_POINTER_COUNT];
  uint8                    **shape_pointer_allocations[RENDER_SHAPE_POINTER_COUNT];
  void                      *shape_collections[RENDER_SHAPE_POINTER_COUNT];
  uint8                      reserved_030[0x0C];
  uint32                    *sort_keys;
  void                      *wrapper_file;
  int32                      sort_refresh_countdown;
  uint8                      reserved_048[4];
  int32                      depth_extent;
  int32                      width_extent;
  int32                      height_extent;
  uint8                      reserved_058[0x1C];
  RenderResourcePolygonBatch high_detail_batches[3];
  RenderResourcePolygonBatch medium_detail_batches[3];
  int32                      high_detail_shape_indices[4];
  uint8                     *high_detail_shape_variants[5];
  uint8                     *distant_sprite_resources[2];
  uint8                     *map_shape;
  int32                      tire_smoke_vertex_indices[3];
  uint8                     *palette_shapes[3][4];
  int32                      palette_shape_counts[3];
  int32                      active_palette_shape;
  int32                      animated_shape_indices[12];
  uint8                     *animated_shapes[39];
  int32                      brake_light_shape_indices[4];
  uint8                     *brake_light_shapes[4];
  uint8                      reserved_20_c[0x0C];
} RenderResourceRecord;

extern void *g_TRANSFORMED_COORDINATE_BUFFER;
extern void *g_SCREEN_COORDINATE_BUFFER;
extern void *g_RENDER_DISTANCE_BUFFER;
extern void *g_RENDER_SORT_ORDER_BUFFER;
extern int32 g_PRIMARY_RENDER_DEPTH_LIMIT;
extern int32 g_SECONDARY_RENDER_DEPTH_LIMIT;
extern int32 g_CAR_RENDER_DISTANCE_THRESHOLDS[CAR_RENDER_DISTANCE_THRESHOLD_COUNT];
extern int32 g_OBJECT_RENDER_DISTANCE_THRESHOLDS[OBJECT_RENDER_DISTANCE_THRESHOLD_COUNT];
extern RenderResourceRecord g_OBJECT_RENDER_RESOURCE_RECORDS[OBJECT_RENDER_RESOURCE_RECORD_COUNT];
extern RenderResourceRecord g_CAR_RENDER_RESOURCE_RECORDS[CAR_RENDER_RESOURCE_RECORD_COUNT];

void
initialize_render_distance_thresholds(void);
void
initialize_simulation_render_resources(void);
void
initialize_render_resource_record(void *resource_, RenderResourceRecord *record_);
void
normalize_render_shape_metadata(uint8 *shape_);
void
render_projected_object_polygons(int32               polygon_count_,
                                 const void         *polygons_,
                                 const Point2       *projected_,
                                 const uint32       *sort_order_,
                                 const Vector3      *transformed_,
                                 const uint8 *const *shape_pointers_);
void
render_unextended_projected_object_polygons(int32               polygon_count_,
                                            const void         *polygons_,
                                            const Point2       *projected_,
                                            const uint32       *sort_order_,
                                            const Vector3      *transformed_,
                                            const uint8 *const *shape_pointers_);
int32
render_road_object_resource(const RenderResourceRecord *resource_,
                            const Vector3              *position_,
                            int32                       heading_);
void
initialize_object_render_resources(void *resources_);
void
release_object_render_resources(void);
void
release_car_render_resources(void);
void
shutdown_simulation_render_resources(void);
void
shutdown_simulation_resources(void);
