#include "data_stream_request_queue.h"
#include "data_stream_state.h"

#include "datastream.h"

DSRequestMsgPtr
take_data_stream_request(DSStreamCBPtr stream_)
{
  DSRequestMsgPtr  request;
  DataStreamState *stream_state;

  stream_state = (DataStreamState *) stream_;
  request      = stream_state->request_head;
  if(request != NULL)
    {
      stream_state->request_head = (DSRequestMsgPtr) request->link;
      if(stream_state->request_tail == request)
        {
          stream_state->request_tail = NULL;
        }
    }
  return request;
}
