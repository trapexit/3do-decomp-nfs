#pragma once

#include "types.h"

extern uint32 g_SYSTEM_FREE_MEMORY_BYTES;
extern uint32 g_TASK_FREE_MEMORY_BYTES;

void
update_free_memory_counts(void);
