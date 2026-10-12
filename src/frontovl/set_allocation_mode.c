#include "allocation.h"

AllocationMode g_ALLOCATION_MODE = ALLOCATION_MODE_PRIMARY;

void
set_allocation_mode(AllocationMode mode_)
{
  g_ALLOCATION_MODE = mode_;
}
