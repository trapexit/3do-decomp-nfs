#pragma once

#include "types.h"

extern uint32 *g_CONTROLLER_AUXILIARY_CONTROL_BITS;

uint32
read_third_controller_state(void);
int32
read_controller_state(int32 controller_number_, int32 wait_for_input_, uint32 *control_bits_);
void
wait_for_controller_input(uint32 requested_control_mask_);
uint32
poll_controller_input(int32 frame_limit_, int32 play_feedback_, uint32 requested_control_mask_);
uint32
read_stable_controller_input(void);
int32
initialize_controller_input(void);
int32
shutdown_controller_input(void);
