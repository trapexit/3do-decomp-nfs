#pragma once

#include "frontend_render_state.h"

#include "types.h"

typedef struct FrontendMenuItem
{
  uint8 reserved_00[0x10];
  int32 coordinate_x;
  int32 coordinate_y;
} FrontendMenuItem;

typedef struct FrontendMenuCoordinates
{
  int32 x;
  int32 y;
} FrontendMenuCoordinates;

typedef struct GameRuntimeState
{
  int32  track_segment_count;
  int32  result;
  int32  race_finish_elapsed_frames;
  int32  race_start_frame;
  uint8  reserved_10[4];
  uint32 flags_14;
  uint32 flags_18;
  int32  statistics_track_segment_threshold;
  int32  statistics_frame_window;
  int32  statistics_capture_complete;
} GameRuntimeState;

extern GameRuntimeState        g_GAME_RUNTIME_STATE;
extern FrontendMenuCoordinates g_FRONTEND_MENU_COORDINATES;

int32
run_frontend_transition(void);
void
update_frontend_menu_coordinates(void);
