#pragma once

#include "types.h"

typedef struct RenderFrameTimingState
{
  uint32 frame_count;
  uint32 interval_frame_count;
  uint32 total_vbl_delta;
  uint32 accumulated_vbl_delta;
  uint32 latest_vbl_delta;
  int32  transition_delay;
  uint32 average_rate;
} RenderFrameTimingState;

extern RenderFrameTimingState g_RENDER_FRAME_TIMING_STATE;

void
render_next_frame(void);
void
render_next_frame_callback(void);
