#pragma once

#include "types.h"

typedef struct FrontendInputState
{
  uint32 reserved_00;
  uint32 reserved_04;
  void  *buffer;
  void  *pause_game_shape;
  uint32 previous_buttons;
} FrontendInputState;

extern FrontendInputState g_FRONTEND_INPUT_STATE;

#define G_FRONTEND_INPUT_BUFFER (g_FRONTEND_INPUT_STATE.buffer)

void
release_frontend_input_buffer(void);
int32
poll_frontend_control_input(int32 control_pad_count_, int32 wait_, uint32 *control_input_);
uint32
read_frontend_control_input(void);
uint32
wait_for_frontend_control_press(uint32 control_mask_);
void
wait_for_frontend_control_release(void);
int32
clear_frontend_control_values(uint32 *first_value_, uint32 *second_value_);
