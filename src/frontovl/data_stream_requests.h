#pragma once

#include "memory_pool.h"

#include "datastream.h"

int32
send_data_stream_request(Item            message_item_,
                         boolean         asynchronous_,
                         DSStreamCBPtr   stream_,
                         DSRequestMsgPtr request_);
int32
broadcast_to_data_stream_subscribers(DSStreamCBPtr    stream_,
                                     MemoryPool      *message_pool_,
                                     SubscriberMsgPtr message_);
int32
subscribe_to_data_stream(Item            message_item_,
                         DSRequestMsgPtr request_,
                         DSStreamCBPtr   stream_,
                         DSDataType      stream_type_,
                         Item            subscriber_port_);
int32
preroll_data_stream(Item message_item_, DSRequestMsgPtr request_, DSStreamCBPtr stream_);
int32
start_data_stream(Item            message_item_,
                  DSRequestMsgPtr request_,
                  DSStreamCBPtr   stream_,
                  uint32          options_);
int32
stop_data_stream(Item            message_item_,
                 DSRequestMsgPtr request_,
                 DSStreamCBPtr   stream_,
                 uint32          options_);
int32
go_to_data_stream_marker(Item            message_item_,
                         DSRequestMsgPtr request_,
                         DSStreamCBPtr   stream_,
                         uint32          marker_value_,
                         uint32          options_);
int32
get_data_stream_channel(Item            message_item_,
                        DSRequestMsgPtr request_,
                        DSStreamCBPtr   stream_,
                        DSDataType      stream_type_,
                        int32           channel_number_,
                        int32          *channel_status_);
int32
set_data_stream_channel(Item            message_item_,
                        DSRequestMsgPtr request_,
                        DSStreamCBPtr   stream_,
                        DSDataType      stream_type_,
                        int32           channel_number_,
                        int32           channel_status_);
int32
control_data_stream(Item            message_item_,
                    DSRequestMsgPtr request_,
                    DSStreamCBPtr   stream_,
                    DSDataType      stream_type_,
                    int32           operation_,
                    void           *argument_);
int32
connect_data_stream(Item            message_item_,
                    DSRequestMsgPtr request_,
                    DSStreamCBPtr   stream_,
                    Item            acquisition_port_);
int32
wait_for_data_stream_end(Item message_item_, DSRequestMsgPtr request_, DSStreamCBPtr stream_);
int32
synchronize_data_stream_clock(DSStreamCBPtr stream_,
                              MemoryPool   *message_pool_,
                              uint32        current_time_);
