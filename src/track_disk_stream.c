#include "track_disk_stream.h"

#include "game_session_resources.h"
#include "memory.h"

#include "filestreamfunctions.h"

void
initialize_track_disk_stream(char *path_, int32 buffer_size_)
{
  g_TRACK_DISK_STREAM_STATE.stream = OpenDiskStream(path_, buffer_size_);
  (void) g_TRACK_DISK_STREAM_STATE.stream;
  g_TRACK_DISK_STREAM_STATE.active          = 0;
  g_TRACK_DISK_STREAM_STATE.current_section = -1;
}

void
shutdown_track_disk_stream(void)
{
  CloseDiskStream(g_TRACK_DISK_STREAM_STATE.stream);
  free_memory(g_TRACK_DISK_STREAM_STATE.buffer);
  g_TRACK_DISK_STREAM_STATE.active = 0;
}

int32
read_track_stream_bytes(void *destination_, int32 byte_count_)
{
  int32 read_count;

  read_count = ReadDiskStream(g_TRACK_DISK_STREAM_STATE.stream, (char *) destination_, byte_count_);
  if((read_count >= 0) && (read_count >= byte_count_))
    return 0;

  g_TRACK_STREAM_READ_ERROR_COUNT++;
  return -1;
}

int32
read_track_stream_header(void *destination_)
{
  return read_track_stream_bytes(destination_, TRACK_STREAM_HEADER_BYTES);
}

int32
seek_track_stream_section(int32 section_)
{
  return SeekDiskStream(g_TRACK_DISK_STREAM_STATE.stream,
                        G_TRACK_DATA->section_offsets[section_],
                        SEEK_SET);
}

int32
load_track_stream_section(int32 section_)
{
  TrackStreamHeader header;
  int32             buffer_offset;
  int32             result;

  if(section_ < 0)
    return 0;

  buffer_offset = G_TRACK_DATA->section_buffer_offsets[section_];
  while(1)
    {
      seek_track_stream_section(section_);
      result = read_track_stream_header(&header);
      if(result < 0)
        continue;
      if(header.magic == TRACK_STREAM_HEADER_MAGIC)
        break;
    }

  if((buffer_offset + header.payload_bytes) > TRACK_STREAM_BUFFER_BYTES)
    return 0;

  result = read_track_stream_bytes((uint8 *) g_TRACK_DISK_STREAM_STATE.buffer + buffer_offset,
                                   header.payload_bytes);
  seek_track_stream_section(section_ + 1);
  return result;
}

void
update_track_stream_window(int32 section_)
{
  int32 distance;

  distance = section_ - (g_TRACK_DISK_STREAM_STATE.current_section -
                         g_TRACK_DISK_STREAM_STATE.lookahead_section_count);
  if(distance < 0)
    distance = -distance;
  if(distance > g_TRACK_DISK_STREAM_STATE.section_buffer_capacity)
    {
      g_TRACK_DISK_STREAM_STATE.current_section =
        (section_ + g_TRACK_DISK_STREAM_STATE.lookahead_section_count) -
        g_TRACK_DISK_STREAM_STATE.section_buffer_capacity;
    }

  while((g_TRACK_DISK_STREAM_STATE.current_section - section_) <
        g_TRACK_DISK_STREAM_STATE.lookahead_section_count)
    {
      if(g_GAME_SESSION_STATE.stream_index != section_)
        return;

      g_TRACK_DISK_STREAM_STATE.requested_section = g_TRACK_DISK_STREAM_STATE.current_section + 1;
      load_track_stream_section(g_TRACK_DISK_STREAM_STATE.current_section);
      g_TRACK_DISK_STREAM_STATE.current_section += 1;
    }

  while((g_TRACK_DISK_STREAM_STATE.current_section - section_) >
        g_TRACK_DISK_STREAM_STATE.lookahead_section_count)
    {
      if(g_GAME_SESSION_STATE.stream_index != section_)
        return;

      g_TRACK_DISK_STREAM_STATE.requested_section = g_TRACK_DISK_STREAM_STATE.current_section - 1;
      load_track_stream_section((g_TRACK_DISK_STREAM_STATE.current_section - 1) -
                                g_TRACK_DISK_STREAM_STATE.section_buffer_capacity);
      g_TRACK_DISK_STREAM_STATE.current_section -= 1;
      seek_track_stream_section(g_TRACK_DISK_STREAM_STATE.current_section -
                                g_TRACK_DISK_STREAM_STATE.section_buffer_capacity);
    }
}
