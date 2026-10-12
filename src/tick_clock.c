#include "tick_timer.h"

int32 g_CURRENT_TICK = 0;

int32
get_current_tick(void)
{
  return g_CURRENT_TICK;
}
