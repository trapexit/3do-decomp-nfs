#include "control_input_event_table.h"

#include "event.h"
#include "operror.h"

#define CONTROL_INPUT_EVENT_VALUE_SHIFT 16

uint32 g_CONTROL_INPUT_EVENT_TABLE[CONTROL_INPUT_EVENT_TABLE_ENTRY_COUNT];

int32
initialize_control_input_event_table(void)
{
  Err   result;
  int32 entry_index;

  result = InitEventUtility(CONTROL_INPUT_EVENT_UTILITY_PAD_COUNT, 0, 0);
  if(result < 0)
    {
      PrintfSysErr(result);
      return 0;
    }

  for(entry_index = 0; entry_index < CONTROL_INPUT_EVENT_TABLE_ENTRY_COUNT; entry_index++)
    {
      g_CONTROL_INPUT_EVENT_TABLE[entry_index] =
        ((uint32) entry_index << CONTROL_INPUT_EVENT_VALUE_SHIFT);
    }

  return 1;
}
