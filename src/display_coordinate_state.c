#include "display_coordinate_state.h"

#include "game_bootstrap.h"

#include "operamath.h"

#define JOYSTICK_CALIBRATION_FIXED16_ONE 0x00010000
#define JOYSTICK_CALIBRATION_INTEGER_SHIFT 16
#define JOYSTICK_CALIBRATION_DEFAULT_CENTER 500
#define JOYSTICK_CALIBRATION_DEFAULT_MAXIMUM 0x00000400

int32                    g_JOYSTICK_AXIS_VALUES[2];
JoystickCalibrationState g_JOYSTICK_CALIBRATION_STATE;

void
calculate_joystick_calibration_scales(void)
{
  g_JOYSTICK_CALIBRATION_STATE.x.negative_scale =
    g_JOYSTICK_CALIBRATION_STATE.x.center - g_JOYSTICK_CALIBRATION_STATE.x.minimum;
  g_JOYSTICK_CALIBRATION_STATE.y.negative_scale =
    g_JOYSTICK_CALIBRATION_STATE.y.center - g_JOYSTICK_CALIBRATION_STATE.y.minimum;
  g_JOYSTICK_CALIBRATION_STATE.x.positive_scale =
    g_JOYSTICK_CALIBRATION_STATE.x.maximum - g_JOYSTICK_CALIBRATION_STATE.x.center;
  g_JOYSTICK_CALIBRATION_STATE.y.positive_scale =
    g_JOYSTICK_CALIBRATION_STATE.y.maximum - g_JOYSTICK_CALIBRATION_STATE.y.center;

  g_JOYSTICK_CALIBRATION_STATE.x.negative_scale =
    DivSF16(JOYSTICK_CALIBRATION_FIXED16_ONE,
            g_JOYSTICK_CALIBRATION_STATE.x.negative_scale << JOYSTICK_CALIBRATION_INTEGER_SHIFT);
  g_JOYSTICK_CALIBRATION_STATE.x.positive_scale =
    DivSF16(JOYSTICK_CALIBRATION_FIXED16_ONE,
            g_JOYSTICK_CALIBRATION_STATE.x.positive_scale << JOYSTICK_CALIBRATION_INTEGER_SHIFT);
  g_JOYSTICK_CALIBRATION_STATE.y.negative_scale =
    DivSF16(JOYSTICK_CALIBRATION_FIXED16_ONE,
            g_JOYSTICK_CALIBRATION_STATE.y.negative_scale << JOYSTICK_CALIBRATION_INTEGER_SHIFT);
  g_JOYSTICK_CALIBRATION_STATE.y.positive_scale =
    DivSF16(JOYSTICK_CALIBRATION_FIXED16_ONE,
            g_JOYSTICK_CALIBRATION_STATE.y.positive_scale << JOYSTICK_CALIBRATION_INTEGER_SHIFT);
  g_JOYSTICK_AXIS_VALUES[0] = 0;
  g_JOYSTICK_AXIS_VALUES[1] = 0;
}

void
initialize_joystick_calibration_state(void)
{
  if(g_STARTUP_CONFIGURATION.joystick_calibration_configured != 0)
    {
      g_JOYSTICK_CALIBRATION_STATE.x.minimum = g_STARTUP_CONFIGURATION.joystick_x_minimum;
      g_JOYSTICK_CALIBRATION_STATE.x.center  = g_STARTUP_CONFIGURATION.joystick_x_center;
      g_JOYSTICK_CALIBRATION_STATE.x.maximum = g_STARTUP_CONFIGURATION.joystick_x_maximum;
      g_JOYSTICK_CALIBRATION_STATE.y.minimum = g_STARTUP_CONFIGURATION.joystick_y_minimum;
      g_JOYSTICK_CALIBRATION_STATE.y.center  = g_STARTUP_CONFIGURATION.joystick_y_center;
      g_JOYSTICK_CALIBRATION_STATE.y.maximum = g_STARTUP_CONFIGURATION.joystick_y_maximum;
    }
  else
    {
      g_JOYSTICK_CALIBRATION_STATE.x.minimum = 0;
      g_JOYSTICK_CALIBRATION_STATE.x.center  = JOYSTICK_CALIBRATION_DEFAULT_CENTER;
      g_JOYSTICK_CALIBRATION_STATE.x.maximum = JOYSTICK_CALIBRATION_DEFAULT_MAXIMUM;
      g_JOYSTICK_CALIBRATION_STATE.y.minimum = 0;
      g_JOYSTICK_CALIBRATION_STATE.y.center  = JOYSTICK_CALIBRATION_DEFAULT_CENTER;
      g_JOYSTICK_CALIBRATION_STATE.y.maximum = JOYSTICK_CALIBRATION_DEFAULT_MAXIMUM;
    }
  calculate_joystick_calibration_scales();

  g_JOYSTICK_CALIBRATION_STATE.current_x               = 0;
  g_JOYSTICK_CALIBRATION_STATE.current_y               = 0;
  g_JOYSTICK_CALIBRATION_STATE.buttons                 = 0;
  g_JOYSTICK_CALIBRATION_STATE.recalibration_requested = 0;
  g_JOYSTICK_CALIBRATION_STATE.reserved_38[0]          = 0;
  g_JOYSTICK_CALIBRATION_STATE.reserved_38[1]          = 0;
  g_JOYSTICK_AXIS_VALUES[0]                            = 0;
  g_JOYSTICK_AXIS_VALUES[1]                            = 0;
}
