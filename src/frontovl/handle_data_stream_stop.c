#include "data_stream_state.h"
#include "data_stream_subscriber_broadcast.h"
#include "data_stream_subscriber_requests.h"

enum
{
  K_DATA_STREAM_ACTIVE_STATE_MASK     = 0xD,
  K_DATA_STREAM_CLOCK_WAS_VALID_STATE = 0x10
};

int32
handle_data_stream_stop(DSStreamCBPtr stream_, DSRequestMsgPtr request_)
{
  SubscriberMsg    message;
  DataStreamState *stream_state;

  stream_state = (DataStreamState *) stream_;
  if((stream_->streamFlags & K_DATA_STREAM_CLOCK_WAS_VALID_STATE) != 0)
    {
      get_data_stream_clock(stream_state, &stream_state->last_valid_clock);
    }
  stream_->streamFlags &= ~K_DATA_STREAM_ACTIVE_STATE_MASK;

  message.whatToDo         = kStreamOpStop;
  message.privatePtr       = request_;
  message.msg.stop.options = request_->msg.stop.options;
  send_message_to_data_stream_subscribers(stream_, &message);
  return 0;
}
