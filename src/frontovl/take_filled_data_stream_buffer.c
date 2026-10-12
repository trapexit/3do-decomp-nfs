#include "data_stream_buffer_queue.h"

DSDataBufPtr
take_filled_data_stream_buffer(DSStreamCBPtr stream_)
{
  DSDataBufPtr buffer;

  buffer = stream_->filledBufHead;
  if(buffer != NULL)
    {
      stream_->filledBufHead = buffer->next;
      if(stream_->filledBufTail == buffer)
        {
          stream_->filledBufTail = NULL;
        }
    }
  return buffer;
}
