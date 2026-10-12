#pragma once

#include "types.h"

extern const char *g_FRONTEND_MUSIC_PATHS[3];
extern const char *g_FRONTEND_CAR_DISPLAY_SHAPE_PATHS[8];
extern const char *g_FRONTEND_CAR_TITLE_SHAPE_NAMES[8];
extern const char *g_FRONTEND_ATTRACT_SHAPE_NAMES[3];
extern int32       g_FRONTEND_SCENE_AUDIO_ACTIVE;
extern int32       g_FRONTEND_ATTRACT_INDEX;

int32
resume_frontend_audio_stream(void);
int32
pause_frontend_audio_stream(void);
int32
start_frontend_scene_audio(void);
void
stop_frontend_scene_audio(void);
int32
run_frontend_skill_selection(void);
void
run_frontend_event_presentation(uint32 flags_);
void
suspend_frontend_music_for_scene(void);
void
resume_frontend_music_after_scene(void);
