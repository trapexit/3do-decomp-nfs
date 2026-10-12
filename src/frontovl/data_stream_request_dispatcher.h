#pragma once

#include "data_stream_state.h"

#include "datastream.h"

int32
dispatch_data_stream_request(DataStreamState *stream_,
                             DSRequestMsgPtr  request_,
                             boolean         *keep_running_);
