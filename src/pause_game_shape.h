#pragma once

#include "frontend_control_input.h"

#define G_PAUSE_GAME_SHAPE (g_FRONTEND_INPUT_STATE.pause_game_shape)

void *
load_pause_game_shape(void);
void
unload_pause_game_shape(void);
