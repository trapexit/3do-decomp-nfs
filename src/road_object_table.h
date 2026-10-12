#pragma once

#include "road_data.h"

#include "types.h"

#define ROAD_OBJECT_SCAN_LIMIT 1000
#define ROAD_OBJECT_RECORDS_PER_SECTION 4
#define ROAD_OBJECT_SEARCH_DIVISOR 2

typedef struct RoadObjectRecord
{
  int32 record_id;
  uint8 definition_index;
  uint8 heading;
  uint8 reserved_06[2];
  uint8 cel_frame_table_select;
  uint8 reserved_09;
  int16 offset_x;
  int16 offset_y;
  int16 offset_z;
} RoadObjectRecord;

typedef struct RoadObjectDefinition
{
  uint8 reserved_00;
  uint8 mesh_type;
  uint8 render_resource_index;
  uint8 extended_cel_index;
  int32 width;
  int32 length;
  int32 height;
} RoadObjectDefinition;

typedef struct RoadObjectProcessingState
{
  int32 projected_vertex_count;
  int32 mesh_vertex_count;
  int32 render_vertex_cursor;
  int32 first_record_index;
  int32 current_record_index;
  int32 current_record_id;
  int32 processed_record_count;
} RoadObjectProcessingState;

extern RoadObjectProcessingState g_ROAD_OBJECT_PROCESSING_STATE;
extern int32                     g_ROAD_OBJECT_RECORD_COUNT;

#define G_ROAD_OBJECT_RECORDS ((RoadObjectRecord *) g_ROAD_RUNTIME_STATE.road_object_records)
#define G_ROAD_OBJECT_DEFINITIONS                                                                  \
  ((RoadObjectDefinition *) g_ROAD_RUNTIME_STATE.road_object_definitions)

void
count_road_object_records(void);
void
initialize_road_object_processing(int32 section_index_);
void
prepare_road_object_render_range(void);
int32
activate_nearby_road_object(void);
int32
is_road_object_near_active_vehicle(const Vector3 *position_);
int32
seek_road_object_record(int32 record_id_);
