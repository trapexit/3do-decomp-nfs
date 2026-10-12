#pragma once

#include "types.h"

#define CONTROL_INPUT_EVENT_TABLE_ENTRY_COUNT 36
#define CONTROL_INPUT_EVENT_UTILITY_PAD_COUNT 3

extern uint32 g_CONTROL_INPUT_EVENT_TABLE[CONTROL_INPUT_EVENT_TABLE_ENTRY_COUNT];

int32
initialize_control_input_event_table(void);
