#pragma once

#include "types.h"

typedef void (*TaskEntryPoint)(void);

typedef struct TaskStackAllocation
{
  Item   item;
  void  *stack;
  uint32 aligned_stack_byte_count;
} TaskStackAllocation;

Item
create_task_with_allocated_stack(TaskEntryPoint       entry_point_,
                                 int32                stack_byte_count_,
                                 int32                priority_,
                                 const char          *name_,
                                 TaskStackAllocation *allocation_);
int32
release_task_stack_allocation(TaskStackAllocation *allocation_);
