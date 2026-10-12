#include "data_stream_buffers.h"
#include "runtime_memory.h"

enum
{
  K_STREAM_QUADBYTE_SIZE          = 4,
  K_STREAM_QUADBYTE_ROUNDING_MASK = K_STREAM_QUADBYTE_SIZE - 1,
  K_STREAM_ALLOCATION_ATTRIBUTES  = 0x20
};

static const char G_DATA_STREAM_BUFFER_ALLOCATION_NAME[] = "firstBp";

DSDataBufPtr
create_data_stream_buffer_list(int32 buffer_count_, int32 buffer_byte_count_)
{
  int32        aligned_buffer_byte_count;
  int32        entry_byte_count;
  int32        total_byte_count;
  int32        buffer_index;
  DSDataBufPtr buffers;
  DSDataBufPtr buffer;

  aligned_buffer_byte_count =
    (buffer_byte_count_ + K_STREAM_QUADBYTE_ROUNDING_MASK) & ~K_STREAM_QUADBYTE_ROUNDING_MASK;
  entry_byte_count = sizeof(DSDataBuf) + aligned_buffer_byte_count;
  total_byte_count = buffer_count_ * entry_byte_count;
  buffers          = (DSDataBufPtr) allocate_runtime_memory(G_DATA_STREAM_BUFFER_ALLOCATION_NAME,
                                                            total_byte_count,
                                                            K_STREAM_ALLOCATION_ATTRIBUTES);
  if(buffers == NULL)
    {
      return NULL;
    }

  buffer = buffers;
  for(buffer_index = 0; buffer_index < buffer_count_ - 1; buffer_index++)
    {
      buffer->next          = (DSDataBufPtr) ((uint8 *) buffer + entry_byte_count);
      buffer->permanentNext = buffer->next;
      buffer                = buffer->next;
    }
  buffer->next          = NULL;
  buffer->permanentNext = NULL;

  return buffers;
}

int32
dispose_data_stream_buffer_list(DSDataBufPtr buffers_)
{
  volatile int32 status;

  status = free_runtime_memory(buffers_);
  return status;
}
