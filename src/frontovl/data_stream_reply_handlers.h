#pragma once

#include "data_stream_state.h"

#include "datastream.h"
#include "msgport.h"

void
handle_data_stream_subscriber_reply(DataStreamState *stream_,
                                    SubscriberMsgPtr subscriber_message_,
                                    Message         *reply_message_);
void
handle_data_stream_acquisition_reply(DataStreamState *stream_,
                                     DataAcqMsgPtr    acquisition_message_,
                                     Message         *reply_message_);
