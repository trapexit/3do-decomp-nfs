#include "track_configuration_slots.h"

void *g_TRACK_CONFIGURATION_SLOTS[TRACK_CONFIGURATION_SLOT_COUNT];

void
set_track_configuration_slot(int32 slot_index_, void *configuration_)
{
  g_TRACK_CONFIGURATION_SLOTS[slot_index_] = configuration_;
}
