#pragma once

#include "types.h"

typedef struct RuntimeMemoryBuffer
{
  uint8  reserved_before_primary_buffer[0x10];
  void  *primary_buffer;
  uint8  reserved_before_owned_buffer[0x04];
  void  *owned_buffer;
  uint32 owned_buffer_byte_count;
  uint8  reserved_after_owned_buffer[0x80];
} RuntimeMemoryBuffer;

void *
AllocBuffer(RuntimeMemoryBuffer *buffer_);
RuntimeMemoryBuffer *
create_runtime_memory_buffer(void);
