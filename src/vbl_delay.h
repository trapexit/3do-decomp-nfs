#pragma once

#include "kernel.h"
#include "types.h"

extern Item g_VBL_TIMER_IO_REQUEST;
extern Item g_GAME_VBL_IO_REQUEST;

Err
delay_vbls(int32 field_count_);
Err
delay_game_vbls(int32 field_count_);
void
wait_for_vbls(int32 field_count_);
