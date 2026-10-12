#pragma once

#include "road_data.h"

#define TRACK_DATA_PATH_SIZE 64

#define G_TRACK_DATA_PATH (g_ROAD_RUNTIME_STATE.track_name)

void
set_track_data_path(const char *path_);
