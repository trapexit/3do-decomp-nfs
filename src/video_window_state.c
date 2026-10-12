#include "video_window_state.h"

#include "display_buffer_state.h"
#include "display_graphics.h"

#define VIDEO_WINDOW_FOLLOWING_BUFFER_OFFSET 2

int32 g_PENDING_VIDEO_WINDOW_INDEX = 0;

void
prepare_pending_video_window(void)
{
  g_PENDING_VIDEO_WINDOW_INDEX = (g_CURRENT_DISPLAY_BUFFER_INDEX + 1);
  if(g_PENDING_VIDEO_WINDOW_INDEX >= g_DISPLAY_BUFFER_COUNT)
    g_PENDING_VIDEO_WINDOW_INDEX = 0;

  store_display_buffer_descriptor_snapshot_by_index(g_PENDING_VIDEO_WINDOW_INDEX);
}

void
prepare_following_video_window(void)
{
  g_PENDING_VIDEO_WINDOW_INDEX =
    (g_CURRENT_DISPLAY_BUFFER_INDEX + VIDEO_WINDOW_FOLLOWING_BUFFER_OFFSET);
  if(g_PENDING_VIDEO_WINDOW_INDEX >= g_DISPLAY_BUFFER_COUNT)
    g_PENDING_VIDEO_WINDOW_INDEX = (g_PENDING_VIDEO_WINDOW_INDEX - g_DISPLAY_BUFFER_COUNT);

  store_display_buffer_descriptor_snapshot_by_index(g_PENDING_VIDEO_WINDOW_INDEX);
}

int32
get_pending_video_window_index(void)
{
  return g_PENDING_VIDEO_WINDOW_INDEX;
}
