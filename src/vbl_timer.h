#pragma once

#include "types.h"

extern uint32 g_PREVIOUS_VBL_NUMBER;
extern uint32 g_RENDER_VBL_DEADLINE;

uint32
get_vbl_number(void);
uint32
get_render_vbl_number(void);
void
schedule_render_vbl_deadline(uint32 vbl_count_);
void
wait_for_render_vbl_deadline(void);
int32
is_render_vbl_deadline_reached(void);
void
wait_for_render_vbl_count(uint32 vbl_count_);
uint32
sample_vbl_delta(void);
