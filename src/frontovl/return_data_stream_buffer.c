#include "data_stream_buffer_queue.h"

void
return_data_stream_buffer(DSStreamCBPtr stream_, DSDataBufPtr buffer_)
{
  buffer_->next        = stream_->freeBufHead;
  stream_->freeBufHead = buffer_;
  stream_->currentFreeBufferCount += 1;
  reply_to_data_stream_completion_request(stream_);
}
