#pragma once

#include "types.h"

#define HORIZON_RING_COUNT 2

typedef struct HorizonRing HorizonRing;

extern HorizonRing *g_HORIZON_RINGS[HORIZON_RING_COUNT];
extern void        *g_HORIZON_CEL_RESOURCE;
extern int32        g_HORIZON_SCREEN_POSITION;
extern int32        g_HORIZON_CONFIGURATION_VARIANT;
extern int32        g_HORIZON_PROJECTION_SCALE_FIXED;
extern int32        g_HORIZON_VERTICAL_ORIGIN_FIXED;
extern int32        g_HORIZON_BITMAP_PAGE_COUNT;
extern int32        g_HORIZON_CONFIGURATION_PRESETS[9][14];

void
free_horizon_ring_memory(void *memory_);
void
free_horizon_ring_resources(void);
