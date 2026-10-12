#include "data_stream_buffer_queue.h"
#include "data_stream_state.h"
#include "data_stream_subscriber_broadcast.h"

enum
{
  K_DATA_STREAM_ACTIVE_STATE_MASK = 0xD,
  K_DATA_STREAM_ABORTING_STATE    = 0x100
};

void
abort_data_stream(DSStreamCBPtr stream_)
{
  SubscriberMsg    message;
  int32            pending_reply_count;
  DataStreamState *stream_state;

  stream_state = (DataStreamState *) stream_;
  stream_->streamFlags &= ~K_DATA_STREAM_ACTIVE_STATE_MASK;
  stream_->streamFlags |= K_DATA_STREAM_ABORTING_STATE;
  message.whatToDo   = kStreamOpAbort;
  message.privatePtr = NULL;

  pending_reply_count = stream_state->subscriber_replies_pending;
  send_message_to_data_stream_subscribers(stream_, &message);
  stream_state->subscriber_replies_pending = pending_reply_count;
}
