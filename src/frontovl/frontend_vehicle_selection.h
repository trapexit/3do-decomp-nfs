#pragma once

#include "types.h"

extern const char *g_FRONTEND_CAR_SHAPE_NAMES[8];
extern const char *g_FRONTEND_CAR_STATS_SHAPE_PATH;
extern const char *g_FRONTEND_CAR_STATS_SHAPE_PATHS[8];
extern const char *g_FRONTEND_CAR_SPEECH_PATHS[8];
extern void       *g_FRONTEND_VEHICLE_DETAIL_SHAPES;
extern void       *g_FRONTEND_VEHICLE_STATS_SHAPES;

int32
draw_frontend_vehicle_selection(int32 vehicle_index_, int32 scroll_offset_);
uint8
select_frontend_vehicle(int32 frame_count_, uint8 vehicle_index_);
