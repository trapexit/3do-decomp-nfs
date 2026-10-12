#pragma once

#include "datastream.h"

DSDataBufPtr
create_data_stream_buffer_list(int32 buffer_count_, int32 buffer_byte_count_);
int32
dispose_data_stream_buffer_list(DSDataBufPtr buffers_);
