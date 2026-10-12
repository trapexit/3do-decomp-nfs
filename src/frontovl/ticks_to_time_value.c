/* Frontovl tick conversion through the Portfolio C library veneer. */

#include "ticks_to_time_value.h"

/* This Portfolio 1.3 export is absent from the newer public headers. */
void
_TicksToTimeVal(uint32 tick_count_, TimeVal *time_value_);

void
ticks_to_time_value(uint32 tick_count_, TimeVal *time_value_)
{
  _TicksToTimeVal(tick_count_, time_value_);
}
