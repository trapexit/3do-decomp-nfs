#include "control_subscriber.h"
#include "memory_pool.h"

#define CONTROL_SUBSCRIBER_CONTEXT_SIZE 0xE0

int32
initialize_control_subscriber(void)
{
  g_CONTROL_SUBSCRIBER_CONTEXT_POOL = create_memory_pool(
      CONTROL_SUBSCRIBER_CONTEXT_COUNT,
      CONTROL_SUBSCRIBER_CONTEXT_SIZE);
  if (g_CONTROL_SUBSCRIBER_CONTEXT_POOL == 0)
    return -1;

  return 0;
}