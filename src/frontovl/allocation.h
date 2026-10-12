#pragma once

#include "types.h"

typedef int32 AllocationMode;

#define ALLOCATION_MODE_PRIMARY ((AllocationMode) 1)
#define ALLOCATION_MODE_SECONDARY ((AllocationMode) 2)

extern AllocationMode g_ALLOCATION_MODE;

void
set_allocation_mode(AllocationMode mode_);
