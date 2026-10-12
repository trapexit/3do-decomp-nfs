#include "data_stream_buffer_queue.h"
#include "data_stream_state.h"
#include "data_stream_subscriber_requests.h"

#include "msgport.h"

int32
register_data_stream_completion_request(DSStreamCBPtr stream_, DSRequestMsgPtr request_)
{
  DSRequestMsgPtr  prior_request;
  DataStreamState *stream_state;

  stream_state  = (DataStreamState *) stream_;
  prior_request = stream_state->end_of_stream_request;
  if(prior_request != NULL)
    {
      ReplyMsg(prior_request->msgItem, kDSEOSRegistrationErr, prior_request, sizeof(DSRequestMsg));
    }

  stream_state->end_of_stream_request = request_;
  reply_to_data_stream_completion_request(stream_);
  return 0;
}
