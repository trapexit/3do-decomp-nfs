#include "data_stream_state.h"

#include "sdk_veneers.h"

#define STREAM_BRANCH_IN_PROGRESS 0x08
#define STREAM_ABSOLUTE_BRANCH 0x20
#define STREAM_RUNNING 0x001
#define STREAM_END_OF_FILE 0x002
#define STREAM_CLOCK_VALID 0x004
#define STREAM_CLOCK_WAS_VALID 0x010
#define STREAM_ABORTED 0x100
#define STREAM_STOPPED 0x200

#define STREAM_ABORT_ERROR (-1000)
#define STREAM_END_OF_FILE_ERROR (-999)
#define STREAM_WAS_RUNNING_ERROR (-996)
#define STREAM_NOT_RUNNING_ERROR (-997)
#define STREAM_CLOCK_INVALID_ERROR (-212)

int32
is_data_stream_marker(DataStreamState *stream_, uint32 marker_value_, boolean *is_marker_)
{
  if((stream_->flags & STREAM_BRANCH_IN_PROGRESS) != 0 &&
     (stream_->flags & STREAM_ABSOLUTE_BRANCH) != 0)
    {
      stream_->flags &= ~(STREAM_BRANCH_IN_PROGRESS | STREAM_ABSOLUTE_BRANCH);
      *is_marker_ = 1;
      return 0;
    }

  if((stream_->flags & STREAM_BRANCH_IN_PROGRESS) != 0 &&
     marker_value_ == stream_->branch_destination)
    {
      stream_->flags &= ~STREAM_BRANCH_IN_PROGRESS;
      *is_marker_ = 1;
    }
  else
    {
      *is_marker_ = 0;
    }

  return 0;
}

boolean
has_filled_data_stream_buffers(DataStreamState *stream_)
{
  if(stream_->filled_buffer_head != NULL)
    {
      return 1;
    }

  return 0;
}

int32
get_filled_data_stream_buffer_count(DataStreamState *stream_)
{
  int32             buffer_count;
  DataStreamBuffer *buffer;

  if((stream_->flags & STREAM_RUNNING) != 0)
    {
      return STREAM_WAS_RUNNING_ERROR;
    }
  if((stream_->flags & STREAM_ABORTED) != 0)
    {
      return STREAM_ABORT_ERROR;
    }
  if((stream_->flags & (STREAM_END_OF_FILE | STREAM_STOPPED)) != 0)
    {
      return STREAM_END_OF_FILE_ERROR;
    }
  if(stream_->filled_buffer_head == NULL)
    {
      return 0;
    }

  buffer_count = 1;
  buffer       = stream_->filled_buffer_head;
  while(buffer != stream_->filled_buffer_tail)
    {
      buffer = buffer->next;
      buffer_count += 1;
    }

  return buffer_count;
}

boolean
is_data_stream_at_end(DataStreamState *stream_)
{
  uint32 end_state;

  end_state = STREAM_END_OF_FILE | STREAM_STOPPED;
  if((stream_->flags & end_state) == end_state)
    {
      return 1;
    }

  return 0;
}

int32
set_data_stream_absolute_branch(DataStreamState *stream_)
{
  stream_->flags |= STREAM_BRANCH_IN_PROGRESS | STREAM_ABSOLUTE_BRANCH;
  return 0;
}

int32
set_data_stream_branch_destination(DataStreamState *stream_, uint32 branch_destination_)
{
  stream_->flags &= ~STREAM_BRANCH_IN_PROGRESS;
  stream_->branch_destination = branch_destination_;
  stream_->flags |= STREAM_BRANCH_IN_PROGRESS;
  return 0;
}

int32
set_data_stream_clock(DataStreamState *stream_, uint32 stream_clock_)
{
  if((stream_->flags & STREAM_RUNNING) == 0)
    {
      return STREAM_NOT_RUNNING_ERROR;
    }

  stream_->clock_offset = (GetAudioTime() - stream_clock_);
  stream_->flags |= (STREAM_CLOCK_VALID | STREAM_CLOCK_WAS_VALID);
  return 0;
}

int32
get_data_stream_clock(DataStreamState *stream_, uint32 *stream_clock_)
{
  if((stream_->flags & STREAM_CLOCK_VALID) == 0)
    {
      return STREAM_CLOCK_INVALID_ERROR;
    }

  *stream_clock_ = GetAudioTime() - stream_->clock_offset;
  return 0;
}
