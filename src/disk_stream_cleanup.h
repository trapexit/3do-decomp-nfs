#pragma once

#include "filestream.h"
#include "types.h"

Stream *
open_disk_stream_default(char *path_);
int32
close_disk_stream_and_return_zero(Stream *stream_);
