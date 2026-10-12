#pragma once

#include "datastream.h"

void
queue_data_stream_request(DSStreamCBPtr stream_, DSRequestMsgPtr request_);
DSRequestMsgPtr
take_data_stream_request(DSStreamCBPtr stream_);
