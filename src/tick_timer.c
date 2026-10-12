#include "tick_timer.h"

int32 g_TICK_BASELINE = 0;
int32 g_PREVIOUS_TICK = 0;
int32 g_TICK_DEADLINE = 0;

int32
get_elapsed_ticks(int32 start_tick_)
{
  return get_current_tick() - start_tick_;
}

int32
sample_tick_delta(void)
{
  int32 current_tick;
  int32 previous_tick;

  previous_tick   = g_PREVIOUS_TICK;
  current_tick    = get_current_tick();
  g_PREVIOUS_TICK = current_tick;
  return current_tick - previous_tick;
}

void
reset_tick_state(void)
{
  g_TICK_DEADLINE = 0;
  g_CURRENT_TICK  = 0;
  g_PREVIOUS_TICK = 0;
  g_TICK_BASELINE = 0;
}

void
set_tick_deadline(int32 delay_ticks_)
{
  g_TICK_DEADLINE = get_current_tick() + delay_ticks_;
}

void
wait_for_tick_deadline(void)
{
  while(!is_tick_deadline_reached())
    {
    }
}

int32
is_tick_deadline_reached(void)
{
  return get_current_tick() - g_TICK_DEADLINE >= 0;
}

void
wait_ticks(int32 duration_ticks_)
{
  int32 target_tick;

  target_tick = get_current_tick() + duration_ticks_;
  while(get_current_tick() - target_tick < 0)
    {
    }
}
