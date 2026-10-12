#pragma once

#include "memory_display_input.h"

#include "types.h"

extern char                       *g_FRONTEND_PLAYER_NAME;
extern char                       *g_FRONTEND_SKILL_HEADING;
extern void                       *g_FRONTEND_DISPLAY_CONTEXT;
extern void                       *g_POST_RACE_CURRENT_RESULT;
extern MemoryDisplayInputCallback *g_MEMORY_DISPLAY_INPUT_CALLBACK_SLOT;
extern int32                      *g_POST_RACE_RESULT_SLOT;
extern void                       *g_FRONTEND_JOYSTICK_CALIBRATION_STATE;
extern int32                      *g_FRONTEND_ACTIVE_PLAYER_INDEX;
extern int32                      *g_FRONTEND_SHARED_CONTROL_VALUE_A;
extern int32                      *g_FRONTEND_SHARED_CONTROL_VALUE_B;
extern int32                       g_POST_RACE_SAVE_ENABLED;
