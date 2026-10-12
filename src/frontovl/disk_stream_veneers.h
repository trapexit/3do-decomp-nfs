#pragma once

#include "filestream.h"

Stream *
open_runtime_disk_stream(char *path_, int32 buffer_size_);
int32
read_runtime_disk_stream(Stream *stream_, char *buffer_, int32 byte_count_);
int32
seek_runtime_disk_stream(Stream *stream_, int32 offset_, enum SeekOrigin origin_);
void
close_runtime_disk_stream(Stream *stream_);
