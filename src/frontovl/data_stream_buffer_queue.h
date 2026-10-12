#pragma once

#include "datastream.h"

void
reply_to_data_stream_completion_request(DSStreamCBPtr stream_);
void
return_data_stream_buffer(DSStreamCBPtr stream_, DSDataBufPtr buffer_);
DSDataBufPtr
take_free_data_stream_buffer(DSStreamCBPtr stream_);
DSDataBufPtr
take_filled_data_stream_buffer(DSStreamCBPtr stream_);
int32
request_data_stream_buffer_fill(DSStreamCBPtr stream_, DSDataBufPtr buffer_);
void
abort_data_stream(DSStreamCBPtr stream_);
int32
release_data_stream_chunk(DSStreamCBPtr stream_, int32 subscriber_status_, DSDataBufPtr buffer_);
