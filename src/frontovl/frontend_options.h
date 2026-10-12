#pragma once

#include "race_state.h"

#include "types.h"

extern const char       *g_FRONTEND_AUDIO_MODE_SHAPE_NAMES[4];
extern void             *g_FRONTEND_OPTIONS_SHAPES;
extern RaceConfiguration g_FRONTEND_OPTIONS_CONFIGURATION;
extern int32             g_FRONTEND_OPTIONS_SELECTION_Y[3];
extern int32             g_FRONTEND_OPTIONS_SELECTION_X[3];

int32
draw_frontend_options(int32 selection_index_);
int32
run_frontend_options(void);
