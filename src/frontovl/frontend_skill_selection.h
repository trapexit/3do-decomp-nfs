#pragma once

#include "types.h"

extern const char *g_FRONTEND_TRACK_SPEECH_PATHS[4];
extern void       *g_FRONTEND_TRACK_ROUTE_SHAPES;
extern const char *g_FRONTEND_TRACK_ROUTE_TITLE_SHAPE_NAMES[4];
extern const char *g_FRONTEND_TRACK_ROUTE_MARKER_SHAPE_NAMES[12];

int32
draw_frontend_track_selection(int32 track_index_);
uint8
select_frontend_track(int32 frame_count_, uint8 track_index_);
