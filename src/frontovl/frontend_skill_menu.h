#pragma once

#include "race_state.h"

#include "types.h"

extern const char       *g_FRONTEND_SKILL_TRACK_SHAPE_NAMES[4];
extern const char       *g_FRONTEND_SKILL_VEHICLE_TITLE_SHAPE_NAMES[8];
extern const char       *g_FRONTEND_SKILL_VEHICLE_SHAPE_NAMES[8];
extern const char       *g_FRONTEND_SKILL_OPPONENT_SHAPE_NAMES[9];
extern RaceConfiguration g_FRONTEND_SKILL_CONFIGURATION;
extern void             *g_FRONTEND_SKILL_CONTROL_SHAPES;
extern void             *g_FRONTEND_SKILL_CAR_SHAPES;
extern int32             g_FRONTEND_SKILL_DOWN_SELECTIONS[5];
extern int32             g_FRONTEND_SKILL_UP_SELECTIONS[5];
extern int32             g_FRONTEND_SKILL_LEFT_SELECTIONS[5];
extern int32             g_FRONTEND_SKILL_RIGHT_SELECTIONS[5];
extern int32             g_FRONTEND_VEHICLE_ABS_AVAILABILITY[8];

int32
draw_frontend_skill_menu(int32 selection_index_);
int32
run_frontend_skill_menu(int32 frame_count_);
