#include "task_stack_allocation.h"

#include "memory.h"

#include "item.h"
#include "kernelnodes.h"
#include "operror.h"
#include "task.h"

#define TASK_STACK_ALIGNMENT 4
#define TASK_STACK_ALIGNMENT_PADDING 3U
#define TASK_STACK_ALIGNMENT_MASK 0xFFFFFFFCU
#define TASK_STACK_MEMORY_FLAGS 0
#define TASK_ERROR_TEXT_BYTE_COUNT 256
#define TASK_BASE_PRIORITY 0x50
#define TASK_MAXIMUM_QUANTUM 0x4000
#define TASK_PRIORITY_TAG_INDEX 0
#define TASK_QUANTUM_TAG_INDEX 1
#define TASK_ENTRY_TAG_INDEX 2
#define TASK_STACK_SIZE_TAG_INDEX 3
#define TASK_STACK_POINTER_TAG_INDEX 4
#define TASK_ARGUMENT_COUNT_TAG_INDEX 5
#define TASK_ARGUMENT_POINTER_TAG_INDEX 6
#define TASK_NAME_TAG_INDEX 7
#define TASK_END_TAG_INDEX 8
#define TASK_TAG_COUNT 9

Item
create_task_with_allocated_stack(TaskEntryPoint       entry_point_,
                                 int32                stack_byte_count_,
                                 int32                priority_,
                                 const char          *name_,
                                 TaskStackAllocation *allocation_)
{
  char   error_text[TASK_ERROR_TEXT_BYTE_COUNT];
  TagArg tags[TASK_TAG_COUNT];
  int32  aligned_stack_byte_count;
  void  *stack;
  Item   item;

  aligned_stack_byte_count = (int32) (((uint32) stack_byte_count_ + TASK_STACK_ALIGNMENT_PADDING) &
                                      TASK_STACK_ALIGNMENT_MASK);
  tags[TASK_PRIORITY_TAG_INDEX].ta_Tag         = TAG_ITEM_PRI;
  tags[TASK_PRIORITY_TAG_INDEX].ta_Arg         = (void *) (uint32) (priority_ + TASK_BASE_PRIORITY);
  tags[TASK_QUANTUM_TAG_INDEX].ta_Tag          = CREATETASK_TAG_MAXQ;
  tags[TASK_QUANTUM_TAG_INDEX].ta_Arg          = (void *) (uint32) TASK_MAXIMUM_QUANTUM;
  tags[TASK_ENTRY_TAG_INDEX].ta_Tag            = CREATETASK_TAG_PC;
  tags[TASK_ENTRY_TAG_INDEX].ta_Arg            = (void *) entry_point_;
  tags[TASK_STACK_SIZE_TAG_INDEX].ta_Tag       = CREATETASK_TAG_STACKSIZE;
  tags[TASK_STACK_SIZE_TAG_INDEX].ta_Arg       = (void *) (uint32) aligned_stack_byte_count;
  tags[TASK_STACK_POINTER_TAG_INDEX].ta_Tag    = CREATETASK_TAG_SP;
  tags[TASK_STACK_POINTER_TAG_INDEX].ta_Arg    = NULL;
  tags[TASK_ARGUMENT_COUNT_TAG_INDEX].ta_Tag   = CREATETASK_TAG_ARGC;
  tags[TASK_ARGUMENT_COUNT_TAG_INDEX].ta_Arg   = NULL;
  tags[TASK_ARGUMENT_POINTER_TAG_INDEX].ta_Tag = CREATETASK_TAG_ARGP;
  tags[TASK_ARGUMENT_POINTER_TAG_INDEX].ta_Arg = NULL;
  tags[TASK_NAME_TAG_INDEX].ta_Tag             = TAG_ITEM_NAME;
  tags[TASK_NAME_TAG_INDEX].ta_Arg             = (void *) name_;
  tags[TASK_END_TAG_INDEX].ta_Tag              = TAG_END;
  tags[TASK_END_TAG_INDEX].ta_Arg              = NULL;

  stack = allocate_named_memory(name_, stack_byte_count_, TASK_STACK_MEMORY_FLAGS);
  if(stack == NULL)
    return 0;

  tags[TASK_STACK_POINTER_TAG_INDEX].ta_Arg = (uint8 *) stack + stack_byte_count_;
  item = CreateSizedItem(MKNODEID(KERNELNODE, TASKNODE), tags, 0);
  if(item < 0)
    {
      GetSysErr(error_text, TASK_ERROR_TEXT_BYTE_COUNT - 1, item);
      error_text[TASK_ERROR_TEXT_BYTE_COUNT - 1] = '\0';
      free_memory(stack);
      return item;
    }

  allocation_->item                     = item;
  allocation_->stack                    = stack;
  allocation_->aligned_stack_byte_count = aligned_stack_byte_count;
  return item;
}

int32
release_task_stack_allocation(TaskStackAllocation *allocation_)
{
  free_memory(allocation_->stack);
  return 0;
}
