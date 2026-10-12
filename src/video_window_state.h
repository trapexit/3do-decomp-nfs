#pragma once

#include "types.h"

extern int32 g_PENDING_VIDEO_WINDOW_INDEX;

void
prepare_pending_video_window(void);
void
prepare_following_video_window(void);
int32
get_pending_video_window_index(void);
