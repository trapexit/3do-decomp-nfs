#pragma once

#include "filestreamfunctions.h"
#include "types.h"

int32
read_disk_stream_elements(void   *destination_,
                          int32   element_size_,
                          int32   element_count_,
                          Stream *stream_);
