/* Frontovl allocation and initialization of runtime memory buffers. */

#include "runtime_memory_buffer.h"

#include "kernel_memory.h"

#include "kernel.h"
#include "mem.h"

#define RUNTIME_MEMORY_BUFFER_CONTEXT_BYTE_COUNT 0xA0
#define RUNTIME_MEMORY_BUFFER_CONTENT_BYTE_COUNT 0x400
#define RUNTIME_MEMORY_BUFFER_ALLOCATION_FLAGS 0x100

void *
AllocBuffer(RuntimeMemoryBuffer *buffer_)
{
  void *memory;

  if(buffer_ == NULL)
    return NULL;

  memory                = allocate_memory_from_lists(KernelBase->kb_CurrentTask->t_FreeMemoryLists,
                                                     RUNTIME_MEMORY_BUFFER_CONTENT_BYTE_COUNT,
                                                     0);
  buffer_->owned_buffer = memory;
  if(memory != NULL)
    {
      buffer_->primary_buffer          = memory;
      buffer_->owned_buffer_byte_count = RUNTIME_MEMORY_BUFFER_CONTENT_BYTE_COUNT;
    }

  return memory;
}

RuntimeMemoryBuffer *
create_runtime_memory_buffer(void)
{
  RuntimeMemoryBuffer *buffer;

  buffer = (RuntimeMemoryBuffer *) allocate_memory_from_lists(
    KernelBase->kb_CurrentTask->t_FreeMemoryLists,
    RUNTIME_MEMORY_BUFFER_CONTEXT_BYTE_COUNT,
    RUNTIME_MEMORY_BUFFER_ALLOCATION_FLAGS);
  if(buffer == NULL)
    return NULL;

  if(AllocBuffer(buffer) != NULL)
    return buffer;

  FreeMemToMemLists(KernelBase->kb_CurrentTask->t_FreeMemoryLists,
                    buffer,
                    RUNTIME_MEMORY_BUFFER_CONTEXT_BYTE_COUNT);
  return NULL;
}
