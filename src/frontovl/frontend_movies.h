#pragma once

#include "types.h"

extern char g_FRONTEND_OVERLAY_TEXT_A[20];
extern char g_FRONTEND_OVERLAY_TEXT_B[20];
extern char g_FRONTEND_OVERLAY_TEXT_C[20];
extern char g_FRONTEND_OVERLAY_TEXT_D[20];

int32
enumerate_frontend_movies(void);
void
draw_frontend_movie_overlay_text(void);
void
play_frontend_movie_sequence(void);
