#pragma once

#include "datastream.h"

#define STREAM_SUBSCRIBER_RETAIL_MISS_ERROR (-985)

int32
find_data_stream_subscriber(DSStreamCBPtr    stream_,
                            DSDataType       stream_type_,
                            DSSubscriberPtr *subscriber_result_);
int32
request_data_stream_channel_status(DSStreamCBPtr stream_, DSRequestMsgPtr request_);
int32
set_data_stream_channel_status(DSStreamCBPtr stream_, DSRequestMsgPtr request_);
int32
handle_internal_data_stream_control(DSStreamCBPtr stream_, DSRequestMsgPtr request_);
int32
control_data_stream_subscriber(DSStreamCBPtr stream_, DSRequestMsgPtr request_);
void
remove_data_stream_subscriber(DSStreamCBPtr stream_, DSSubscriberPtr subscriber_);
int32
close_data_stream(DSStreamCBPtr stream_, DSRequestMsgPtr request_);
int32
register_data_stream_subscriber(DSStreamCBPtr stream_, DSRequestMsgPtr request_);
int32
handle_data_stream_preroll(DSStreamCBPtr stream_, DSRequestMsgPtr request_);
int32
handle_data_stream_start(DSStreamCBPtr stream_, DSRequestMsgPtr request_);
int32
handle_data_stream_stop(DSStreamCBPtr stream_, DSRequestMsgPtr request_);
int32
position_data_stream_at_marker(DSStreamCBPtr stream_, DSRequestMsgPtr request_);
int32
connect_data_stream_acquisition(DSStreamCBPtr stream_, DSRequestMsgPtr request_);
int32
register_data_stream_completion_request(DSStreamCBPtr stream_, DSRequestMsgPtr request_);
