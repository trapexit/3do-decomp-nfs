#include "road_data.h"

#include "game_session_resources.h"
#include "memory.h"
#include "render_thread_signal.h"
#include "road_object_table.h"
#include "signal_status.h"
#include "simulation_snapshot.h"
#include "track_disk_stream.h"
#include "track_position.h"
#include "track_processing_cache.h"
#include "track_stream_worker.h"
#include "vbl_delay.h"
#include "vehicle_runtime_pool.h"

#include "stddef.h"
#include "stdio.h"

#define ROAD_TRACK_PATH_BYTES 64
#define ROAD_TRACK_PATH_FORMAT "DriveData/tracks/%s.trk"
#define ROAD_TRACK_DATA_ALLOCATION_NAME "RoadSection"
#define ROAD_TRACK_DATA_BYTES 0x16C44
#define ROAD_TRACK_DATA_MEMORY_FLAGS 0x20
#define ROAD_TRACK_DISK_BUFFER_BYTES 0xA000
#define ROAD_TRACK_REFERENCE_RECORDS_OFFSET 0x13B4
#define ROAD_TRACK_SEGMENTS_OFFSET 0x13B4
#define ROAD_TRACK_RUNTIME_DATA_OFFSET 0x16534
#define ROAD_OBJECT_TABLE_INDEX_OFFSET 0x16C3C
#define ROAD_STREAM_OFFSET_MASK 0x1FFFF
#define ROAD_STREAM_STANDARD_LOOKAHEAD 6
#define ROAD_STREAM_ALTERNATE_LOOKAHEAD 2
#define ROAD_STREAM_STANDARD_BUFFER_BYTES 0x221D4
#define ROAD_STREAM_ALTERNATE_BUFFER_BYTES 0x20000
#define ROAD_STREAM_BUFFER_ALLOCATION_NAME "RoadSlices"
#define ROAD_OBJECT_ALLOCATION_NAME "RoadObjects"
#define ROAD_OBJECT_RECORD_BYTES 16
#define ROAD_RESOURCE_PACKET_ZERO_INDEX 0
#define ROAD_RESOURCE_PACKET_ONE_INDEX 1
#define ROAD_RESOURCE_WAIT_FIELDS 1
#define ROAD_STREAM_RENDER_WAIT_COMMAND 1
#define ROAD_TRACK_POSITION_UNITS_PER_SECTION 4
#define ROAD_STREAM_RING_SECTION_COUNT 13
#define ROAD_SLICE_BYTES 600
#define ROAD_INITIAL_SLICE_REGION_BYTES 0x1C20
#define ROAD_SECTION_GEOMETRY_REGION_OFFSET 0x1E78
#define ROAD_SECTION_GEOMETRY_BYTES 0x9C
#define ROAD_TRACK_HEADER_WORD_BYTES 4

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

RoadRuntimeState     g_ROAD_RUNTIME_STATE;
TrackDiskStreamState g_TRACK_DISK_STREAM_SNAPSHOT;
int32                g_ACTIVE_ROAD_SECTION_INDEX;
uint8               *g_TRACK_RUNTIME_DATA;
void                *g_SECONDARY_ROAD_RENDER_CCB;

extern void *g_ROAD_RESOURCE_PACKET_ZERO;
extern void *g_ROAD_RESOURCE_PACKET_ONE;
extern void *g_ROAD_RENDER_CCB;

COMPILE_TIME_SIZE_ASSERT(road_runtime_state_size, sizeof(RoadRuntimeState) == 0x54);
COMPILE_TIME_SIZE_ASSERT(vehicle_runtime_pool_road_render_state_offset,
                         offsetof(VehicleRuntimePoolState, road_render_state) == 0xE4);
COMPILE_TIME_SIZE_ASSERT(road_runtime_object_definitions_offset,
                         offsetof(RoadRuntimeState, road_object_definitions) == 0x44);
COMPILE_TIME_SIZE_ASSERT(road_runtime_track_data_offset,
                         offsetof(RoadRuntimeState, track_data) == 0x4C);
COMPILE_TIME_SIZE_ASSERT(track_data_section_count_offset,
                         offsetof(TrackData, section_count) == 0x04);
COMPILE_TIME_SIZE_ASSERT(track_data_stream_layout_mode_offset,
                         offsetof(TrackData, stream_layout_mode) == 0x28);
COMPILE_TIME_SIZE_ASSERT(track_data_buffer_offsets_offset,
                         offsetof(TrackData, section_buffer_offsets) ==
                           TRACK_SECTION_BUFFER_OFFSET);
COMPILE_TIME_SIZE_ASSERT(track_data_section_offsets_offset,
                         offsetof(TrackData, section_offsets) == TRACK_SECTION_OFFSETS_OFFSET);
COMPILE_TIME_SIZE_ASSERT(simulation_projection_track_position_offset,
                         offsetof(SimulationProjectionSource, track_section_position) == 0xF8);
COMPILE_TIME_SIZE_ASSERT(road_section_geometry_size,
                         sizeof(RoadSectionGeometry) == ROAD_SECTION_GEOMETRY_BYTES);

static void
_configure_track_stream_buffer(void);
static void
_load_road_objects(void);
static void
_initialize_track_runtime_pointers(void);
static int32
_get_current_road_section_index(void);
static int32
_is_road_stream_window_ready(int32 section_index_);

void
initialize_road_data(const char *track_name_)
{
  char       track_path[ROAD_TRACK_PATH_BYTES];
  TrackData *track_data;

  sprintf(track_path, ROAD_TRACK_PATH_FORMAT, track_name_);
  track_data = (TrackData *) allocate_named_memory(ROAD_TRACK_DATA_ALLOCATION_NAME,
                                                   ROAD_TRACK_DATA_BYTES,
                                                   ROAD_TRACK_DATA_MEMORY_FLAGS);
  g_ROAD_RUNTIME_STATE.track_data = track_data;
  g_REFERENCE_DISTANCE_RECORDS =
    (ReferenceDistanceRecord *) ((uint8 *) track_data + ROAD_TRACK_REFERENCE_RECORDS_OFFSET);

  initialize_track_disk_stream(track_path, ROAD_TRACK_DISK_BUFFER_BYTES);
  read_track_stream_bytes(g_ROAD_RUNTIME_STATE.track_data, ROAD_TRACK_DATA_BYTES);
  _configure_track_stream_buffer();
  _load_road_objects();
  initialize_signal_statuses(
    *(int32 *) ((uint8 *) G_TRACK_DATA + TRACK_SIGNAL_STATUS_COUNT_OFFSET));
  count_road_object_records();
  invalidate_track_processing_cache();
  update_track_stream_window(0);
  _initialize_track_runtime_pointers();
  g_TRACK_DISK_STREAM_STATE.active = 1;
}

void
shutdown_road_data(void)
{
  shutdown_track_disk_stream();
  free_memory(g_ROAD_RUNTIME_STATE.road_objects);
  free_memory(g_ROAD_RUNTIME_STATE.track_data);
}

void
initialize_road_resource_packets(void)
{
  open_game_session_stream(g_ROAD_RUNTIME_STATE.track_name);
  while(g_TRACK_DISK_STREAM_STATE.active == 0)
    wait_for_vbls(ROAD_RESOURCE_WAIT_FIELDS);

  do
    {
      g_RENDER_THREAD_STATE.road_resource_packet_zero =
        acquire_game_session_resource_packet(g_GAME_SESSION_RESOURCE_BUFFER_INDEX,
                                             ROAD_RESOURCE_PACKET_ZERO_INDEX);
      if(g_RENDER_THREAD_STATE.road_resource_packet_zero == NULL)
        wait_for_vbls(ROAD_RESOURCE_WAIT_FIELDS);
    }
  while(g_RENDER_THREAD_STATE.road_resource_packet_zero == NULL);

  do
    {
      g_RENDER_THREAD_STATE.road_resource_packet_one =
        acquire_game_session_resource_packet(g_GAME_SESSION_RESOURCE_BUFFER_INDEX,
                                             ROAD_RESOURCE_PACKET_ONE_INDEX);
      if(g_RENDER_THREAD_STATE.road_resource_packet_one == NULL)
        wait_for_vbls(ROAD_RESOURCE_WAIT_FIELDS);
    }
  while(g_RENDER_THREAD_STATE.road_resource_packet_one == NULL);
}

void
synchronize_road_section_stream(int32 section_index_)
{
  int32 section_distance;

  g_ACTIVE_ROAD_SECTION_INDEX = section_index_;
  section_distance            = (g_TRACK_DISK_STREAM_STATE.current_section - section_index_);
  if(section_distance != g_TRACK_DISK_STREAM_STATE.lookahead_section_count)
    {
      request_track_stream_window(section_index_);
      while(_is_road_stream_window_ready(section_index_) == 0)
        {
          send_render_io_command(ROAD_STREAM_RENDER_WAIT_COMMAND);
          request_track_stream_window(section_index_);
        }
    }

  g_TRACK_DISK_STREAM_SNAPSHOT                   = g_TRACK_DISK_STREAM_STATE;
  g_VEHICLE_RUNTIME_POOL_STATE.road_render_state = 0;
}

uint8 *
get_road_slice_address(int32 section_index_)
{
  int32 current_section;
  int32 relative_section;
  int32 slice_offset;

  current_section = (g_FRONTEND_RENDER_STATE.projection.source->track_section_position /
                     ROAD_TRACK_POSITION_UNITS_PER_SECTION);
  relative_section = (section_index_ - current_section);
  if((relative_section >= 0) && (relative_section < ROAD_STREAM_RING_SECTION_COUNT))
    {
      slice_offset = (G_TRACK_DATA->section_buffer_offsets[current_section] +
                      (relative_section * ROAD_SLICE_BYTES));
    }
  else
    {
      slice_offset =
        *(int32 *) ((uint8 *) G_TRACK_DATA + ((section_index_ - 1) * ROAD_TRACK_HEADER_WORD_BYTES));
      slice_offset += ROAD_INITIAL_SLICE_REGION_BYTES;
    }

  return (uint8 *) g_TRACK_DISK_STREAM_STATE.buffer + slice_offset;
}

RoadSectionGeometry *
get_road_section_geometry_address(int32 section_index_)
{
  int32 current_section;
  int32 relative_section;
  int32 geometry_offset;

  current_section = (g_FRONTEND_RENDER_STATE.projection.source->track_section_position /
                     ROAD_TRACK_POSITION_UNITS_PER_SECTION);
  geometry_offset =
    (G_TRACK_DATA->section_buffer_offsets[current_section] + ROAD_SECTION_GEOMETRY_REGION_OFFSET);
  if(section_index_ <= current_section)
    {
      relative_section = ((current_section + 1) - section_index_);
      geometry_offset += (relative_section * ROAD_SECTION_GEOMETRY_BYTES);
    }

  return (RoadSectionGeometry *) ((uint8 *) g_TRACK_DISK_STREAM_STATE.buffer + geometry_offset);
}

uint8 *
get_road_section_address(int32 section_index_)
{
  return (uint8 *) g_TRACK_DISK_STREAM_STATE.buffer +
         G_TRACK_DATA->section_buffer_offsets[section_index_];
}

static void
_configure_track_stream_buffer(void)
{
  int32 section_index;
  int32 buffer_bytes;

  if(G_TRACK_DATA->stream_layout_mode == 0)
    {
      g_TRACK_DISK_STREAM_STATE.lookahead_section_count = ROAD_STREAM_STANDARD_LOOKAHEAD;
      for(section_index = 0; section_index < G_TRACK_DATA->section_count; section_index += 1)
        {
          G_TRACK_DATA->section_buffer_offsets[section_index] &= ROAD_STREAM_OFFSET_MASK;
        }
      buffer_bytes = ROAD_STREAM_STANDARD_BUFFER_BYTES;
    }
  else
    {
      g_TRACK_DISK_STREAM_STATE.lookahead_section_count = ROAD_STREAM_ALTERNATE_LOOKAHEAD;
      buffer_bytes                                      = ROAD_STREAM_ALTERNATE_BUFFER_BYTES;
    }

  g_TRACK_DISK_STREAM_STATE.section_buffer_capacity =
    (g_TRACK_DISK_STREAM_STATE.lookahead_section_count + 1);
  g_TRACK_DISK_STREAM_STATE.buffer =
    allocate_named_memory(ROAD_STREAM_BUFFER_ALLOCATION_NAME, buffer_bytes, 0);
  g_ROAD_RUNTIME_STATE.road_slice_buffer = (uint8 *) g_TRACK_DISK_STREAM_STATE.buffer;
}

static void
_load_road_objects(void)
{
  TrackStreamHeader header;
  int32             object_table_index;

  read_track_stream_header(&header);
  g_ROAD_RUNTIME_STATE.road_objects =
    (uint8 *) allocate_named_memory(ROAD_OBJECT_ALLOCATION_NAME, header.payload_bytes, 0);
  read_track_stream_bytes(g_ROAD_RUNTIME_STATE.road_objects,
                          header.payload_bytes - TRACK_STREAM_HEADER_BYTES);
  g_ROAD_RUNTIME_STATE.road_object_definitions = g_ROAD_RUNTIME_STATE.road_objects;
  object_table_index =
    *(int32 *) ((uint8 *) G_TRACK_DATA + ROAD_OBJECT_TABLE_INDEX_OFFSET);
  g_ROAD_RUNTIME_STATE.road_object_records =
    g_ROAD_RUNTIME_STATE.road_objects + (object_table_index * ROAD_OBJECT_RECORD_BYTES);
}

static void
_initialize_track_runtime_pointers(void)
{
  g_TRACK_SEGMENTS = (TrackSegment *) ((uint8 *) G_TRACK_DATA + ROAD_TRACK_SEGMENTS_OFFSET);
  g_TRACK_SEGMENTS[0].surface_incline   = (int16) G_TRACK_DATA->initial_surface_incline;
  g_TRACK_SEGMENTS[0].heading           = (int16) G_TRACK_DATA->initial_heading;
  g_TRACK_SEGMENTS[0].surface_reference = g_REFERENCE_DISTANCE_RECORDS[0].position;
  g_TRACK_RUNTIME_DATA = (uint8 *) G_TRACK_DATA + ROAD_TRACK_RUNTIME_DATA_OFFSET;
}

static int32
_get_current_road_section_index(void)
{
  return (g_FRONTEND_RENDER_STATE.projection.source->track_section_position /
          ROAD_TRACK_POSITION_UNITS_PER_SECTION);
}

static int32
_is_road_stream_window_ready(int32 section_index_)
{
  int32 current_distance;
  int32 requested_distance;

  current_distance   = (g_TRACK_DISK_STREAM_STATE.current_section - section_index_);
  requested_distance = (g_TRACK_DISK_STREAM_STATE.requested_section - section_index_);
  return ((current_distance >= (g_TRACK_DISK_STREAM_STATE.lookahead_section_count - 1)) &&
          (current_distance <= g_TRACK_DISK_STREAM_STATE.section_buffer_capacity) &&
          (requested_distance <= g_TRACK_DISK_STREAM_STATE.section_buffer_capacity));
}
