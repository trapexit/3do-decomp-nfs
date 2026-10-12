/* Frontovl thread creation with caller-owned stack-list allocation. */

#include "runtime_thread.h"

#include "kernel_memory.h"

#include "item.h"
#include "kernel.h"
#include "kernelnodes.h"
#include "mem.h"
#include "operror.h"
#include "task.h"

#define RUNTIME_THREAD_STACK_ALIGNMENT 16
#define RUNTIME_THREAD_PRIORITY_TAG_INDEX 0
#define RUNTIME_THREAD_NAME_TAG_INDEX 1
#define RUNTIME_THREAD_PC_TAG_INDEX 2
#define RUNTIME_THREAD_STACK_SIZE_TAG_INDEX 3
#define RUNTIME_THREAD_STACK_POINTER_TAG_INDEX 4
#define RUNTIME_THREAD_ALLOCATED_STACK_TAG_INDEX 5
#define RUNTIME_THREAD_END_TAG_INDEX 6
#define RUNTIME_THREAD_TAG_COUNT 7
#define RUNTIME_THREAD_EMPTY_NAME_TAG 0xFF

Item
create_runtime_thread(const char *name_,
                      uint8       priority_,
                      void        (*entry_point_)(void),
                      int32       stack_byte_count_)
{
  TagArg tags[RUNTIME_THREAD_TAG_COUNT];
  int32  aligned_stack_byte_count;
  uint8 *stack_memory;

  aligned_stack_byte_count = (stack_byte_count_ + (RUNTIME_THREAD_STACK_ALIGNMENT - 1)) &
                             ~(RUNTIME_THREAD_STACK_ALIGNMENT - 1);
  stack_memory = (uint8 *) allocate_memory_from_lists(KernelBase->kb_CurrentTask->t_FreeMemoryLists,
                                                      aligned_stack_byte_count,
                                                      0);
  if(stack_memory == NULL)
    return NOMEM;

  tags[RUNTIME_THREAD_PRIORITY_TAG_INDEX].ta_Tag = TAG_ITEM_PRI;
  tags[RUNTIME_THREAD_PRIORITY_TAG_INDEX].ta_Arg = (void *) (uint32) priority_;
  if(name_ == NULL)
    tags[RUNTIME_THREAD_NAME_TAG_INDEX].ta_Tag = RUNTIME_THREAD_EMPTY_NAME_TAG;
  else
    {
      tags[RUNTIME_THREAD_NAME_TAG_INDEX].ta_Tag = TAG_ITEM_NAME;
      tags[RUNTIME_THREAD_NAME_TAG_INDEX].ta_Arg = (void *) name_;
    }

  tags[RUNTIME_THREAD_PC_TAG_INDEX].ta_Tag            = CREATETASK_TAG_PC;
  tags[RUNTIME_THREAD_PC_TAG_INDEX].ta_Arg            = (void *) entry_point_;
  tags[RUNTIME_THREAD_STACK_SIZE_TAG_INDEX].ta_Tag    = CREATETASK_TAG_STACKSIZE;
  tags[RUNTIME_THREAD_STACK_SIZE_TAG_INDEX].ta_Arg    = (void *) (uint32) aligned_stack_byte_count;
  tags[RUNTIME_THREAD_STACK_POINTER_TAG_INDEX].ta_Tag = CREATETASK_TAG_SP;
  tags[RUNTIME_THREAD_STACK_POINTER_TAG_INDEX].ta_Arg =
    (void *) (stack_memory + aligned_stack_byte_count);
  tags[RUNTIME_THREAD_ALLOCATED_STACK_TAG_INDEX].ta_Tag = CREATETASK_TAG_ALLOCDTHREADSP;
  tags[RUNTIME_THREAD_ALLOCATED_STACK_TAG_INDEX].ta_Arg = NULL;
  tags[RUNTIME_THREAD_END_TAG_INDEX].ta_Tag             = TAG_END;
  return CreateSizedItem(MKNODEID(KERNELNODE, TASKNODE), tags, 0);
}

Err
delete_runtime_thread(Item thread_item_)
{
  Task *thread;
  void *stack_memory;
  int32 stack_byte_count;
  Err   status;

  thread = (Task *) LookupItem(thread_item_);
  if(thread == NULL)
    return BADITEM;

  stack_memory     = thread->t_StackBase;
  stack_byte_count = thread->t_StackSize;
  status           = DeleteItem(thread_item_);
  if(status == 0)
    {
      FreeMemToMemLists(KernelBase->kb_CurrentTask->t_FreeMemoryLists,
                        stack_memory,
                        stack_byte_count);
    }

  return status;
}
