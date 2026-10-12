#include "shutdown_callbacks.h"

#include "control_input.h"
#include "platform_shutdown.h"

#include "svc_funcs.h"

#define SHUTDOWN_MODE_DEFAULT 1
#define SHUTDOWN_MODE_FORCED 2
#define SHUTDOWN_TRIGGER_BUTTON 0x200
ShutdownCallback g_SHUTDOWN_CALLBACKS[SHUTDOWN_CALLBACK_CAPACITY] = { NULL };
int32            g_SHUTDOWN_MODE                                  = SHUTDOWN_MODE_DEFAULT;

void
run_shutdown_callbacks(void)
{
  int32            callback_index;
  ShutdownCallback callback;

  if((g_SHUTDOWN_MODE == SHUTDOWN_MODE_FORCED) ||
     ((g_SHUTDOWN_MODE == SHUTDOWN_MODE_DEFAULT) &&
      ((read_primary_control_pad_buttons() & SHUTDOWN_TRIGGER_BUTTON) != 0)))
    {
      notify_platform_shutdown();
    }

  for(callback_index = SHUTDOWN_CALLBACK_CAPACITY - 1; callback_index >= 0; callback_index--)
    {
      callback = g_SHUTDOWN_CALLBACKS[callback_index];
      if(callback != NULL)
        callback();
      g_SHUTDOWN_CALLBACKS[callback_index] = NULL;
    }

  svc_ExitTask(0);
}

void
register_shutdown_callback(ShutdownCallback callback_)
{
  int32 callback_index;

  for(callback_index = 0; callback_index < SHUTDOWN_CALLBACK_CAPACITY; callback_index++)
    {
      if((g_SHUTDOWN_CALLBACKS[callback_index] == NULL) ||
         (g_SHUTDOWN_CALLBACKS[callback_index] == callback_))
        {
          g_SHUTDOWN_CALLBACKS[callback_index] = callback_;
          return;
        }
    }
}
