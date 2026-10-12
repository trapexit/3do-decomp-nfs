#pragma once

#include "types.h"

typedef struct GameFrameState
{
  uint32 reserved_00;
  uint8  running;
  uint8  reserved_05[3];
  uint32 frame_number;
  Item   display_audio_item;
  Item   cue;
  uint32 cue_signal;
  uint32 frame_limit;
  uint32 frame_progress;
  uint32 initial_audio_time;
  uint32 scheduled_audio_time;
  uint32 previous_game_frame_counter;
} GameFrameState;

extern GameFrameState g_GAME_FRAME_STATE;

uint32
run_game_frame(void);
