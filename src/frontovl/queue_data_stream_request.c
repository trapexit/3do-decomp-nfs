#include "data_stream_request_queue.h"
#include "data_stream_state.h"

#include "datastream.h"

void
queue_data_stream_request(DSStreamCBPtr stream_, DSRequestMsgPtr request_)
{
  DataStreamState *stream_state;

  stream_state   = (DataStreamState *) stream_;
  request_->link = NULL;
  if(stream_state->request_head == NULL)
    {
      stream_state->request_head = request_;
      stream_state->request_tail = request_;
    }
  else
    {
      stream_state->request_tail->link = request_;
      stream_state->request_tail       = request_;
    }
}
