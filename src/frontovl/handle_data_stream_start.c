#include "data_stream_buffer_queue.h"
#include "data_stream_state.h"
#include "data_stream_subscriber_broadcast.h"
#include "data_stream_subscriber_requests.h"

enum
{
  K_DATA_STREAM_RUNNING_STATE            = 0x1,
  K_DATA_STREAM_CLOCK_VALID_STATE        = 0x4,
  K_DATA_STREAM_CLOCK_WAS_VALID_STATE    = 0x10,
  K_DATA_STREAM_ABORTED_STATE            = 0x100,
  K_START_DATA_STREAM_RESET_CLOCK_OPTION = 0x2
};

int32
handle_data_stream_start(DSStreamCBPtr stream_, DSRequestMsgPtr request_)
{
  SubscriberMsg    message;
  DataStreamState *stream_state;

  if((stream_->streamFlags & K_DATA_STREAM_RUNNING_STATE) != 0)
    {
      return kDSWasRunningErr;
    }
  if((stream_->streamFlags & K_DATA_STREAM_ABORTED_STATE) != 0)
    {
      abort_data_stream(stream_);
      return kDSAbortErr;
    }

  stream_state = (DataStreamState *) stream_;
  stream_->streamFlags |= K_DATA_STREAM_RUNNING_STATE;
  stream_->streamFlags &= ~K_DATA_STREAM_CLOCK_VALID_STATE;
  if((stream_->streamFlags & K_DATA_STREAM_CLOCK_WAS_VALID_STATE) != 0)
    {
      set_data_stream_clock(stream_state, stream_state->last_valid_clock);
    }
  if((request_->msg.start.options & K_START_DATA_STREAM_RESET_CLOCK_OPTION) != 0)
    {
      set_data_stream_clock(stream_state, 0);
    }

  message.whatToDo          = kStreamOpStart;
  message.privatePtr        = request_;
  message.msg.start.options = request_->msg.start.options;
  send_message_to_data_stream_subscribers(stream_, &message);
  return 0;
}
