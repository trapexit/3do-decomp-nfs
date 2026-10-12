#include "streaming_thread.h"

#include "kernel_item.h"
#include "runtime_error.h"
#include "runtime_memory.h"
#include "runtime_memory_ops.h"
#include "streaming_thread_stack.h"
#include "tracked_memory.h"

#include "item.h"
#include "kernel.h"
#include "mem.h"
#include "task.h"

#define STREAMING_STACK_ALIGNMENT 4
#define STREAMING_THREAD_NAME_SIZE 32
#define STREAMING_THREAD_TAG_COUNT 9
#define STREAMING_THREAD_MAX_QUANTUM 0x4000
#define STREAMING_THREAD_NODE_SIZE 0
#define STREAMING_THREAD_GUARD "Daredevil"
#define STREAMING_THREAD_MEMORY_NAME "stack"
#define STREAMING_THREAD_DEFAULT_NAME "Thread"
#define STREAMING_THREAD_DISPOSE_ERROR "DisposeThread error:"
#define THREAD_PRIORITY_TAG_INDEX 0
#define THREAD_ENTRY_TAG_INDEX 1
#define THREAD_STACK_SIZE_TAG_INDEX 2
#define THREAD_STACK_POINTER_TAG_INDEX 3
#define THREAD_ARGUMENT_COUNT_TAG_INDEX 4
#define THREAD_ARGUMENT_TAG_INDEX 5
#define THREAD_QUANTUM_TAG_INDEX 6
#define THREAD_NAME_TAG_INDEX 7
#define THREAD_END_TAG_INDEX 8

Item
create_streaming_thread(StreamingThreadEntry thread_entry_,
                        int32                stack_size_,
                        int32                priority_,
                        const char          *thread_name_,
                        void               **stack_memory_out_,
                        int32                argument_count_,
                        void                *argument_)
{
  Item   thread;
  char  *stack_memory;
  int32  aligned_stack_size;
  TagArg tags[STREAMING_THREAD_TAG_COUNT];
  char   generated_name[STREAMING_THREAD_NAME_SIZE];

  aligned_stack_size =
    (stack_size_ + STREAMING_STACK_ALIGNMENT - 1) & ~(STREAMING_STACK_ALIGNMENT - 1);

  *stack_memory_out_ = NULL;
  stack_memory =
    ALLOCATE_NAMED_SECONDARY_MEMORY((uint32) aligned_stack_size, STREAMING_THREAD_MEMORY_NAME);
  copy_runtime_memory(stack_memory, STREAMING_THREAD_GUARD, sizeof(STREAMING_THREAD_GUARD));
  if(stack_memory == NULL)
    {
      return 0;
    }

  *stack_memory_out_ = stack_memory;

  tags[THREAD_PRIORITY_TAG_INDEX].ta_Tag       = TAG_ITEM_PRI;
  tags[THREAD_PRIORITY_TAG_INDEX].ta_Arg       = (void *) priority_;
  tags[THREAD_ENTRY_TAG_INDEX].ta_Tag          = CREATETASK_TAG_PC;
  tags[THREAD_ENTRY_TAG_INDEX].ta_Arg          = (void *) thread_entry_;
  tags[THREAD_STACK_SIZE_TAG_INDEX].ta_Tag     = CREATETASK_TAG_STACKSIZE;
  tags[THREAD_STACK_SIZE_TAG_INDEX].ta_Arg     = (void *) aligned_stack_size;
  tags[THREAD_STACK_POINTER_TAG_INDEX].ta_Tag  = CREATETASK_TAG_SP;
  tags[THREAD_STACK_POINTER_TAG_INDEX].ta_Arg  = stack_memory + aligned_stack_size;
  tags[THREAD_ARGUMENT_COUNT_TAG_INDEX].ta_Tag = CREATETASK_TAG_ARGC;
  tags[THREAD_ARGUMENT_COUNT_TAG_INDEX].ta_Arg = (void *) argument_count_;
  tags[THREAD_ARGUMENT_TAG_INDEX].ta_Tag       = CREATETASK_TAG_ARGP;
  tags[THREAD_ARGUMENT_TAG_INDEX].ta_Arg       = argument_;
  tags[THREAD_QUANTUM_TAG_INDEX].ta_Tag        = CREATETASK_TAG_MAXQ;
  tags[THREAD_QUANTUM_TAG_INDEX].ta_Arg        = (void *) STREAMING_THREAD_MAX_QUANTUM;
  tags[THREAD_NAME_TAG_INDEX].ta_Tag           = TAG_ITEM_NAME;
  if(thread_name_ == NULL)
    {
      tags[THREAD_NAME_TAG_INDEX].ta_Arg = make_thread_name(generated_name,
                                                            sizeof(generated_name),
                                                            STREAMING_THREAD_DEFAULT_NAME,
                                                            (int32) stack_memory);
    }
  else
    {
      tags[THREAD_NAME_TAG_INDEX].ta_Arg = (void *) thread_name_;
    }
  tags[THREAD_END_TAG_INDEX].ta_Tag = TAG_END;

  thread = CreateSizedItem(MKNODEID(KERNELNODE, TASKNODE), tags, STREAMING_THREAD_NODE_SIZE);
  if(thread < 0)
    {
      free_streaming_thread_stack(stack_memory);
      *stack_memory_out_ = NULL;
    }

  return thread;
}

int32
dispose_streaming_thread(Item thread_item_)
{
  int32 status;
  void *checked_item;

  checked_item = check_kernel_item(thread_item_, KERNELNODE, TASKNODE);
  status       = DeleteItem(thread_item_);
  if(status < 0 && checked_item != NULL)
    {
      runtime_debug_printf(STREAMING_THREAD_DISPOSE_ERROR);
      print_runtime_error(status);
    }

  return status;
}
