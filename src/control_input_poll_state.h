#pragma once

#include "types.h"

typedef struct ControlInputPollState
{
  uint32 status_00;
  uint32 status_04;
  uint32 status_08;
  uint32 status_0c;
  uint32 primary_status;
  int32  simulation_start_scale;
  int32  simulation_start_delay;
  uint32 simulation_control_buttons;
  int32  frontend_display_enabled;
} ControlInputPollState;

extern ControlInputPollState g_CONTROL_INPUT_POLL_STATE;

int32
reset_control_input_poll_state(void);
void
configure_frontend_display(int32 enabled_, int32 mode_);
void
adjust_current_simulation_mode_configuration(uint32 buttons_, int32 fine_adjustment_);
