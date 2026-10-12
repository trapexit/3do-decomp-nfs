#include "data_stream_buffer_queue.h"

DSDataBufPtr
take_free_data_stream_buffer(DSStreamCBPtr stream_)
{
  DSDataBufPtr buffer;

  buffer = stream_->freeBufHead;
  if(buffer != NULL)
    {
      stream_->freeBufHead = buffer->next;
      buffer->useCount     = 0;
      buffer->streamCBPtr  = stream_;
      stream_->currentFreeBufferCount -= 1;
    }
  return buffer;
}
