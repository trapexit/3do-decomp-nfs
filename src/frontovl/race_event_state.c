/* Frontovl race-event state predicates. */

#include "race_event_state.h"

#include "race_state.h"

#define FIRST_LATE_RACE_EVENT_INDEX 2

int32
is_late_event_or_novice_skill(void)
{
  if(RACE_CONFIGURATION->event_index >= FIRST_LATE_RACE_EVENT_INDEX)
    {
      return 1;
    }

  if(RACE_CONFIGURATION->event_index != 1)
    {
      return 0;
    }

  return (RACE_CONFIGURATION->skill_level == 0);
}
