#pragma once

#include "datastream.h"

int32
send_message_to_data_stream_subscribers(DSStreamCBPtr stream_, SubscriberMsgPtr message_);
