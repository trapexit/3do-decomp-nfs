#pragma once

#include "thread_creation.h"

#include "types.h"

typedef struct TimerTaskStackState
{
  void  *stack;
  uint32 stack_byte_count;
  void  *allocation;
} TimerTaskStackState;

extern TimerTaskStackState g_TIMER_TASK_STACK_STATE;
extern ThreadCreationState g_TIMER_TASK_THREAD_STATE;

void
initialize_timer_task(void);
void
shutdown_timer_task(void);
void
run_timer_task(void);
