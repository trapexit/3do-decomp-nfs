#include "streaming_thread.h"
#include "tracked_memory.h"

#include "datastream.h"

void
release_data_stream_message_pools(DSStreamCBPtr stream_)
{
  FREE_SECONDARY_MEMORY(stream_->subsMsgPool);
  FREE_SECONDARY_MEMORY(stream_->dataMsgPool);
}
