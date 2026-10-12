#include "vehicle_control_input.h"

#include "control_input_poll_state.h"
#include "game_bootstrap.h"
#include "geometry.h"
#include "vehicle_control_event.h"
#include "vehicle_keyboard_control.h"

#include "operamath.h"

#define VEHICLE_CONTROL_FULL_VALUE 0xFF
#define VEHICLE_CONTROL_SECONDARY_MAX_VALUE 100
#define VEHICLE_GEAR_UP_ACTION 2
#define VEHICLE_GEAR_TOGGLE_ACTIVE_ACTION 1
#define VEHICLE_GEAR_TOGGLE_INACTIVE_ACTION 0
#define VEHICLE_CONTROL_MASK_ACCELERATE_INDEX 0
#define VEHICLE_CONTROL_MASK_BRAKE_INDEX 1
#define VEHICLE_CONTROL_MASK_GEAR_UP_INDEX 3
#define VEHICLE_CONTROL_MASK_GEAR_TOGGLE_INDEX 4
#define VEHICLE_CONTROL_MASK_UP_LEFT_INDEX 5
#define VEHICLE_CONTROL_MASK_LEFT_INDEX 6
#define VEHICLE_CONTROL_MASK_DOWN_LEFT_INDEX 7
#define VEHICLE_CONTROL_MASK_UP_RIGHT_INDEX 8
#define VEHICLE_CONTROL_MASK_RIGHT_INDEX 9
#define VEHICLE_CONTROL_MASK_DOWN_RIGHT_INDEX 10
#define VEHICLE_CONTROL_MASK_AUXILIARY_INDEX 12
#define VEHICLE_STEERING_SHALLOW_ANGLE 0x180000
#define VEHICLE_STEERING_STANDARD_ANGLE 0x1B0000
#define VEHICLE_STEERING_SHARP_ANGLE 0x1E0000
#define VEHICLE_STEERING_ANALOG_LIMIT 0x400000
#define VEHICLE_STEERING_ATTENUATION_LIMIT 0x18000
#define VEHICLE_STEERING_RAMP_LIMIT 0x19999
#define VEHICLE_STEERING_FIXED16_SHIFT 16
#define VEHICLE_STEERING_SHALLOW_SPEED_INDEX 0
#define VEHICLE_STEERING_STANDARD_SPEED_INDEX 1
#define VEHICLE_STEERING_SHARP_SPEED_INDEX 2
#define VEHICLE_CONTROL_ANALOG_TARGET_LIMIT 0x40
#define VEHICLE_CONTROL_JOYSTICK_PRESET_INDEX 5
#define VEHICLE_CONTROL_REFERENCE_LOOKAHEAD 6
#define VEHICLE_CONTROL_PACKED_HEADING_HIGH_INDEX 4
#define VEHICLE_CONTROL_PACKED_HEADING_LOW_INDEX 5
#define VEHICLE_CONTROL_PACKED_HEADING_BYTE_SHIFT 8
#define VEHICLE_CONTROL_PACKED_HEADING_SHIFT 10
#define VEHICLE_CONTROL_LATERAL_THRESHOLD 0xB333
#define VEHICLE_CONTROL_LATERAL_OFFSET_FIXED 0x20000
#define VEHICLE_CONTROL_ANGLE_HALF_REVOLUTION 0x800000
#define VEHICLE_CONTROL_ANGLE_REVOLUTION 0x1000000

typedef struct VehicleControlGeometryState
{
  Vector3 position;
  uint8   reserved_0_c[4];
  int32   heading_angle;
  uint8   reserved_14[0x30];
  int32   track_segment_index;
} VehicleControlGeometryState;

extern void
adjust_vehicle_gear_state(VehicleParameterState *state_, int32 action_);

int32 g_VEHICLE_ACCELERATE_BUTTON_MASK;
int32 g_VEHICLE_BRAKE_BUTTON_MASK;
int32 g_VEHICLE_CONTROL_RESERVED_BUTTON_MASK;
int32 g_VEHICLE_GEAR_UP_BUTTON_MASK;
int32 g_VEHICLE_GEAR_TOGGLE_BUTTON_MASK;
int32 g_VEHICLE_AUXILIARY_BUTTON_MASK;

int32
calculate_vehicle_control_heading(VehicleParameterState *state_)
{
  VehicleControlGeometryState *geometry_state;
  ReferenceDistanceRecord     *current_record;
  ReferenceDistanceRecord     *target_record;
  int32                        current_heading;
  int32                        target_heading;
  int32                        lateral_distance;
  int32                        lateral_offset;
  int32                        target_x;
  int32                        target_z;
  int32                        heading_difference;

  geometry_state = (VehicleControlGeometryState *) state_;
  current_record = &g_REFERENCE_DISTANCE_RECORDS[geometry_state->track_segment_index];
  target_record  = &g_REFERENCE_DISTANCE_RECORDS[geometry_state->track_segment_index +
                                                 VEHICLE_CONTROL_REFERENCE_LOOKAHEAD];

  current_heading =
    (int32) (int16) (((uint16) current_record
                        ->packed_track_direction[VEHICLE_CONTROL_PACKED_HEADING_HIGH_INDEX]
                      << VEHICLE_CONTROL_PACKED_HEADING_BYTE_SHIFT) |
                     current_record
                       ->packed_track_direction[VEHICLE_CONTROL_PACKED_HEADING_LOW_INDEX]);
  current_heading <<= VEHICLE_CONTROL_PACKED_HEADING_SHIFT;

  lateral_distance =
    MulSF16(SinF16(current_heading), current_record->position.z - geometry_state->position.z);
  lateral_distance -=
    MulSF16(CosF16(current_heading), current_record->position.x - geometry_state->position.x);

  if(lateral_distance < -VEHICLE_CONTROL_LATERAL_THRESHOLD)
    lateral_offset = -VEHICLE_CONTROL_LATERAL_OFFSET_FIXED;
  else if(lateral_distance > VEHICLE_CONTROL_LATERAL_THRESHOLD)
    lateral_offset = VEHICLE_CONTROL_LATERAL_OFFSET_FIXED;
  else
    lateral_offset = 0;

  target_heading =
    (int32) (int16) (((uint16) target_record
                        ->packed_track_direction[VEHICLE_CONTROL_PACKED_HEADING_HIGH_INDEX]
                      << VEHICLE_CONTROL_PACKED_HEADING_BYTE_SHIFT) |
                     target_record
                       ->packed_track_direction[VEHICLE_CONTROL_PACKED_HEADING_LOW_INDEX]);
  target_heading <<= VEHICLE_CONTROL_PACKED_HEADING_SHIFT;

  target_x = target_record->position.x + MulSF16(CosF16(target_heading), lateral_offset);
  target_z = target_record->position.z - MulSF16(SinF16(target_heading), lateral_offset);
  heading_difference =
    Atan2F16(target_z - geometry_state->position.z, target_x - geometry_state->position.x) -
    geometry_state->heading_angle;

  if(heading_difference > VEHICLE_CONTROL_ANGLE_HALF_REVOLUTION)
    heading_difference -= VEHICLE_CONTROL_ANGLE_REVOLUTION;
  else if(heading_difference < -VEHICLE_CONTROL_ANGLE_HALF_REVOLUTION)
    heading_difference += VEHICLE_CONTROL_ANGLE_REVOLUTION;
  return heading_difference;
}

void
update_player_vehicle_control_mode(VehicleParameterState *state_)
{
  int32 analog_target;
  int32 absolute_target;

  analog_target   = calculate_vehicle_control_heading(state_);
  absolute_target = analog_target;
  if(absolute_target < 0)
    absolute_target = -absolute_target;
  if(absolute_target > VEHICLE_CONTROL_ANALOG_TARGET_LIMIT)
    analog_target = 0;

  if(g_STARTUP_CONFIGURATION.control_preset_index == VEHICLE_CONTROL_JOYSTICK_PRESET_INDEX)
    {
      adjust_vehicle_control_event_value(state_,
                                         g_CONTROL_INPUT_POLL_STATE.simulation_start_scale,
                                         analog_target);
      apply_vehicle_keyboard_control_input(state_,
                                           g_CONTROL_INPUT_POLL_STATE.simulation_start_delay);
    }
  else
    {
      adjust_vehicle_steering_control(state_,
                                      g_CONTROL_INPUT_POLL_STATE.simulation_control_buttons,
                                      analog_target);
      apply_vehicle_digital_control_inputs(state_,
                                           g_CONTROL_INPUT_POLL_STATE.simulation_control_buttons);
    }
}

void
adjust_vehicle_steering_control(VehicleParameterState *state_,
                                uint32                 button_states_,
                                int32                  analog_target_)
{
  const VehicleParameters *parameters;
  int32                    angle_difference;
  int32                    analog_difference;
  int32                    attenuation;
  int32                    steering_step;
  int32                    steering_target;
  int32                    speed;
  int32                    speed_threshold;

  parameters = state_->parameters;
  if((button_states_ &
      g_STARTUP_CONFIGURATION.control_button_masks[VEHICLE_CONTROL_MASK_RIGHT_INDEX]) == 0)
    {
      if((button_states_ &
          g_STARTUP_CONFIGURATION.control_button_masks[VEHICLE_CONTROL_MASK_LEFT_INDEX]) == 0)
        {
          steering_target = 0;
          speed_threshold =
            parameters->automatic_steering_speed_thresholds[VEHICLE_STEERING_STANDARD_SPEED_INDEX];
        }
      else if((button_states_ & g_STARTUP_CONFIGURATION
                                  .control_button_masks[VEHICLE_CONTROL_MASK_DOWN_LEFT_INDEX]) ==
              g_STARTUP_CONFIGURATION.control_button_masks[VEHICLE_CONTROL_MASK_DOWN_LEFT_INDEX])
        {
          steering_target = -VEHICLE_STEERING_SHARP_ANGLE;
          speed_threshold =
            parameters->automatic_steering_speed_thresholds[VEHICLE_STEERING_SHARP_SPEED_INDEX];
        }
      else if((button_states_ &
               g_STARTUP_CONFIGURATION.control_button_masks[VEHICLE_CONTROL_MASK_UP_LEFT_INDEX]) ==
              g_STARTUP_CONFIGURATION.control_button_masks[VEHICLE_CONTROL_MASK_UP_LEFT_INDEX])
        {
          steering_target = -VEHICLE_STEERING_SHALLOW_ANGLE;
          speed_threshold =
            parameters->automatic_steering_speed_thresholds[VEHICLE_STEERING_SHALLOW_SPEED_INDEX];
        }
      else
        {
          steering_target = -VEHICLE_STEERING_STANDARD_ANGLE;
          speed_threshold =
            parameters->automatic_steering_speed_thresholds[VEHICLE_STEERING_STANDARD_SPEED_INDEX];
        }
    }
  else if((button_states_ &
           g_STARTUP_CONFIGURATION.control_button_masks[VEHICLE_CONTROL_MASK_DOWN_RIGHT_INDEX]) ==
          g_STARTUP_CONFIGURATION.control_button_masks[VEHICLE_CONTROL_MASK_DOWN_RIGHT_INDEX])
    {
      steering_target = VEHICLE_STEERING_SHARP_ANGLE;
      speed_threshold =
        parameters->automatic_steering_speed_thresholds[VEHICLE_STEERING_SHARP_SPEED_INDEX];
    }
  else if((button_states_ &
           g_STARTUP_CONFIGURATION.control_button_masks[VEHICLE_CONTROL_MASK_UP_RIGHT_INDEX]) ==
          g_STARTUP_CONFIGURATION.control_button_masks[VEHICLE_CONTROL_MASK_UP_RIGHT_INDEX])
    {
      steering_target = VEHICLE_STEERING_SHALLOW_ANGLE;
      speed_threshold =
        parameters->automatic_steering_speed_thresholds[VEHICLE_STEERING_SHALLOW_SPEED_INDEX];
    }
  else
    {
      steering_target = VEHICLE_STEERING_STANDARD_ANGLE;
      speed_threshold =
        parameters->automatic_steering_speed_thresholds[VEHICLE_STEERING_STANDARD_SPEED_INDEX];
    }

  speed = state_->vehicle_speed;
  if((speed >= 0) && (steering_target == 0))
    {
      angle_difference = analog_target_;
      if(angle_difference < 0)
        angle_difference = -angle_difference;
      if(angle_difference <
         (parameters->maximum_automatic_steering_angle * (speed >> VEHICLE_STEERING_FIXED16_SHIFT)))
        {
          steering_target = analog_target_;
        }
    }

  if(speed < 0)
    speed = -speed;
  attenuation =
    parameters->automatic_steering_attenuation * (speed >> VEHICLE_STEERING_FIXED16_SHIFT);
  if(attenuation > VEHICLE_STEERING_ATTENUATION_LIMIT)
    attenuation = VEHICLE_STEERING_ATTENUATION_LIMIT;
  attenuation = parameters->automatic_steering_ramp_scale - attenuation;
  if(attenuation > VEHICLE_STEERING_RAMP_LIMIT)
    attenuation = VEHICLE_STEERING_RAMP_LIMIT;
  steering_step = speed_threshold * attenuation;

  if(state_->vehicle_speed > 0)
    {
      angle_difference = analog_target_;
      if(angle_difference < 0)
        angle_difference = -angle_difference;
      if(angle_difference < VEHICLE_STEERING_ANALOG_LIMIT)
        {
          analog_difference = analog_target_ - state_->control_event_value;
          angle_difference  = steering_target - state_->control_event_value;
          if((analog_difference < 0) && (angle_difference < 0))
            {
              steering_step <<= (uint32) parameters->alternate_steering_ramp_multiply_shift;
            }
        }
    }

  angle_difference = state_->control_event_value - steering_target;
  if(angle_difference < 0)
    angle_difference = -angle_difference;
  if(steering_step > angle_difference)
    steering_step = angle_difference;

  if(state_->control_event_value < steering_target)
    {
      state_->control_event_value += steering_step;
    }
  else if(state_->control_event_value > steering_target)
    {
      state_->control_event_value -= steering_step;
    }
}

void
apply_vehicle_digital_control_inputs(VehicleParameterState *state_, int32 button_states_)
{
  if((button_states_ &
      g_STARTUP_CONFIGURATION.control_button_masks[VEHICLE_CONTROL_MASK_ACCELERATE_INDEX]) != 0)
    {
      state_->secondary_control_value = 0;
      adjust_vehicle_primary_control_value(state_, VEHICLE_CONTROL_FULL_VALUE);
    }
  else if((button_states_ &
           g_STARTUP_CONFIGURATION.control_button_masks[VEHICLE_CONTROL_MASK_BRAKE_INDEX]) != 0)
    {
      adjust_vehicle_secondary_control_value(state_, VEHICLE_CONTROL_FULL_VALUE);
      adjust_vehicle_primary_control_value(state_, 0);
    }
  else
    {
      if(state_->secondary_control_value > VEHICLE_CONTROL_SECONDARY_MAX_VALUE)
        {
          state_->secondary_control_value = VEHICLE_CONTROL_SECONDARY_MAX_VALUE;
        }

      if(state_->primary_control_value != 0)
        {
          adjust_vehicle_primary_control_value(state_, 0);
        }
      else if(state_->secondary_control_value != 0)
        {
          adjust_vehicle_secondary_control_value(state_, 0);
        }
    }

  state_->auxiliary_control_active =
    (button_states_ &
     g_STARTUP_CONFIGURATION.control_button_masks[VEHICLE_CONTROL_MASK_AUXILIARY_INDEX]) != 0;

  if((button_states_ &
      g_STARTUP_CONFIGURATION.control_button_masks[VEHICLE_CONTROL_MASK_GEAR_UP_INDEX]) != 0)
    {
      adjust_vehicle_gear_state(state_, VEHICLE_GEAR_UP_ACTION);
    }
  else
    {
      adjust_vehicle_gear_state(
        state_,
        (button_states_ &
         g_STARTUP_CONFIGURATION.control_button_masks[VEHICLE_CONTROL_MASK_GEAR_TOGGLE_INDEX]) !=
            0 ?
          VEHICLE_GEAR_TOGGLE_ACTIVE_ACTION :
          VEHICLE_GEAR_TOGGLE_INACTIVE_ACTION);
    }
}
