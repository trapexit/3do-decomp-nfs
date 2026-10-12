#include "disk_stream_cleanup.h"

#include "filestreamfunctions.h"

Stream *
open_disk_stream_default(char *path_)
{
  return OpenDiskStream(path_, 0);
}

int32
close_disk_stream_and_return_zero(Stream *stream_)
{
  CloseDiskStream(stream_);
  return 0;
}
