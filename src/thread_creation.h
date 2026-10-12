#pragma once

#include "kernel.h"
#include "types.h"

typedef void (*ThreadEntryPoint)(void);

typedef struct ThreadCreationState
{
  Item   item;
  void  *stack;
  uint32 reserved_08;
  int32  priority;
} ThreadCreationState;

void
create_thread_with_stack(const char          *name_,
                         int32                priority_offset_,
                         ThreadEntryPoint     entry_point_,
                         void                *stack_,
                         int32                stack_byte_count_,
                         ThreadCreationState *state_);
