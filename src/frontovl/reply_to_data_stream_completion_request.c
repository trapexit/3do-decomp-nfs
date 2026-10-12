#include "data_stream_buffer_queue.h"
#include "data_stream_state.h"

#include "msgport.h"

enum
{
  K_DATA_STREAM_COMPLETION_STATE_MASK = 0xA02
};

void
reply_to_data_stream_completion_request(DSStreamCBPtr stream_)
{
  DSRequestMsgPtr  request;
  DataStreamState *stream_state;

  stream_state = (DataStreamState *) stream_;
  request      = stream_state->end_of_stream_request;
  if((request != NULL) && ((stream_->streamFlags & K_DATA_STREAM_COMPLETION_STATE_MASK) != 0) &&
     (stream_state->completion_replies_pending == 0))
    {
      ReplyMsg(request->msgItem, 0, request, sizeof(DSRequestMsg));
      stream_state->end_of_stream_request = NULL;
    }
}
