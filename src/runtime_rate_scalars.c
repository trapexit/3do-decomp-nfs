#include "runtime_rate_scalars.h"

#include "driving_warning_display.h"
#include "frontend_transition.h"

#define RUNTIME_RATE_MINUTES_INDEX 3
#define RUNTIME_RATE_SECONDS_INDEX 2
#define RUNTIME_RATE_TENTHS_INDEX 1
#define RUNTIME_RATE_FRAMES_PER_MINUTE 3600
#define RUNTIME_RATE_FRAMES_PER_SECOND 60
#define RUNTIME_RATE_FRAMES_PER_TENTH 6

void
configure_runtime_rate_scalars(int32 interval_)
{
  int32 elapsed_frames;

  elapsed_frames = 0;
  if(g_GAME_RUNTIME_STATE.race_start_frame != 0)
    {
      elapsed_frames = interval_ - g_GAME_RUNTIME_STATE.race_start_frame;
      if(elapsed_frames < 0)
        elapsed_frames = 0;
      else if(g_GAME_RUNTIME_STATE.race_finish_elapsed_frames != 0)
        {
          if(elapsed_frames > g_GAME_RUNTIME_STATE.race_finish_elapsed_frames)
            elapsed_frames = g_GAME_RUNTIME_STATE.race_finish_elapsed_frames;
        }
    }

  g_DRIVING_WARNING_RATE_VALUES[RUNTIME_RATE_MINUTES_INDEX] =
    elapsed_frames / RUNTIME_RATE_FRAMES_PER_MINUTE;
  elapsed_frames -=
    g_DRIVING_WARNING_RATE_VALUES[RUNTIME_RATE_MINUTES_INDEX] * RUNTIME_RATE_FRAMES_PER_MINUTE;
  g_DRIVING_WARNING_RATE_VALUES[RUNTIME_RATE_SECONDS_INDEX] =
    elapsed_frames / RUNTIME_RATE_FRAMES_PER_SECOND;
  elapsed_frames -=
    g_DRIVING_WARNING_RATE_VALUES[RUNTIME_RATE_SECONDS_INDEX] * RUNTIME_RATE_FRAMES_PER_SECOND;
  g_DRIVING_WARNING_RATE_VALUES[RUNTIME_RATE_TENTHS_INDEX] =
    elapsed_frames / RUNTIME_RATE_FRAMES_PER_TENTH;
}
