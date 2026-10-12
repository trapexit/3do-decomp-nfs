/* Frontovl microsecond conversion through the public service veneer. */

#include "microseconds_to_ticks.h"

#include "svc_funcs.h"

int32
_USecToTicks(int32 microseconds_);

int32
microseconds_to_ticks(int32 microseconds_)
{
  return _USecToTicks(microseconds_);
}
