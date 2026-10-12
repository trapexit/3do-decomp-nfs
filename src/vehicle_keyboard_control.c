#include "vehicle_keyboard_control.h"

#define VEHICLE_CONTROL_FULL_VALUE 0xFF
#define VEHICLE_KEYBOARD_BASE_KEY_CODE 0x1E
#define VEHICLE_KEYBOARD_FIRST_GEAR_KEY_CODE 0x3D
#define VEHICLE_KEYBOARD_SECOND_GEAR_KEY_CODE 0x3E
#define VEHICLE_KEYBOARD_THIRD_GEAR_KEY_CODE 0x3F
#define VEHICLE_GEAR_FIRST_ACTION 0
#define VEHICLE_GEAR_SECOND_ACTION 1
#define VEHICLE_GEAR_THIRD_ACTION 2
#define VEHICLE_KEYBOARD_PRIMARY_SCALE 2185
#define VEHICLE_KEYBOARD_PRIMARY_DIVISOR 256
#define VEHICLE_KEYBOARD_SECONDARY_SCALE 7
#define VEHICLE_KEYBOARD_PRIMARY_RISE_LIMIT 0x4C
#define VEHICLE_KEYBOARD_PRIMARY_FALL_LIMIT 0x33
#define VEHICLE_KEYBOARD_PRIMARY_READY_FALL_LIMIT 0xC

void
apply_vehicle_keyboard_control_input(VehicleParameterState *state_, int32 key_code_)
{
  int32 primary_target;
  int32 primary_fall_limit;
  int32 value_difference;

  if(key_code_ == VEHICLE_KEYBOARD_SECOND_GEAR_KEY_CODE)
    {
      key_code_ = 0;
      adjust_vehicle_gear_state(state_, VEHICLE_GEAR_SECOND_ACTION);
      state_->auxiliary_control_active = 0;
    }
  else if(key_code_ == VEHICLE_KEYBOARD_THIRD_GEAR_KEY_CODE)
    {
      key_code_ = 0;
      adjust_vehicle_gear_state(state_, VEHICLE_GEAR_THIRD_ACTION);
      state_->auxiliary_control_active = 0;
    }
  else if(key_code_ == VEHICLE_KEYBOARD_FIRST_GEAR_KEY_CODE)
    {
      key_code_                        = 0;
      state_->auxiliary_control_active = 1;
      adjust_vehicle_gear_state(state_, VEHICLE_GEAR_FIRST_ACTION);
    }
  else
    {
      state_->auxiliary_control_active = 0;
      adjust_vehicle_gear_state(state_, VEHICLE_GEAR_FIRST_ACTION);
    }

  key_code_ -= VEHICLE_KEYBOARD_BASE_KEY_CODE;
  if(key_code_ >= 1)
    key_code_++;

  if(key_code_ <= 0)
    {
      primary_target =
        -((key_code_ * VEHICLE_KEYBOARD_PRIMARY_SCALE) / VEHICLE_KEYBOARD_PRIMARY_DIVISOR);
      state_->secondary_control_value = 0;
    }
  else
    {
      primary_target                  = 0;
      state_->secondary_control_value = -(key_code_ * VEHICLE_KEYBOARD_SECONDARY_SCALE);
    }

  primary_fall_limit = VEHICLE_KEYBOARD_PRIMARY_FALL_LIMIT;
  if(state_->gear_control_ready == 0)
    primary_fall_limit = VEHICLE_KEYBOARD_PRIMARY_READY_FALL_LIMIT;

  if(state_->primary_control_value < primary_target)
    {
      value_difference = primary_target - state_->primary_control_value;
      if(value_difference <= VEHICLE_KEYBOARD_PRIMARY_RISE_LIMIT)
        {
          state_->primary_control_value = primary_target;
        }
      else
        {
          state_->primary_control_value += VEHICLE_KEYBOARD_PRIMARY_RISE_LIMIT;
        }
    }
  else if(state_->primary_control_value > primary_target)
    {
      value_difference = state_->primary_control_value - primary_target;
      if(value_difference <= primary_fall_limit)
        {
          state_->primary_control_value = primary_target;
        }
      else
        {
          state_->primary_control_value -= primary_fall_limit;
        }
    }

  if(state_->primary_control_value > VEHICLE_CONTROL_FULL_VALUE)
    state_->primary_control_value = VEHICLE_CONTROL_FULL_VALUE;

  if(state_->secondary_control_value > VEHICLE_CONTROL_FULL_VALUE)
    state_->secondary_control_value = VEHICLE_CONTROL_FULL_VALUE;
}
