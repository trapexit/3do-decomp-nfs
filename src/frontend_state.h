#pragma once

#include "game_bootstrap.h"

#include "types.h"

#define FRONTEND_OPTIONS_POSITION_COUNT 3

extern void                *g_FRONTEND_OPTIONS_SHAPE_RESOURCE;
extern StartupConfiguration g_FRONTEND_OPTIONS_CONFIGURATION;
extern uint8                g_FRONTEND_SAVED_SOUND_EFFECTS;
extern int32                g_FRONTEND_OPTIONS_POSITION_Y[FRONTEND_OPTIONS_POSITION_COUNT];
extern int32                g_FRONTEND_OPTIONS_POSITION_X[FRONTEND_OPTIONS_POSITION_COUNT];

void
initialize_frontend_state(void);
int32
initialize_frontend_state_internal(void);
int32
render_frontend_display_options(int32 selected_option_);
void
set_frontend_state(int32 active_, int32 mode_, int32 reset_, int32 transition_active_);
