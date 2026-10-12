#include "streaming_thread_stack.h"

#include "runtime_memory.h"
#include "string_compare.h"
#include "tracked_memory.h"

#define STREAMING_THREAD_STACK_GUARD "Daredevil"

void
free_streaming_thread_stack(void *stack_memory_)
{
  if(compare_byte_strings((const uint8 *) stack_memory_,
                          (const uint8 *) STREAMING_THREAD_STACK_GUARD) != 0)
    {
      runtime_debug_printf("YEOOOOWWWWWW - movie stack overflow\n");
    }

  FREE_SECONDARY_MEMORY(stack_memory_);
}
