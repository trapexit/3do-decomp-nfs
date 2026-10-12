#pragma once

#include "geometry.h"

#include "types.h"

typedef struct RoadSectionGeometry
{
  uint8   reserved_00[0x18];
  Vector3 points[11];
} RoadSectionGeometry;

typedef struct RoadRuntimeState
{
  char   track_name[64];
  uint8 *road_objects;
  uint8 *road_object_definitions;
  uint8 *road_object_records;
  void  *track_data;
  uint8 *road_slice_buffer;
} RoadRuntimeState;

extern RoadRuntimeState g_ROAD_RUNTIME_STATE;
extern uint8           *g_TRACK_RUNTIME_DATA;
extern void            *g_SECONDARY_ROAD_RENDER_CCB;

void
initialize_road_data(const char *track_name_);
void
shutdown_road_data(void);
void
initialize_road_resource_packets(void);
void
synchronize_road_section_stream(int32 section_index_);
uint8 *
get_road_slice_address(int32 section_index_);
RoadSectionGeometry *
get_road_section_geometry_address(int32 section_index_);
uint8 *
get_road_section_address(int32 section_index_);
