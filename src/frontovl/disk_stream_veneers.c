/* Frontovl disk-stream entry points. */

#include "disk_stream_veneers.h"

#include "runtime_file_folio.h"

#include "filestreamfunctions.h"

Stream *
open_runtime_disk_stream(char *path_, int32 buffer_size_)
{
  /* Keep the SDK call as a normal return-producing veneer, not a tail call. */
  Stream *volatile stream;

  stream = OpenDiskStream(path_, buffer_size_);
  return stream;
}

int32
read_runtime_disk_stream(Stream *stream_, char *buffer_, int32 byte_count_)
{
  /* Keep the SDK call as a normal return-producing veneer, not a tail call. */
  int32 volatile byte_count;

  byte_count = ReadDiskStream(stream_, buffer_, byte_count_);
  return byte_count;
}

int32
seek_runtime_disk_stream(Stream *stream_, int32 offset_, enum SeekOrigin origin_)
{
  /* Keep the SDK call as a normal return-producing veneer, not a tail call. */
  int32 volatile status;

  status = SeekDiskStream(stream_, offset_, origin_);
  return status;
}

void
close_runtime_disk_stream(Stream *stream_)
{
  (void) GetFileFolio();
  CloseDiskStream(stream_);
}
