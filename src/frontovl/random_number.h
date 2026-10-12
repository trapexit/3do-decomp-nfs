#pragma once

#include "types.h"

extern uint32 *g_FRONTEND_RANDOM_VALUE_DESTINATION;

void
generate_frontend_random_values(void);
int32
random_below(int32 upper_bound_);
