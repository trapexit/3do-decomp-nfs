#pragma once

#include "types.h"

extern int32 g_PENDING_SIMULATION_TRACK_POSITION;

void
apply_pending_simulation_track_position(void);
void
set_pending_simulation_track_position(int32 track_position_);
void
set_pending_simulation_track_section(int32 section_index_);
void
relocate_simulation_track_position(int32 track_position_);
