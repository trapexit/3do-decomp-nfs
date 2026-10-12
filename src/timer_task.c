#include "timer_task.h"

#include "diagnostics.h"
#include "display_graphics.h"
#include "graphics_folio.h"
#include "io_offset_request.h"
#include "item_cleanup.h"
#include "memory.h"
#include "shutdown_callbacks.h"
#include "tick_timer.h"
#include "timer_task_callbacks.h"

#include "graphics.h"

#define TIMER_TASK_STACK_BYTE_COUNT 0x400
#define TIMER_TASK_PRIORITY_OFFSET 10
#define TIMER_TASK_FIELDS_PER_WAIT 1
#define TIMER_TASK_TICK_RATE 100
#define TIMER_TASK_DEADLINE_RATE 18

TimerTaskStackState g_TIMER_TASK_STACK_STATE = { NULL, TIMER_TASK_STACK_BYTE_COUNT, NULL };
ThreadCreationState g_TIMER_TASK_THREAD_STATE;

void
initialize_timer_task(void)
{
  int32 callback_index;

  if(g_TIMER_TASK_THREAD_STATE.item != 0)
    return;

  if(open_graphics_folio() != 0)
    fatal_error("inittimer - OPENGRAPHICSFOLIO FAILED\n");

  for(callback_index = 0; callback_index < TIMER_TASK_CALLBACK_COUNT; callback_index++)
    {
      g_TIMER_TASK_CALLBACKS[callback_index] = NULL;
    }

  if(g_TIMER_TASK_STACK_STATE.stack == NULL)
    {
      g_TIMER_TASK_STACK_STATE.allocation =
        allocate_named_memory("Timer Stack", g_TIMER_TASK_STACK_STATE.stack_byte_count, 0);
      g_TIMER_TASK_STACK_STATE.stack = g_TIMER_TASK_STACK_STATE.allocation;
    }

  create_thread_with_stack("Timer Task",
                           TIMER_TASK_PRIORITY_OFFSET,
                           run_timer_task,
                           g_TIMER_TASK_STACK_STATE.stack,
                           g_TIMER_TASK_STACK_STATE.stack_byte_count,
                           &g_TIMER_TASK_THREAD_STATE);
  register_shutdown_callback(shutdown_timer_task);
  reset_tick_state();
}

void
shutdown_timer_task(void)
{
  delete_item_and_clear(&g_TIMER_TASK_THREAD_STATE.item);
  if(g_TIMER_TASK_STACK_STATE.allocation != NULL)
    free_memory(g_TIMER_TASK_STACK_STATE.allocation);
  g_TIMER_TASK_STACK_STATE.allocation = NULL;
}

void
run_timer_task(void)
{
  int32 callback_index;
  int32 deadline_accumulator;
  int32 tick_accumulator;
  Item  vbl_io_request;

  vbl_io_request = GetVBLIOReq();
  if(vbl_io_request < 0)
    fatal_error("inittimer - GETVBLIOREQ FAILED\n");

  tick_accumulator     = 0;
  deadline_accumulator = 0;
  for(;;)
    {
      send_io_offset_request(vbl_io_request, TIMER_TASK_FIELDS_PER_WAIT);

      tick_accumulator += TIMER_TASK_TICK_RATE;
      while(tick_accumulator >= 0)
        {
          tick_accumulator -= g_DISPLAY_FIELD_FREQUENCY;
          g_CURRENT_TICK++;
          for(callback_index = 0; callback_index < TIMER_TASK_CALLBACK_COUNT; callback_index++)
            {
              if(g_TIMER_TASK_CALLBACKS[callback_index] != NULL)
                g_TIMER_TASK_CALLBACKS[callback_index]();
            }
        }

      deadline_accumulator += TIMER_TASK_DEADLINE_RATE;
      if(deadline_accumulator >= 0)
        {
          deadline_accumulator -= g_DISPLAY_FIELD_FREQUENCY;
          g_TICK_DEADLINE++;
        }
    }
}
