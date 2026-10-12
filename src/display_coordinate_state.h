#pragma once

#include "types.h"

typedef struct JoystickAxisCalibration
{
  int32 minimum;
  int32 center;
  int32 maximum;
  int32 negative_scale;
  int32 positive_scale;
} JoystickAxisCalibration;

typedef struct JoystickCalibrationState
{
  JoystickAxisCalibration x;
  JoystickAxisCalibration y;
  int32                   current_x;
  int32                   current_y;
  uint32                  buttons;
  int32                   recalibration_requested;
  int32                   reserved_38[2];
} JoystickCalibrationState;

extern int32                    g_JOYSTICK_AXIS_VALUES[2];
extern JoystickCalibrationState g_JOYSTICK_CALIBRATION_STATE;

void
calculate_joystick_calibration_scales(void);
void
initialize_joystick_calibration_state(void);
