#include "track_stream_worker.h"

#include "game_session_resources.h"

#include "task.h"

void
request_track_stream_window(int32 section_)
{
  g_GAME_SESSION_STATE.stream_index = section_;
  SendSignal(g_GAME_SESSION_STATE.task_item, g_GAME_SESSION_STATE.signal_mask);
}
