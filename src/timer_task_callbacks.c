#include "timer_task_callbacks.h"

#include "diagnostics.h"

TimerTaskCallback g_TIMER_TASK_CALLBACKS[TIMER_TASK_CALLBACK_COUNT] = { NULL };

void
register_timer_task_callback(TimerTaskCallback callback_)
{
  int32 callback_index;

  for(callback_index = 0; callback_index < TIMER_TASK_CALLBACK_COUNT; callback_index++)
    {
      if(g_TIMER_TASK_CALLBACKS[callback_index] == NULL)
        {
          g_TIMER_TASK_CALLBACKS[callback_index] = callback_;
          return;
        }
    }

  fatal_error("addtimer - TIMER LIST FULL.\n"
              "Maximum 8 timer functions allowed\n");
}

void
unregister_timer_task_callback(TimerTaskCallback callback_)
{
  int32 callback_index;

  for(callback_index = 0; callback_index < TIMER_TASK_CALLBACK_COUNT; callback_index++)
    {
      if(g_TIMER_TASK_CALLBACKS[callback_index] == callback_)
        {
          g_TIMER_TASK_CALLBACKS[callback_index] = NULL;
          return;
        }
    }
}

int32
count_active_timer_task_callbacks(void)
{
  int32 active_callback_count;
  int32 callback_index;

  active_callback_count = 0;
  for(callback_index = 0; callback_index < TIMER_TASK_CALLBACK_COUNT; callback_index++)
    {
      if(g_TIMER_TASK_CALLBACKS[callback_index] != NULL)
        {
          active_callback_count++;
        }
    }

  return active_callback_count;
}
