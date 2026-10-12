#include "vehicle_control_event.h"

#include "control_input_event_table.h"

#define VEHICLE_CONTROL_EVENT_TABLE_CENTER_INDEX 0x20
#define VEHICLE_CONTROL_EVENT_TABLE_INDEX_INCREMENT 1
#define VEHICLE_CONTROL_EVENT_INPUT_SHIFT 2
#define VEHICLE_CONTROL_FEEDBACK_THRESHOLD 0x120000
#define VEHICLE_CONTROL_FEEDBACK_DIVISOR 128
#define VEHICLE_CONTROL_SMALL_TARGET_THRESHOLD 0xC0000
#define VEHICLE_CONTROL_SMALL_STEP_LIMIT 0x18000
#define VEHICLE_CONTROL_LARGE_STEP_LIMIT 0x40000

void
adjust_vehicle_control_event_value(VehicleParameterState *state_,
                                   int32                  event_value_,
                                   int32                  input_limit_)
{
  int32 control_delta;
  int32 event_index;
  int32 feedback_adjustment;
  int32 input_delta;
  int32 target_value;

  event_index =
    VEHICLE_CONTROL_EVENT_TABLE_CENTER_INDEX - (event_value_ >> VEHICLE_CONTROL_EVENT_INPUT_SHIFT);
  if(event_index >= 1)
    event_index += VEHICLE_CONTROL_EVENT_TABLE_INDEX_INCREMENT;

  if(event_index < 0)
    {
      target_value = -g_CONTROL_INPUT_EVENT_TABLE[-event_index];
    }
  else
    {
      target_value = g_CONTROL_INPUT_EVENT_TABLE[event_index];
    }

  feedback_adjustment = state_->vehicle_speed;
  if(feedback_adjustment < 0)
    {
      input_limit_ = 0;
    }
  else if(feedback_adjustment > VEHICLE_CONTROL_FEEDBACK_THRESHOLD)
    {
      feedback_adjustment = (feedback_adjustment - VEHICLE_CONTROL_FEEDBACK_THRESHOLD) /
                            VEHICLE_CONTROL_FEEDBACK_DIVISOR;
    }
  else
    {
      feedback_adjustment = 0;
    }

  control_delta = target_value - state_->control_event_value;
  input_delta   = input_limit_ - state_->control_event_value;
  if(control_delta > 0 && input_delta > 0)
    {
      if(control_delta < input_delta)
        input_delta = control_delta;
    }
  else if(control_delta < 0 && input_delta < 0)
    {
      if(control_delta >= input_delta)
        input_delta = control_delta;
    }
  else if(control_delta > 0)
    {
      if(control_delta <= VEHICLE_CONTROL_SMALL_TARGET_THRESHOLD)
        input_delta = VEHICLE_CONTROL_SMALL_STEP_LIMIT - feedback_adjustment;
      else
        input_delta = VEHICLE_CONTROL_LARGE_STEP_LIMIT - feedback_adjustment;

      if(input_delta > control_delta)
        input_delta = control_delta;
    }
  else
    {
      if(control_delta >= -VEHICLE_CONTROL_SMALL_TARGET_THRESHOLD)
        input_delta = VEHICLE_CONTROL_SMALL_STEP_LIMIT - feedback_adjustment;
      else
        input_delta = VEHICLE_CONTROL_LARGE_STEP_LIMIT - feedback_adjustment;

      input_delta = -input_delta;
      if(input_delta < control_delta)
        input_delta = control_delta;
    }

  state_->control_event_value += input_delta;
}
