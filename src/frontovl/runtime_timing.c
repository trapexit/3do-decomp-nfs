/* Frontovl vertical-blank timer wait entry points. */

#include "runtime_timing.h"

#include "runtime_io.h"

#include "io.h"

#include "string.h"
#include "time.h"

Err
send_vblank_delay(Item timer_request_, uint32 tick_count_);

Err
wait_runtime_timer_ticks(Item timer_request_, uint32 tick_count_)
{
  Err status;

  status = send_vblank_delay(timer_request_, tick_count_);
  if(status < 0)
    {
      return status;
    }

  return wait_for_io_completion(timer_request_);
}

Err
send_vblank_delay(Item timer_request_, uint32 tick_count_)
{
  IOInfo io_info;

  memset(&io_info, 0, sizeof(io_info));
  io_info.ioi_Command = TIMERCMD_DELAY;
  io_info.ioi_Offset  = tick_count_;
  return SendIO(timer_request_, &io_info);
}
