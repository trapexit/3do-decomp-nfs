#pragma once

#include "graphics.h"

#define SIMULATION_COMMON_ART_CEL_COUNT 4

extern void *g_SIMULATION_COMMON_ART;
extern CCB  *g_SIMULATION_COMMON_ART_CELS[SIMULATION_COMMON_ART_CEL_COUNT];

void
load_simulation_common_art(void);
void
initialize_simulation_common_art_cels(void);
