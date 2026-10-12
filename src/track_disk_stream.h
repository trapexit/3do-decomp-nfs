#pragma once

#include "road_data.h"

#include "filestream.h"
#include "types.h"

#define TRACK_STREAM_HEADER_BYTES 12
#define TRACK_SECTION_OFFSETS_OFFSET 0x98C
#define TRACK_SECTION_BUFFER_OFFSET 0x2C
#define TRACK_SECTION_COUNT 600
#define TRACK_STREAM_BUFFER_BYTES 0x40000
#define TRACK_STREAM_HEADER_MAGIC 0x54524B44

typedef struct TrackDiskStreamState
{
  Stream *volatile stream;
  void *buffer;
  int32 current_section;
  int32 requested_section;
  int32 section_buffer_capacity;
  int32 lookahead_section_count;
  int32 active;
} TrackDiskStreamState;

typedef struct TrackData
{
  uint8 reserved_00[4];
  int32 section_count;
  uint8 reserved_08[0x10];
  int32 initial_surface_incline;
  uint8 reserved_1_c[4];
  int32 initial_heading;
  uint8 reserved_24[4];
  int32 stream_layout_mode;
  int32 section_buffer_offsets[TRACK_SECTION_COUNT];
  int32 section_offsets[1];
} TrackData;

typedef struct TrackStreamHeader
{
  int32 magic;
  int32 payload_bytes;
  int32 section_metadata;
} TrackStreamHeader;

extern TrackDiskStreamState g_TRACK_DISK_STREAM_STATE;
extern TrackDiskStreamState g_TRACK_DISK_STREAM_SNAPSHOT;
extern int32                g_TRACK_STREAM_READ_ERROR_COUNT;
extern int32                g_ACTIVE_ROAD_SECTION_INDEX;

#define G_TRACK_DATA ((TrackData *) g_ROAD_RUNTIME_STATE.track_data)

void
initialize_track_disk_stream(char *path_, int32 buffer_size_);
void
shutdown_track_disk_stream(void);
int32
read_track_stream_bytes(void *destination_, int32 byte_count_);
int32
read_track_stream_header(void *destination_);
int32
seek_track_stream_section(int32 section_);
int32
load_track_stream_section(int32 section_);
void
update_track_stream_window(int32 section_);
