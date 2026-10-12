#pragma once

#include "data_stream_state.h"

#include "datastream.h"

int32
deliver_data_stream(DataStreamState *stream_);
int32
deliver_data_stream_buffer_chunks(DataStreamState *stream_,
                                  DSDataBufPtr     buffer_,
                                  void            *first_chunk_);
