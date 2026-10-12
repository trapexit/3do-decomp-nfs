/* Frontovl wrapper for waiting on a message-port item. */

#include "wait_for_kernel_port.h"

#include "msgport.h"

Item
wait_for_kernel_port(Item message_port_, Item message_item_)
{
  return WaitPort(message_port_, message_item_);
}
