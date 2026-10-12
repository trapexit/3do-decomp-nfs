#include "data_stream_state.h"
#include "data_stream_subscriber_requests.h"

#include "stddef.h"

#define STREAMER_CONTROL_DATA_TYPE             0x5354524D
#define STREAMER_CONTROL_SUBSCRIBER_NOT_FOUND -974

int32
find_data_stream_subscriber(DSStreamCBPtr stream_, DSDataType stream_data_type, DSSubscriberPtr *subscriber_result_)
{
  int32 subscriber_index;
  DataStreamSubscriber *subscriber;
  DataStreamState *stream_state;

  stream_state = (DataStreamState *)stream_;
  for (subscriber_index = 0;
       subscriber_index < stream_state->num_subscribers;
       subscriber_index += 1)
  {
    subscriber = &stream_state->subscribers[subscriber_index];
    if (subscriber->stream_type == stream_data_type)
    {
      *subscriber_result_ = (DSSubscriberPtr)subscriber;
      return 0;
    }
  }

  *subscriber_result_ = NULL;
  if (stream_data_type == STREAMER_CONTROL_DATA_TYPE)
    return STREAMER_CONTROL_SUBSCRIBER_NOT_FOUND;

  return STREAM_SUBSCRIBER_RETAIL_MISS_ERROR;
}