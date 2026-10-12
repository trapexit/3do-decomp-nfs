#pragma once

#include "types.h"

#define TRACK_CONFIGURATION_SLOT_COUNT 24

extern void *g_TRACK_CONFIGURATION_SLOTS[TRACK_CONFIGURATION_SLOT_COUNT];

void
set_track_configuration_slot(int32 slot_index_, void *configuration_);
