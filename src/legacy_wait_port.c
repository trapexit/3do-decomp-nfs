/* Legacy clib WaitPort veneer retained by the LaunchMe image. */

#include "legacy_wait_port.h"

#include "kernel.h"
#include "msgport.h"

Item
_WaitPort(Item message_port_, Item message_)
{
  /* Retail dispatches through KernelBase; use the public SDK service here. */
  (void) KernelBase;
  return WaitPort(message_port_, message_);
}
