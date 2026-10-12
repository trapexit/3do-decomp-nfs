#include "control_input_poll_state.h"

#include "simulation_snapshot.h"

#define CONTROL_CONFIGURATION_DEFAULT_STEP 8
#define CONTROL_CONFIGURATION_COARSE_STEP 64
#define CONTROL_CONFIGURATION_FINE_SHIFT 3
#define CONTROL_CONFIGURATION_COARSE_MODIFIER 0x00400000
#define CONTROL_CONFIGURATION_FINE_MODIFIER 0x00200000
#define CONTROL_CONFIGURATION_MODIFIER_MASK 0x00600000
#define CONTROL_CONFIGURATION_Z_DECREASE 0x04000000
#define CONTROL_CONFIGURATION_Z_INCREASE 0x08000000
#define CONTROL_CONFIGURATION_X_DECREASE 0x10000000
#define CONTROL_CONFIGURATION_X_INCREASE 0x20000000
#define CONTROL_CONFIGURATION_Y_DECREASE 0x40000000
#define CONTROL_CONFIGURATION_Y_INCREASE 0x80000000UL
#define CONTROL_CONFIGURATION_SCALE_DECREASE 0x00800000
#define CONTROL_CONFIGURATION_SCALE_INCREASE 0x01000000

ControlInputPollState g_CONTROL_INPUT_POLL_STATE;
int32                 g_FRONTEND_DISPLAY_MODE_ZERO_REQUESTED;
int32                 g_FRONTEND_DISPLAY_MODE_ONE_REQUESTED;
int32                 g_FRONTEND_DISPLAY_MODE_OTHER_REQUESTED;
int32                 g_CONTROL_CONFIGURATION_ADJUSTMENT_STEP;

void
configure_frontend_display(int32 enabled_, int32 mode_)
{
  g_CONTROL_INPUT_POLL_STATE.frontend_display_enabled = enabled_;
  if(enabled_ == 0)
    return;

  if(mode_ == 0)
    g_FRONTEND_DISPLAY_MODE_ZERO_REQUESTED = 1;
  else if(mode_ == 1)
    g_FRONTEND_DISPLAY_MODE_ONE_REQUESTED = 1;
  else
    g_FRONTEND_DISPLAY_MODE_OTHER_REQUESTED = 1;
}

void
adjust_current_simulation_mode_configuration(uint32 buttons_, int32 fine_adjustment_)
{
  SimulationModeConfiguration *configuration;
  uint32                       action;

  if(buttons_ == 0)
    return;

  g_SIMULATION_RENDER_STATE.interpolation_frames = CONTROL_CONFIGURATION_DEFAULT_STEP;
  g_CONTROL_CONFIGURATION_ADJUSTMENT_STEP         = CONTROL_CONFIGURATION_DEFAULT_STEP;
  if((buttons_ & CONTROL_CONFIGURATION_COARSE_MODIFIER) != 0)
    g_CONTROL_CONFIGURATION_ADJUSTMENT_STEP = CONTROL_CONFIGURATION_COARSE_STEP;
  else if((buttons_ & CONTROL_CONFIGURATION_FINE_MODIFIER) != 0)
    g_CONTROL_CONFIGURATION_ADJUSTMENT_STEP =
      fine_adjustment_ >> CONTROL_CONFIGURATION_FINE_SHIFT;

  configuration = &g_SIMULATION_MODE_CONFIGURATIONS[g_SIMULATION_RENDER_STATE.mode];
  action        = buttons_ & ~CONTROL_CONFIGURATION_MODIFIER_MASK;
  switch(action)
    {
      case CONTROL_CONFIGURATION_Z_DECREASE:
        configuration->transition_vector.z -= g_CONTROL_CONFIGURATION_ADJUSTMENT_STEP;
        break;
      case CONTROL_CONFIGURATION_Z_INCREASE:
        configuration->transition_vector.z += g_CONTROL_CONFIGURATION_ADJUSTMENT_STEP;
        break;
      case CONTROL_CONFIGURATION_X_DECREASE:
        configuration->transition_vector.x -= g_CONTROL_CONFIGURATION_ADJUSTMENT_STEP;
        break;
      case CONTROL_CONFIGURATION_X_INCREASE:
        configuration->transition_vector.x += g_CONTROL_CONFIGURATION_ADJUSTMENT_STEP;
        break;
      case CONTROL_CONFIGURATION_Y_DECREASE:
        configuration->transition_vector.y -= g_CONTROL_CONFIGURATION_ADJUSTMENT_STEP;
        break;
      case CONTROL_CONFIGURATION_Y_INCREASE:
        configuration->transition_vector.y += g_CONTROL_CONFIGURATION_ADJUSTMENT_STEP;
        break;
      case CONTROL_CONFIGURATION_SCALE_DECREASE:
        configuration->movement_scale -= g_CONTROL_CONFIGURATION_ADJUSTMENT_STEP;
        break;
      case CONTROL_CONFIGURATION_SCALE_INCREASE:
        configuration->movement_scale += g_CONTROL_CONFIGURATION_ADJUSTMENT_STEP;
        break;
    }
}

int32
reset_control_input_poll_state(void)
{
  g_CONTROL_INPUT_POLL_STATE.status_00                  = 0;
  g_CONTROL_INPUT_POLL_STATE.status_04                  = 0;
  g_CONTROL_INPUT_POLL_STATE.status_08                  = 0;
  g_CONTROL_INPUT_POLL_STATE.status_0c                  = 0;
  g_CONTROL_INPUT_POLL_STATE.simulation_start_scale     = 0;
  g_CONTROL_INPUT_POLL_STATE.simulation_start_delay     = 0;
  g_CONTROL_INPUT_POLL_STATE.simulation_control_buttons = 0;
  return 1;
}
