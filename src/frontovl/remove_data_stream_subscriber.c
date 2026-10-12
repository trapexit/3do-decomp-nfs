#include "data_stream_state.h"
#include "data_stream_subscriber_requests.h"

void
remove_data_stream_subscriber(DSStreamCBPtr stream_, DSSubscriberPtr subscriber_)
{
  int32                 entries_to_move;
  DataStreamSubscriber *entry;
  DataStreamState      *stream_state;

  stream_state    = (DataStreamState *) stream_;
  entry           = (DataStreamSubscriber *) subscriber_;
  entries_to_move = stream_state->num_subscribers - (entry - stream_state->subscribers) - 1;
  while(entries_to_move > 0)
    {
      *entry = *(entry + 1);
      entry += 1;
      entries_to_move -= 1;
    }
  stream_state->num_subscribers -= 1;
}
