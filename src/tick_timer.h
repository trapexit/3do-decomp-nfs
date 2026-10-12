#pragma once

#include "types.h"

extern int32 g_CURRENT_TICK;
extern int32 g_TICK_BASELINE;
extern int32 g_TICK_DEADLINE;
extern int32 g_PREVIOUS_TICK;

int32
get_current_tick(void);
int32
get_elapsed_ticks(int32 start_tick_);
int32
sample_tick_delta(void);
void
reset_tick_state(void);
void
set_tick_deadline(int32 delay_ticks_);
void
wait_for_tick_deadline(void);
int32
is_tick_deadline_reached(void);
void
wait_ticks(int32 duration_ticks_);
