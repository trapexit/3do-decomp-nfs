#include "vehicle_parameter_loader.h"

#include "control_input.h"
#include "control_input_poll_state.h"
#include "diagnostics.h"
#include "disk_stream_cleanup.h"
#include "display_coordinate_state.h"
#include "distance_sample.h"
#include "fixed_math.h"
#include "game_bootstrap.h"
#include "game_frame_count.h"
#include "geometry.h"
#include "memory.h"
#include "simulation_snapshot.h"
#include "simulation_thread_signal.h"
#include "text_line_reader.h"
#include "vehicle_dynamics_state.h"
#include "vehicle_selection_state.h"
#include "vehicle_simulation_state.h"

#include "operamath.h"

#define VEHICLE_SECONDARY_CONTROL_RISE_DIVISOR 4
#define VEHICLE_TORQUE_SCALE_DEFAULT 0x100
#define VEHICLE_TORQUE_REDUCED_SCALE 0xB4
#define VEHICLE_TORQUE_SCALE_JOYSTICK_PRESET 5
#define VEHICLE_TORQUE_SCALE_START_ADJUST 0x0D
#define VEHICLE_TORQUE_SECRET_VEHICLE_LIMIT 6
#define VEHICLE_TORQUE_SECRET_FIRST_INDEX 1
#define VEHICLE_TORQUE_SECRET_SECOND_INDEX 4
#define VEHICLE_TORQUE_SECRET_THIRD_INDEX 5
#define VEHICLE_TORQUE_SECRET_SPECIAL_ADJUST 0x37
#define VEHICLE_TORQUE_SECRET_NORMAL_ADJUST 0x44
#define VEHICLE_TORQUE_SECRET_HIGH_ADJUST 0x55
#define VEHICLE_TORQUE_SECRET_TIMER 20
#define VEHICLE_TORQUE_FIXED8_SHIFT 8
#define VEHICLE_TORQUE_FIXED8_ROUNDING 0xFF
#define VEHICLE_SECONDARY_CONTROL_THRESHOLD 0x90
#define VEHICLE_GEAR_SHIFT_CONTROL_MODE 1
#define VEHICLE_GEAR_CONTROL_INACTIVE_ACTION 0
#define VEHICLE_GEAR_CONTROL_DOWNSHIFT_ACTION 1
#define VEHICLE_GEAR_CONTROL_UPSHIFT_ACTION 2
#define VEHICLE_GEAR_CONTROL_MANUAL_STAGE 0
#define VEHICLE_GEAR_CONTROL_MINIMUM_STAGE 1
#define VEHICLE_GEAR_CONTROL_TRANSITION_STAGE 2
#define VEHICLE_GEAR_CONTROL_MAXIMUM_STAGE 3
#define VEHICLE_GEAR_SHIFT_DELAY_HALF_DIVISOR 2
#define VEHICLE_MODE_SELECTION_JOYSTICK_PRESET 5
#define VEHICLE_MODE_SELECTION_BUTTON_INDEX 2
#define VEHICLE_MODE_SELECTION_JOYSTICK_MASK 0x10000000
#define VEHICLE_MODE_SELECTION_LATCH_VALUE 0x100
#define VEHICLE_GEAR_SHIFT_ENGINE_SPEED_MARGIN 0x190
#define VEHICLE_FIXED8_SHIFT 8
#define VEHICLE_FIXED16_SHIFT 16
#define VEHICLE_GEAR_INDEX_LIMIT_OFFSET 3
#define VEHICLE_UPSHIFT_GEAR_RATIO_OFFSET 3
#define VEHICLE_DOWNSHIFT_GEAR_RATIO_OFFSET 1
#define VEHICLE_UPSHIFT_THRESHOLD_OFFSET 2
#define VEHICLE_DOWNSHIFT_THRESHOLD_OFFSET 1
#define VEHICLE_DOWNSHIFT_THRESHOLD_NUMERATOR 15
#define VEHICLE_DOWNSHIFT_THRESHOLD_DIVISOR 16
#define VEHICLE_SPECIAL_SHIFT_VEHICLE_INDEX 4
#define VEHICLE_SPECIAL_SHIFT_FRAME_MASK 0x31
#define VEHICLE_SPECIAL_SHIFT_FRAME_PHASE 0x10
#define VEHICLE_SPECIAL_SHIFT_DELAY_FRAMES 3
#define VEHICLE_ENGINE_SPEED_GEAR_RATIO_OFFSET 2
#define VEHICLE_MINIMUM_TARGET_ENGINE_SPEED 700
#define VEHICLE_IDLE_CONTROL_THRESHOLD 13
#define VEHICLE_NEUTRAL_GEAR_INDEX -1
#define VEHICLE_NEUTRAL_ENGINE_ADJUST_DIVISOR 2
#define VEHICLE_DRIVEN_WHEEL_SKID_FLAG 2
#define VEHICLE_SKID_ENGINE_CONTROL_THRESHOLD 220
#define VEHICLE_SKID_ENGINE_DECAY_DIVISOR 8
#define VEHICLE_ENGINE_SPEED_LIMIT 11000
#define VEHICLE_ENGINE_BRAKING_SCALE 4
#define VEHICLE_DRIVE_FORCE_SPEED_SCALE 16
#define VEHICLE_DRIVE_FORCE_LIMIT 0x70000
#define VEHICLE_WHEEL_SPEED_DIFFERENCE_LIMIT 0x30000
#define VEHICLE_WHEEL_COUPLING_CONTROL_LIMIT 0xF0
#define VEHICLE_WHEEL_SPIN_CONTROL_LIMIT 0xFA
#define VEHICLE_WHEEL_SPIN_GEAR_LIMIT 2
#define VEHICLE_WHEEL_COUPLING_THRESHOLD_INDEX 3
#define VEHICLE_WHEEL_COUPLING_SPEED_MARGIN 500
#define VEHICLE_RESISTANCE_SPEED_MULTIPLIER 30
#define VEHICLE_RESISTANCE_ROUNDING_BIAS 0xFF
#define VEHICLE_RESISTANCE_HALF_SHIFT 1
#define VEHICLE_RESISTANCE_QUARTER_SHIFT 2
#define VEHICLE_SURFACE_SOUND_OFF_ROAD 4
#define VEHICLE_SURFACE_FIXED_SHIFT 8
#define VEHICLE_SPEED_SQUARED_SHIFT 16
#define VEHICLE_SURFACE_BASE_ROAD_INDEX 0
#define VEHICLE_SURFACE_BASE_LOOSE_FIRST_INDEX 1
#define VEHICLE_SURFACE_BASE_LOOSE_SECOND_INDEX 2
#define VEHICLE_SURFACE_ALT_ROAD_INDEX 3
#define VEHICLE_SURFACE_ALT_LOOSE_FIRST_INDEX 4
#define VEHICLE_SURFACE_ALT_LOOSE_SECOND_INDEX 5
#define VEHICLE_SURFACE_ROAD_GRIP_SCALE 0x100
#define VEHICLE_SURFACE_ROAD_RESISTANCE_SCALE 0x100
#define VEHICLE_SURFACE_ROAD_ROLLING_RESISTANCE 0x3333
#define VEHICLE_SURFACE_BASE_LOOSE_GRIP_SCALE 0xB3
#define VEHICLE_SURFACE_BASE_LOOSE_DRAG_SCALE 0x1400
#define VEHICLE_SURFACE_BASE_LOOSE_RESISTANCE 0x8CCC
#define VEHICLE_SURFACE_ALT_LOOSE_GRIP_SCALE 0x4C
#define VEHICLE_SURFACE_ALT_LOOSE_DRAG_SCALE 0x300
#define VEHICLE_SURFACE_ALT_LOOSE_RESISTANCE 0x1999
#define VEHICLE_SURFACE_SPECIAL_ROAD_SCALE 0x66
#define VEHICLE_TIRE_FORCE_LIMIT_NUMERATOR 11
#define VEHICLE_TIRE_FORCE_LIMIT_SHIFT 3
#define VEHICLE_TIRE_FORCE_SLIP_FLAG_SHIFT 7
#define VEHICLE_TIRE_FORCE_HALF_DIVISOR 2
#define VEHICLE_TIRE_FORCE_VECTOR_SHIFT 2
#define VEHICLE_TIRE_FORCE_DEMAND_LIMIT_NUMERATOR 5
#define VEHICLE_TIRE_COEFFICIENT_ADJUSTMENT_DIVISOR 2
#define VEHICLE_TIRE_FORCE_SCALE_LOOKUP_LIMIT 0x1FFFC0
#define VEHICLE_TIRE_FORCE_SCALE_INPUT_LIMIT 0x1FFFFE
#define VEHICLE_TIRE_FORCE_SCALE_INDEX_SHIFT 12
#define VEHICLE_TIRE_FORCE_SCALE_OUTPUT_SHIFT 9
#define VEHICLE_FRONT_AXLE_INDEX 1
#define VEHICLE_TIRE_STATE_SLIP_ANGLE_FLAG 1
#define VEHICLE_TIRE_STATE_LONGITUDINAL_SLIP_FLAG 2
#define VEHICLE_TIRE_STATE_LATERAL_SLIP_FLAG 4
#define VEHICLE_TIRE_FORCE_SCALE_SHIFT 8
#define VEHICLE_TIRE_LATERAL_DEMAND_MULTIPLIER 16
#define VEHICLE_TIRE_LATERAL_SLIP_ANGLE_LIMIT 0x30000
#define VEHICLE_TIRE_LONGITUDINAL_LIMIT_NUMERATOR 7
#define VEHICLE_TIRE_LONGITUDINAL_LIMIT_DIVISOR 4
#define VEHICLE_TIRE_AVAILABLE_FORCE_MULTIPLIER 2
#define VEHICLE_TIRE_FORCE_REDUCTION_DIVISOR 4
#define VEHICLE_TIRE_FORCE_CONTROL_SPEED_LIMIT 0xC0000
#define VEHICLE_TIRE_FORCE_CONTROL_ENGINE_LIMIT 4000
#define VEHICLE_TIRE_FORCE_CONTROL_ANGLE_LIMIT 0x190000
#define VEHICLE_TIRE_FORCE_CONTROL_INPUT_LIMIT 0xF0
#define VEHICLE_TIRE_FORCE_CONTROL_REDUCTION_SHIFT 4
#define VEHICLE_TIRE_FRONT_FORCE_REDUCTION_DIVISOR 4
#define VEHICLE_TIRE_SECONDARY_CONTROL_LIMIT 0x90
#define VEHICLE_TIRE_COMBINED_LIMIT_NUMERATOR 5
#define VEHICLE_TIRE_COMBINED_LIMIT_SHIFT 10
#define VEHICLE_GRAVITY_FIXED 0x9CF5C
#define VEHICLE_ANGULAR_UNIT_SCALE_FIXED 0x324
#define VEHICLE_ANGLE_REVOLUTION_LIMIT 0x1000000
#define VEHICLE_ANGLE_WARNING_FORMAT "ANGLE WARNING!! %s=%d"
#define VEHICLE_PARAMETER_ALLOCATION_NAME "Carspecs"
#define VEHICLE_PARAMETER_FIXED_ONE 0x10000
#define VEHICLE_PARAMETER_FIXED_HALF 0x8000
#define VEHICLE_PARAMETER_MPH_TO_MPS 0x71C7
#define VEHICLE_PARAMETER_FEET_TO_METERS 0x4E07
#define VEHICLE_PARAMETER_GRAVITY 0x9CF5C
#define VEHICLE_PARAMETER_FIRST_GEAR_INDEX 0
#define VEHICLE_PARAMETER_SECOND_GEAR_INDEX 1
#define VEHICLE_PARAMETER_FIRST_SHIFT_INDEX 2
#define VEHICLE_PARAMETER_SHIFT_END_OFFSET 1
#define VEHICLE_AUTOMATIC_STEERING_THRESHOLD_COUNT 4

VehicleGearShiftStatistics *g_VEHICLE_GEAR_SHIFT_STATISTICS;
VehicleParameters          *g_VEHICLE_PARAMETERS;
int32                       g_SIMULATION_MODE_INPUT_LATCH;
VehicleSurfacePhysicsEntry  g_VEHICLE_SURFACE_PHYSICS[VEHICLE_SURFACE_PHYSICS_ENTRY_COUNT];
uint8                       g_VEHICLE_SURFACE_PHYSICS_RESERVED_STATE[0x30];

void
scale_vehicle_engine_torque_curve(VehicleParameters *parameters_)
{
  int32 scale;
  int32 scaled_efficiency;
  int32 sample_index;

  scale = VEHICLE_TORQUE_SCALE_DEFAULT;
  if(g_STARTUP_CONFIGURATION.performance_reduction_enabled != 0)
    scale = VEHICLE_TORQUE_REDUCED_SCALE;
  if(g_STARTUP_CONFIGURATION.control_preset_index == VEHICLE_TORQUE_SCALE_JOYSTICK_PRESET)
    {
      scale += VEHICLE_TORQUE_SCALE_START_ADJUST;
    }

  if(g_CONTROL_INPUT_SECRET_ENABLED != 0)
    {
      if(g_PLAYER_VEHICLE_INDEX < VEHICLE_TORQUE_SECRET_VEHICLE_LIMIT)
        {
          if((g_PLAYER_VEHICLE_INDEX == VEHICLE_TORQUE_SECRET_FIRST_INDEX) ||
             (g_PLAYER_VEHICLE_INDEX == VEHICLE_TORQUE_SECRET_SECOND_INDEX) ||
             (g_PLAYER_VEHICLE_INDEX == VEHICLE_TORQUE_SECRET_THIRD_INDEX))
            {
              scale += VEHICLE_TORQUE_SECRET_SPECIAL_ADJUST;
            }
          else
            scale += VEHICLE_TORQUE_SECRET_NORMAL_ADJUST;
        }
      else
        scale += VEHICLE_TORQUE_SECRET_HIGH_ADJUST;
      g_VEHICLE_STATISTICS_NOTIFICATION_TIMER = VEHICLE_TORQUE_SECRET_TIMER;
    }

  scaled_efficiency = parameters_->drivetrain_efficiency * scale;
  if(scaled_efficiency < 0)
    scaled_efficiency += VEHICLE_TORQUE_FIXED8_ROUNDING;
  parameters_->drivetrain_efficiency = scaled_efficiency >> VEHICLE_TORQUE_FIXED8_SHIFT;

  for(sample_index = 0; sample_index < parameters_->engine_torque_sample_count; sample_index++)
    {
      parameters_->engine_torque_samples[sample_index].value = multiply_five_fixed16(
        integer_to_fixed16(parameters_->engine_torque_samples[sample_index].value),
        parameters_->final_drive_ratio,
        parameters_->drivetrain_efficiency,
        parameters_->inverse_rolling_radius,
        parameters_->inverse_mass);
    }
}

int32
lookup_vehicle_tire_force_scale(const VehicleParameterState *state_,
                                int32                        force_magnitude_,
                                int32                        table_index_)
{
  if(force_magnitude_ >= VEHICLE_TIRE_FORCE_SCALE_LOOKUP_LIMIT)
    force_magnitude_ = VEHICLE_TIRE_FORCE_SCALE_INPUT_LIMIT;

  return (
    (int32) state_->tire_force_scale_tables[table_index_][force_magnitude_ >>
                                                          VEHICLE_TIRE_FORCE_SCALE_INDEX_SHIFT]
    << VEHICLE_TIRE_FORCE_SCALE_OUTPUT_SHIFT);
}

VehicleParameters *
load_vehicle_parameters_from_file(const char *path_)
{
  Stream *stream;
  int32   braking_distance;
  int32   braking_speed;
  int32   braking_time;
  int32   fixed8_value;
  uint32  index;

  stream = open_disk_stream_default((char *) path_);
  g_VEHICLE_PARAMETERS =
    (VehicleParameters *) allocate_named_memory(VEHICLE_PARAMETER_ALLOCATION_NAME,
                                                sizeof(*g_VEHICLE_PARAMETERS),
                                                0);
  if(g_VEHICLE_PARAMETERS == NULL)
    return NULL;

  g_VEHICLE_PARAMETERS->mass        = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->body_length = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->body_width  = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->wheelbase   = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->wheel_track = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->inverse_wheelbase =
    DivSF16(integer_to_fixed16(1), g_VEHICLE_PARAMETERS->wheelbase);
  g_VEHICLE_PARAMETERS->inverse_wheel_track =
    DivSF16(integer_to_fixed16(1), g_VEHICLE_PARAMETERS->wheel_track);
  g_VEHICLE_PARAMETERS->rear_mass_ratio = read_noncomment_fixed16_text_line(stream);
  /* Retail consumes the configured ratio, then forces an even axle-mass split. */
  g_VEHICLE_PARAMETERS->rear_mass_ratio = VEHICLE_PARAMETER_FIXED_HALF;
  g_VEHICLE_PARAMETERS->rear_axle_mass =
    MulSF16(g_VEHICLE_PARAMETERS->mass, g_VEHICLE_PARAMETERS->rear_mass_ratio);
  g_VEHICLE_PARAMETERS->front_axle_mass =
    g_VEHICLE_PARAMETERS->mass - g_VEHICLE_PARAMETERS->rear_axle_mass;
  g_VEHICLE_PARAMETERS->inverse_mass = DivSF16(integer_to_fixed16(1), g_VEHICLE_PARAMETERS->mass);
  g_VEHICLE_PARAMETERS->inverse_front_axle_mass =
    DivSF16(integer_to_fixed16(1), g_VEHICLE_PARAMETERS->front_axle_mass);
  g_VEHICLE_PARAMETERS->inverse_rear_axle_mass =
    DivSF16(integer_to_fixed16(1), g_VEHICLE_PARAMETERS->rear_axle_mass);

  g_VEHICLE_PARAMETERS->front_drive_ratio = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->front_brake_ratio = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->rear_brake_ratio =
    VEHICLE_PARAMETER_FIXED_ONE - g_VEHICLE_PARAMETERS->front_brake_ratio;
  g_VEHICLE_PARAMETERS->inertia_factor            = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->body_roll_scale           = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->body_pitch_scale          = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->front_friction_multiplier = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->rear_friction_multiplier  = read_noncomment_fixed16_text_line(stream);

  fixed8_value = read_noncomment_fixed16_text_line(stream) >> VEHICLE_FIXED8_SHIFT;
  g_VEHICLE_PARAMETERS->front_lateral_tire_force_scale = fixed8_value;
  g_VEHICLE_PARAMETERS->front_longitudinal_tire_force_scale =
    VEHICLE_PARAMETER_FIXED_ONE / fixed8_value;
  fixed8_value = read_noncomment_fixed16_text_line(stream) >> VEHICLE_FIXED8_SHIFT;
  g_VEHICLE_PARAMETERS->rear_lateral_tire_force_scale = fixed8_value;
  g_VEHICLE_PARAMETERS->rear_longitudinal_tire_force_scale =
    VEHICLE_PARAMETER_FIXED_ONE / fixed8_value;
  g_VEHICLE_PARAMETERS->burnout_lateral_force_divisor =
    read_noncomment_fixed16_text_line(stream) >> VEHICLE_FIXED8_SHIFT;
  g_VEHICLE_PARAMETERS->lateral_acceleration_limit = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->anti_lock_brakes_enabled   = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->traction_control_enabled   = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->primary_control_rise_limit =
    read_noncomment_fixed16_text_line(stream) >> VEHICLE_FIXED8_SHIFT;
  g_VEHICLE_PARAMETERS->primary_control_fall_limit =
    read_noncomment_fixed16_text_line(stream) >> VEHICLE_FIXED8_SHIFT;
  g_VEHICLE_PARAMETERS->secondary_control_low_rise_limit =
    read_noncomment_fixed16_text_line(stream) >> VEHICLE_FIXED8_SHIFT;
  g_VEHICLE_PARAMETERS->secondary_control_high_rise_limit =
    read_noncomment_fixed16_text_line(stream) >> VEHICLE_FIXED8_SHIFT;
  g_VEHICLE_PARAMETERS->secondary_control_low_fall_limit =
    read_noncomment_fixed16_text_line(stream) >> VEHICLE_FIXED8_SHIFT;
  g_VEHICLE_PARAMETERS->secondary_control_high_fall_limit =
    read_noncomment_fixed16_text_line(stream) >> VEHICLE_FIXED8_SHIFT;

  g_ANTI_LOCK_BRAKES_ENABLED =
    ((g_ANTI_LOCK_BRAKES_ENABLED > 0) && (g_VEHICLE_PARAMETERS->anti_lock_brakes_enabled > 0));
  g_TRACTION_CONTROL_ENABLED =
    ((g_TRACTION_CONTROL_ENABLED > 0) && (g_VEHICLE_PARAMETERS->traction_control_enabled > 0));

  g_VEHICLE_PARAMETERS->center_of_gravity_height = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->roll_axis_height         = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->front_roll_stiffness     = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->rear_roll_stiffness      = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->front_roll_stiffness_ratio =
    DivSF16(g_VEHICLE_PARAMETERS->front_roll_stiffness,
            g_VEHICLE_PARAMETERS->front_roll_stiffness + g_VEHICLE_PARAMETERS->rear_roll_stiffness);
  g_VEHICLE_PARAMETERS->rear_roll_stiffness_ratio =
    VEHICLE_PARAMETER_FIXED_ONE - g_VEHICLE_PARAMETERS->front_roll_stiffness_ratio;
  g_VEHICLE_PARAMETERS->center_of_gravity_above_roll_axis =
    g_VEHICLE_PARAMETERS->center_of_gravity_height - g_VEHICLE_PARAMETERS->roll_axis_height;
  g_VEHICLE_PARAMETERS->surface_resistance_scale = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->reverse_resistance_speed_threshold =
    read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->final_drive_ratio     = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->drivetrain_efficiency = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->rolling_radius        = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->inverse_rolling_radius =
    DivSF16(integer_to_fixed16(1), g_VEHICLE_PARAMETERS->rolling_radius);
  g_VEHICLE_PARAMETERS->wheel_speed_to_engine_speed_scale =
    read_noncomment_fixed16_text_line(stream);

  braking_speed = MulSF16(VEHICLE_PARAMETER_MPH_TO_MPS, read_noncomment_fixed16_text_line(stream));
  braking_distance =
    MulSF16(VEHICLE_PARAMETER_FEET_TO_METERS, read_noncomment_fixed16_text_line(stream));
  braking_time = DivSF16(braking_distance, braking_speed);
  g_VEHICLE_PARAMETERS->maximum_tire_coefficient =
    DivSF16(DivSF16(braking_speed, braking_time << 1), VEHICLE_PARAMETER_GRAVITY);
  g_VEHICLE_PARAMETERS->auxiliary_brake_force   = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->tire_slip_angle_limit   = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->normal_coefficient_loss = read_noncomment_fixed16_text_line(stream);

  g_VEHICLE_PARAMETERS->gear_count = read_noncomment_integer_text_line(stream);
  g_VEHICLE_PARAMETERS->gear_ratios[VEHICLE_PARAMETER_FIRST_GEAR_INDEX] =
    read_noncomment_fixed16_text_line(stream);
  index = VEHICLE_PARAMETER_SECOND_GEAR_INDEX;
  while(index < (uint32) g_VEHICLE_PARAMETERS->gear_count)
    {
      g_VEHICLE_PARAMETERS->gear_ratios[index] = read_noncomment_fixed16_text_line(stream);
      index++;
    }

  index = VEHICLE_PARAMETER_FIRST_SHIFT_INDEX;
  while(index < (uint32) (g_VEHICLE_PARAMETERS->gear_count - VEHICLE_PARAMETER_SHIFT_END_OFFSET))
    {
      g_VEHICLE_PARAMETERS->gear_shift_thresholds[index] =
        read_noncomment_integer_text_line(stream);
      index++;
    }

  g_VEHICLE_PARAMETERS->maximum_engine_speed = read_noncomment_integer_text_line(stream);
  g_VEHICLE_PARAMETERS->minimum_engine_speed = read_noncomment_integer_text_line(stream);
  g_VEHICLE_PARAMETERS->ride_height          = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->visual_center_y      = read_noncomment_integer_text_line(stream);
  g_VEHICLE_PARAMETERS->maximum_automatic_steering_angle =
    read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->automatic_steering_ramp_multiply_shift =
    read_noncomment_integer_text_line(stream);
  g_VEHICLE_PARAMETERS->automatic_steering_ramp_divide_shift =
    read_noncomment_integer_text_line(stream);
  g_VEHICLE_PARAMETERS->automatic_steering_model = read_noncomment_integer_text_line(stream);
  index                                          = 0;
  while(index < VEHICLE_AUTOMATIC_STEERING_THRESHOLD_COUNT)
    {
      g_VEHICLE_PARAMETERS->automatic_steering_speed_thresholds[index] =
        read_noncomment_integer_text_line(stream);
      index++;
    }
  g_VEHICLE_PARAMETERS->automatic_steering_ramp_scale  = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->automatic_steering_attenuation = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_PARAMETERS->alternate_steering_ramp_multiply_shift =
    read_noncomment_integer_text_line(stream);
  g_VEHICLE_PARAMETERS->alternate_steering_ramp_divide_shift =
    read_noncomment_integer_text_line(stream);
  g_VEHICLE_PARAMETERS->gear_shift_delay = read_noncomment_integer_text_line(stream);
  g_VEHICLE_PARAMETERS->disengaged_engine_speed_adjustment =
    read_noncomment_integer_text_line(stream);
  g_VEHICLE_PARAMETERS->disengaged_engine_speed_acceleration =
    read_noncomment_integer_text_line(stream);
  g_VEHICLE_PARAMETERS->engaged_engine_speed_deceleration =
    read_noncomment_integer_text_line(stream);
  g_VEHICLE_PARAMETERS->engaged_engine_speed_acceleration =
    read_noncomment_integer_text_line(stream);
  g_VEHICLE_PARAMETERS->drivetrain_force_scale = read_noncomment_fixed16_text_line(stream);

  g_VEHICLE_PARAMETERS->engine_torque_sample_count = read_noncomment_integer_text_line(stream);
  g_VEHICLE_PARAMETERS->engine_torque_samples[0].distance =
    read_noncomment_integer_text_line(stream);
  g_VEHICLE_PARAMETERS->engine_torque_samples[0].value = read_noncomment_integer_text_line(stream);
  index                                                = 1;
  while(index < (uint32) g_VEHICLE_PARAMETERS->engine_torque_sample_count)
    {
      g_VEHICLE_PARAMETERS->engine_torque_samples[index].distance =
        read_noncomment_integer_text_line(stream);
      g_VEHICLE_PARAMETERS->engine_torque_samples[index].value =
        read_noncomment_integer_text_line(stream);
      index++;
    }

  if(close_disk_stream_and_return_zero(stream) == -1)
    return NULL;

  return g_VEHICLE_PARAMETERS;
}

int32
load_vehicle_parameter_state(VehicleParameterState *state_, const char *path_)
{
  state_->parameters = (VehicleParameters *) load_vehicle_parameters_from_file(path_);
  if(state_->parameters != NULL)
    return 1;

  return 0;
}

void
adjust_vehicle_primary_control_value(VehicleParameterState *state_, int32 target_value_)
{
  int32 value_difference;

  if(state_->primary_control_value < target_value_)
    {
      value_difference = target_value_ - state_->primary_control_value;
      if(value_difference > state_->parameters->primary_control_rise_limit)
        target_value_ =
          state_->primary_control_value + state_->parameters->primary_control_rise_limit;
    }
  else
    {
      if(state_->primary_control_value <= target_value_)
        return;

      value_difference = state_->primary_control_value - target_value_;
      if(value_difference > state_->parameters->primary_control_fall_limit)
        target_value_ =
          state_->primary_control_value - state_->parameters->primary_control_fall_limit;
    }

  state_->primary_control_value = target_value_;
}

void
adjust_vehicle_secondary_control_value(VehicleParameterState *state_, int32 target_value_)
{
  int32 control_limit;
  int32 value_difference;

  if(state_->secondary_control_value < target_value_)
    {
      if(state_->secondary_control_value < VEHICLE_SECONDARY_CONTROL_THRESHOLD)
        {
          control_limit = state_->parameters->secondary_control_low_rise_limit;
          control_limit += control_limit / VEHICLE_SECONDARY_CONTROL_RISE_DIVISOR;
        }
      else
        {
          control_limit = state_->parameters->secondary_control_high_rise_limit;
        }

      value_difference = target_value_ - state_->secondary_control_value;
      if(value_difference > control_limit)
        target_value_ = state_->secondary_control_value + control_limit;
    }
  else
    {
      if(state_->secondary_control_value <= target_value_)
        return;

      if(state_->secondary_control_value >= VEHICLE_SECONDARY_CONTROL_THRESHOLD)
        {
          control_limit = state_->parameters->secondary_control_high_fall_limit;
        }
      else
        {
          control_limit = state_->parameters->secondary_control_low_fall_limit;
        }

      value_difference = state_->secondary_control_value - target_value_;
      if(value_difference > control_limit)
        target_value_ = state_->secondary_control_value - control_limit;
    }

  state_->secondary_control_value = target_value_;
}

void
record_vehicle_gear_shift(VehicleParameterState *state_, int32 direction_)
{
  if(G_FRONTEND_EXIT_REQUESTED != VEHICLE_GEAR_SHIFT_CONTROL_MODE)
    return;

  if(direction_ > 0)
    {
      g_VEHICLE_GEAR_SHIFT_STATISTICS->upshift_count++;
      g_VEHICLE_GEAR_SHIFT_STATISTICS->upshift_engine_speed_total += state_->engine_speed;
    }
  else
    {
      g_VEHICLE_GEAR_SHIFT_STATISTICS->downshift_count++;
      g_VEHICLE_GEAR_SHIFT_STATISTICS->downshift_engine_speed_total += state_->engine_speed;
    }

  if(state_->parameters->maximum_engine_speed - VEHICLE_GEAR_SHIFT_ENGINE_SPEED_MARGIN >=
     state_->engine_speed)
    {
      return;
    }

  g_VEHICLE_GEAR_SHIFT_STATISTICS->high_engine_speed_shift_count++;
}

void
adjust_vehicle_gear_state(VehicleParameterState *state_, int32 action_)
{
  const VehicleParameters *parameters;
  int32                    gear_stage;
  int32                    mode_input_active;
  int32                    shift_delay;

  parameters        = state_->parameters;
  mode_input_active = 0;
  if(G_FRONTEND_EXIT_REQUESTED == VEHICLE_GEAR_SHIFT_CONTROL_MODE)
    {
      if((g_STARTUP_CONFIGURATION.control_preset_index != VEHICLE_MODE_SELECTION_JOYSTICK_PRESET) &&
         ((g_CONTROL_INPUT_POLL_STATE.status_00 &
           g_STARTUP_CONFIGURATION.control_button_masks[VEHICLE_MODE_SELECTION_BUTTON_INDEX]) != 0))
        {
          mode_input_active = 1;
        }
      else if((g_JOYSTICK_CALIBRATION_STATE.buttons & VEHICLE_MODE_SELECTION_JOYSTICK_MASK) != 0)
        {
          mode_input_active = 1;
        }

      if(mode_input_active != 0)
        {
          if((g_SIMULATION_MODE_INPUT_LATCH & VEHICLE_MODE_SELECTION_LATCH_VALUE) == 0)
            {
              update_simulation_mode_selection(&g_SIMULATION_RENDER_STATE);
            }
        }
    }

  if(action_ != VEHICLE_GEAR_CONTROL_INACTIVE_ACTION)
    {
      gear_stage = state_->gear_control_stage;
      if(gear_stage >= VEHICLE_GEAR_CONTROL_MINIMUM_STAGE)
        {
          if(state_->gear_control_mode != VEHICLE_GEAR_CONTROL_DOWNSHIFT_ACTION)
            {
              if((action_ == VEHICLE_GEAR_CONTROL_DOWNSHIFT_ACTION) &&
                 (gear_stage >= VEHICLE_GEAR_CONTROL_TRANSITION_STAGE))
                {
                  gear_stage--;
                  state_->gear_control_stage = gear_stage;
                  if(gear_stage == VEHICLE_GEAR_CONTROL_TRANSITION_STAGE)
                    {
                      state_->gear_control_ready = 0;
                      state_->gear_control_index = VEHICLE_NEUTRAL_GEAR_INDEX;
                    }
                  else
                    {
                      state_->gear_control_index   = 0;
                      state_->gear_control_ready   = 1;
                      state_->gear_control_pending = 0;
                    }
                  goto finalize_gear_control;
                }
              if(state_->gear_control_mode == VEHICLE_GEAR_CONTROL_UPSHIFT_ACTION)
                goto finalize_gear_control;
            }

          if((action_ == VEHICLE_GEAR_CONTROL_UPSHIFT_ACTION) &&
             (gear_stage < VEHICLE_GEAR_CONTROL_MAXIMUM_STAGE))
            {
              gear_stage++;
              state_->gear_control_stage = gear_stage;
              if(gear_stage == VEHICLE_GEAR_CONTROL_TRANSITION_STAGE)
                {
                  state_->gear_control_ready = 0;
                  state_->gear_control_index = VEHICLE_NEUTRAL_GEAR_INDEX;
                }
              else
                {
                  state_->gear_control_index = 0;
                  state_->gear_control_ready = 1;
                  if(gear_stage == VEHICLE_GEAR_CONTROL_MAXIMUM_STAGE)
                    state_->gear_control_index = 0;
                }
            }
        }
      else
        {
          if(state_->gear_control_mode != VEHICLE_GEAR_CONTROL_DOWNSHIFT_ACTION)
            {
              if((action_ == VEHICLE_GEAR_CONTROL_DOWNSHIFT_ACTION) &&
                 (state_->gear_control_index >= VEHICLE_NEUTRAL_GEAR_INDEX))
                {
                  state_->gear_control_index--;
                  state_->gear_control_ready = 0;
                  state_->gear_shift_delay =
                    parameters->gear_shift_delay / VEHICLE_GEAR_SHIFT_DELAY_HALF_DIVISOR;
                  state_->deferred_primary_control_value = state_->primary_control_value;
                  state_->gear_control_pending           = 1;
                  record_vehicle_gear_shift(state_, -1);
                  goto finalize_gear_control;
                }
              if(state_->gear_control_mode == VEHICLE_GEAR_CONTROL_UPSHIFT_ACTION)
                goto finalize_gear_control;
            }

          if((action_ == VEHICLE_GEAR_CONTROL_UPSHIFT_ACTION) &&
             (state_->gear_control_index <
              (parameters->gear_count - VEHICLE_GEAR_INDEX_LIMIT_OFFSET)))
            {
              state_->gear_control_index++;
              state_->gear_control_ready = 0;
              shift_delay                = parameters->gear_shift_delay;
              if((g_PLAYER_VEHICLE_INDEX == VEHICLE_SPECIAL_SHIFT_VEHICLE_INDEX) &&
                 ((G_GAME_FRAME_COUNTER & VEHICLE_SPECIAL_SHIFT_FRAME_MASK) ==
                  VEHICLE_SPECIAL_SHIFT_FRAME_PHASE))
                {
                  shift_delay += VEHICLE_SPECIAL_SHIFT_DELAY_FRAMES;
                }
              state_->gear_shift_delay               = shift_delay;
              state_->deferred_primary_control_value = state_->primary_control_value;
              state_->gear_control_pending           = 1;
              record_vehicle_gear_shift(state_, 1);
            }
        }
    }

finalize_gear_control:
  state_->gear_control_mode = action_;
  if((G_FRONTEND_EXIT_REQUESTED == VEHICLE_GEAR_SHIFT_CONTROL_MODE) && (mode_input_active != 0))
    {
      g_SIMULATION_MODE_INPUT_LATCH = VEHICLE_MODE_SELECTION_LATCH_VALUE;
    }
  else
    {
      g_SIMULATION_MODE_INPUT_LATCH = 0;
    }

  if(state_->gear_control_stage != VEHICLE_GEAR_CONTROL_MANUAL_STAGE)
    return;
  if(state_->gear_shift_delay < 0)
    return;

  if(state_->gear_shift_delay == 0)
    {
      state_->primary_control_value = state_->deferred_primary_control_value;
      state_->gear_control_pending  = 0;
      if(state_->gear_control_index != VEHICLE_NEUTRAL_GEAR_INDEX)
        state_->gear_control_ready = 1;
    }
  state_->gear_shift_delay--;
}

/* Keep both adjacent-gear predictions together because they form one selection operation. */
void
adjust_vehicle_automatic_gear(VehicleParameterState *state_, const VehicleParameters *parameters_)
{
  int32 scaled_vehicle_speed;
  int32 predicted_engine_speed;
  int32 downshift_threshold;
  int32 gear_index;

  gear_index = state_->gear_control_index;
  if(((gear_index + VEHICLE_GEAR_INDEX_LIMIT_OFFSET) != parameters_->gear_count) &&
     (parameters_->gear_shift_thresholds[gear_index + VEHICLE_UPSHIFT_THRESHOLD_OFFSET] <
      state_->target_engine_speed))
    {
      scaled_vehicle_speed =
        (((state_->parameters->wheel_speed_to_engine_speed_scale >> VEHICLE_FIXED8_SHIFT) *
          (state_->vehicle_speed >> VEHICLE_FIXED8_SHIFT)) >>
         VEHICLE_FIXED8_SHIFT);
      predicted_engine_speed =
        (((state_->parameters->gear_ratios[gear_index + VEHICLE_UPSHIFT_GEAR_RATIO_OFFSET] >>
           VEHICLE_FIXED8_SHIFT) *
          scaled_vehicle_speed) >>
         VEHICLE_FIXED16_SHIFT);
      state_->target_engine_speed = predicted_engine_speed;
      state_->gear_control_index++;
      return;
    }

  if(gear_index == 0)
    return;

  scaled_vehicle_speed =
    (((state_->parameters->wheel_speed_to_engine_speed_scale >> VEHICLE_FIXED8_SHIFT) *
      (state_->vehicle_speed >> VEHICLE_FIXED8_SHIFT)) >>
     VEHICLE_FIXED8_SHIFT);
  predicted_engine_speed =
    (((state_->parameters->gear_ratios[gear_index + VEHICLE_DOWNSHIFT_GEAR_RATIO_OFFSET] >>
       VEHICLE_FIXED8_SHIFT) *
      scaled_vehicle_speed) >>
     VEHICLE_FIXED16_SHIFT);
  downshift_threshold =
    ((parameters_->gear_shift_thresholds[gear_index + VEHICLE_DOWNSHIFT_THRESHOLD_OFFSET] *
      VEHICLE_DOWNSHIFT_THRESHOLD_NUMERATOR) /
     VEHICLE_DOWNSHIFT_THRESHOLD_DIVISOR);
  if(downshift_threshold <= predicted_engine_speed)
    return;

  state_->target_engine_speed = predicted_engine_speed;
  state_->gear_control_index--;
}

void
update_vehicle_automatic_transmission(VehicleParameterState *state_)
{
  int32 previous_gear_index;
  int32 shift_delay;

  if(state_->gear_shift_delay > 0)
    state_->gear_shift_delay--;

  if(state_->gear_shift_delay == 0)
    {
      state_->primary_control_value = state_->deferred_primary_control_value;
      state_->gear_shift_delay      = -1;
      state_->gear_control_ready    = 1;
      state_->gear_control_pending  = 0;
    }
  else if(state_->gear_shift_delay >= 0)
    {
      return;
    }

  previous_gear_index = state_->gear_control_index;
  adjust_vehicle_automatic_gear(state_, state_->parameters);
  if(previous_gear_index == state_->gear_control_index)
    return;

  state_->deferred_primary_control_value = state_->primary_control_value;
  state_->gear_control_pending           = 1;
  state_->primary_control_value          = 0;
  state_->gear_control_ready             = 0;
  shift_delay                            = state_->parameters->gear_shift_delay;
  if((g_PLAYER_VEHICLE_INDEX == VEHICLE_SPECIAL_SHIFT_VEHICLE_INDEX) &&
     ((G_GAME_FRAME_COUNTER & VEHICLE_SPECIAL_SHIFT_FRAME_MASK) ==
      VEHICLE_SPECIAL_SHIFT_FRAME_PHASE))
    {
      shift_delay += VEHICLE_SPECIAL_SHIFT_DELAY_FRAMES;
    }

  state_->gear_shift_delay = shift_delay;
}

void
adjust_vehicle_gear_index(VehicleParameterState *state_, int32 delta_)
{
  int32 gear_index;

  gear_index = (state_->gear_control_index + delta_);
  if(gear_index == state_->parameters->gear_count)
    gear_index = (state_->parameters->gear_count - 1);
  if(gear_index < 0)
    gear_index = 0;

  state_->gear_control_index = gear_index;
}

/* Preserve the oracle's one-pass signed fixed-point update and clamp order. */
void
update_vehicle_engine_speed(VehicleParameterState *state_)
{
  const VehicleParameters *parameters;
  int32                    target_engine_speed;
  int32                    control_engine_speed;
  int32                    engine_speed_adjustment;

  parameters = state_->parameters;
  if(state_->gear_control_pending != 0)
    state_->primary_control_value = 0;

  target_engine_speed = (((parameters->wheel_speed_to_engine_speed_scale >> VEHICLE_FIXED8_SHIFT) *
                          (state_->driven_wheel_speed >> VEHICLE_FIXED8_SHIFT)) >>
                         VEHICLE_FIXED8_SHIFT);
  target_engine_speed =
    (((parameters
         ->gear_ratios[state_->gear_control_index + VEHICLE_ENGINE_SPEED_GEAR_RATIO_OFFSET] >>
       VEHICLE_FIXED8_SHIFT) *
      target_engine_speed) >>
     VEHICLE_FIXED16_SHIFT);
  if(target_engine_speed < VEHICLE_MINIMUM_TARGET_ENGINE_SPEED)
    target_engine_speed = VEHICLE_MINIMUM_TARGET_ENGINE_SPEED;
  state_->target_engine_speed = target_engine_speed;

  control_engine_speed =
    ((state_->primary_control_value * parameters->maximum_engine_speed) >> VEHICLE_FIXED8_SHIFT);
  if(state_->gear_control_ready == 0)
    {
      if(state_->primary_control_value < VEHICLE_IDLE_CONTROL_THRESHOLD)
        {
          engine_speed_adjustment = parameters->disengaged_engine_speed_adjustment;
          if(state_->gear_control_index == VEHICLE_NEUTRAL_GEAR_INDEX)
            engine_speed_adjustment /= VEHICLE_NEUTRAL_ENGINE_ADJUST_DIVISOR;

          if(state_->engine_speed > parameters->minimum_engine_speed)
            {
              state_->engine_speed -= engine_speed_adjustment;
              if(state_->engine_speed < parameters->minimum_engine_speed)
                state_->engine_speed = parameters->minimum_engine_speed;
            }
          else
            {
              state_->engine_speed += engine_speed_adjustment;
              if(state_->engine_speed > parameters->minimum_engine_speed)
                state_->engine_speed = parameters->minimum_engine_speed;
            }
        }
      else
        {
          engine_speed_adjustment =
            ((parameters->disengaged_engine_speed_acceleration * state_->primary_control_value) >>
             VEHICLE_FIXED8_SHIFT);
          if(state_->gear_control_index == VEHICLE_NEUTRAL_GEAR_INDEX)
            engine_speed_adjustment /= VEHICLE_NEUTRAL_ENGINE_ADJUST_DIVISOR;

          state_->engine_speed += engine_speed_adjustment;
          if(state_->engine_speed > control_engine_speed)
            state_->engine_speed = control_engine_speed;
        }
    }
  else if(state_->engine_speed > state_->target_engine_speed)
    {
      engine_speed_adjustment = parameters->engaged_engine_speed_deceleration;
      if(((state_->driven_wheel_state_flags & VEHICLE_DRIVEN_WHEEL_SKID_FLAG) != 0) &&
         (state_->primary_control_value > VEHICLE_SKID_ENGINE_CONTROL_THRESHOLD))
        {
          engine_speed_adjustment /= VEHICLE_SKID_ENGINE_DECAY_DIVISOR;
        }

      state_->engine_speed -= engine_speed_adjustment;
      if(state_->engine_speed < state_->target_engine_speed)
        state_->engine_speed = state_->target_engine_speed;
    }
  else
    {
      state_->engine_speed += parameters->engaged_engine_speed_acceleration;
      if(state_->engine_speed > state_->target_engine_speed)
        state_->engine_speed = state_->target_engine_speed;
    }

  if((state_->gear_control_stage != 0) && (state_->gear_control_index == 0) &&
     (state_->engine_speed < parameters->minimum_engine_speed))
    {
      state_->engine_speed = parameters->minimum_engine_speed;
    }
  if(state_->engine_speed > VEHICLE_ENGINE_SPEED_LIMIT)
    state_->engine_speed = VEHICLE_ENGINE_SPEED_LIMIT;
  if(state_->target_engine_speed > VEHICLE_ENGINE_SPEED_LIMIT)
    state_->target_engine_speed = VEHICLE_ENGINE_SPEED_LIMIT;
}

/* Keep the oracle's coupled force, slip, and wheel-speed update order intact. */
int32
calculate_vehicle_drive_force(VehicleParameterState *state_)
{
  const VehicleParameters   *parameters;
  const DistanceSampleTable *torque_table;
  int32                      engine_speed;
  int32                      control_engine_speed;
  int32                      drive_force;
  int32                      force_magnitude;
  int32                      gear_ratio;
  int32                      speed_magnitude;
  int32                      wheel_speed;
  int32                      wheel_speed_difference;
  int32                      wheel_speed_divisor;

  parameters   = state_->parameters;
  torque_table = (const DistanceSampleTable *) parameters;
  drive_force  = 0;
  if(state_->gear_control_ready != 0)
    {
      engine_speed = state_->engine_speed;
      if(engine_speed < state_->target_engine_speed)
        {
          drive_force =
            ((state_->target_engine_speed - engine_speed) * -VEHICLE_ENGINE_BRAKING_SCALE);
          drive_force =
            ((parameters->drivetrain_force_scale * drive_force) >> VEHICLE_FIXED8_SHIFT);
          gear_ratio =
            (parameters
               ->gear_ratios[state_->gear_control_index + VEHICLE_ENGINE_SPEED_GEAR_RATIO_OFFSET] >>
             VEHICLE_FIXED8_SHIFT);
          drive_force = (gear_ratio * drive_force);
        }
      else
        {
          if((state_->gear_control_stage != 0) && (state_->gear_control_index == 0) &&
             (engine_speed < parameters->minimum_engine_speed))
            {
              engine_speed = state_->target_engine_speed;
            }

          control_engine_speed =
            ((state_->primary_control_value * parameters->maximum_engine_speed) >>
             VEHICLE_FIXED8_SHIFT);
          if(engine_speed > control_engine_speed)
            {
              force_magnitude =
                (parameters->drivetrain_force_scale * (engine_speed - control_engine_speed));
              if(force_magnitude <= 0)
                force_magnitude = -force_magnitude;
              force_magnitude >>= VEHICLE_FIXED8_SHIFT;
              gear_ratio  = (parameters->gear_ratios[state_->gear_control_index +
                                                     VEHICLE_ENGINE_SPEED_GEAR_RATIO_OFFSET] >>
                             VEHICLE_FIXED8_SHIFT);
              drive_force = (gear_ratio * force_magnitude);
              if(drive_force <= 0)
                drive_force = -drive_force;

              speed_magnitude = state_->vehicle_speed;
              if(speed_magnitude <= 0)
                speed_magnitude = -speed_magnitude;
              if(drive_force > (speed_magnitude * VEHICLE_DRIVE_FORCE_SPEED_SCALE))
                {
                  drive_force = -(state_->vehicle_speed * VEHICLE_DRIVE_FORCE_SPEED_SCALE);
                }
              else if(state_->vehicle_speed > 0)
                {
                  drive_force = -drive_force;
                }
              else if(state_->vehicle_speed == 0)
                {
                  drive_force = 0;
                }
            }
          else
            {
              force_magnitude = get_distance_sample_value(torque_table, engine_speed);
              gear_ratio      = (parameters->gear_ratios[state_->gear_control_index +
                                                         VEHICLE_ENGINE_SPEED_GEAR_RATIO_OFFSET] >>
                                 VEHICLE_FIXED8_SHIFT);
              force_magnitude = (gear_ratio * (force_magnitude >> VEHICLE_FIXED8_SHIFT));
              if(force_magnitude > VEHICLE_DRIVE_FORCE_LIMIT)
                force_magnitude = VEHICLE_DRIVE_FORCE_LIMIT;

              wheel_speed_divisor =
                (((parameters->wheel_speed_to_engine_speed_scale >> VEHICLE_FIXED8_SHIFT) *
                  gear_ratio) >>
                 VEHICLE_FIXED16_SHIFT);
              wheel_speed =
                ((int32) ((uint32) engine_speed << VEHICLE_FIXED16_SHIFT) / wheel_speed_divisor);
              wheel_speed_difference = (wheel_speed - state_->vehicle_speed);
              speed_magnitude        = wheel_speed_difference;
              if(speed_magnitude <= 0)
                speed_magnitude = -speed_magnitude;
              if((speed_magnitude > VEHICLE_WHEEL_SPEED_DIFFERENCE_LIMIT) &&
                 (state_->primary_control_value > VEHICLE_WHEEL_COUPLING_CONTROL_LIMIT) &&
                 (state_->gear_control_index < VEHICLE_WHEEL_SPIN_GEAR_LIMIT))
                {
                  drive_force = wheel_speed_difference;
                  if(state_->primary_control_value > VEHICLE_WHEEL_SPIN_CONTROL_LIMIT)
                    {
                      g_DRIVEN_WHEEL_SPIN_ACTIVE = 1;
                    }
                }
              else
                {
                  drive_force =
                    ((state_->primary_control_value * force_magnitude) >> VEHICLE_FIXED8_SHIFT);
                }
            }
        }
    }

  force_magnitude = drive_force;
  if(force_magnitude <= 0)
    force_magnitude = -force_magnitude;
  if((state_->primary_control_value > VEHICLE_WHEEL_COUPLING_CONTROL_LIMIT) &&
     (force_magnitude > (state_->rear_tire_force_limit - state_->driven_wheel_slip_adjustment)) &&
     (state_->engine_speed <
      (parameters->gear_shift_thresholds[VEHICLE_WHEEL_COUPLING_THRESHOLD_INDEX] -
       VEHICLE_WHEEL_COUPLING_SPEED_MARGIN)) &&
     (parameters->front_drive_ratio == 0))
    {
      gear_ratio =
        (parameters
           ->gear_ratios[state_->gear_control_index + VEHICLE_ENGINE_SPEED_GEAR_RATIO_OFFSET] >>
         VEHICLE_FIXED8_SHIFT);
      wheel_speed_divisor =
        (((parameters->wheel_speed_to_engine_speed_scale >> VEHICLE_FIXED8_SHIFT) * gear_ratio) >>
         VEHICLE_FIXED16_SHIFT);
      state_->driven_wheel_speed =
        ((int32) ((uint32) state_->engine_speed << VEHICLE_FIXED16_SHIFT) / wheel_speed_divisor);
      if((state_->vehicle_speed > 0) && (state_->driven_wheel_speed < state_->vehicle_speed))
        {
          state_->driven_wheel_speed = state_->vehicle_speed;
        }
      else if((state_->vehicle_speed <= 0) && (state_->driven_wheel_speed > state_->vehicle_speed))
        {
          state_->driven_wheel_speed = state_->vehicle_speed;
        }

      return drive_force;
    }

  state_->driven_wheel_speed = state_->vehicle_speed;
  return drive_force;
}

int32
calculate_vehicle_surface_resistance(VehicleParameterState *state_, int32 speed_)
{
  const VehicleSurfacePhysicsEntry *surface;
  int32                             resistance;
  int32                             resistance_scale;
  int32                             speed_force_limit;
  int32                             speed_squared;
  int32                             surface_index;

  if(g_VEHICLE_SURFACE_INDEX == 0)
    {
      g_VEHICLE_SURFACE_SOUND_MODE = 0;
    }
  else
    {
      g_VEHICLE_SURFACE_SOUND_MODE = VEHICLE_SURFACE_SOUND_OFF_ROAD;
      g_VEHICLE_GEAR_SHIFT_STATISTICS->off_surface_frame_count++;
    }

  speed_squared =
    (((speed_ >> VEHICLE_SURFACE_FIXED_SHIFT) * (speed_ >> VEHICLE_SURFACE_FIXED_SHIFT)) >>
     VEHICLE_SPEED_SQUARED_SHIFT);
  surface_index = g_VEHICLE_SURFACE_INDEX;
  if(g_VEHICLE_ALTERNATE_SURFACE_BANK_ACTIVE != 0)
    surface_index += g_VEHICLE_SURFACE_BANK_OFFSET;
  surface = &g_VEHICLE_SURFACE_PHYSICS[surface_index];

  resistance_scale =
    (state_->parameters->surface_resistance_scale * surface->speed_squared_resistance_scale);
  if(resistance_scale < 0)
    resistance_scale += VEHICLE_RESISTANCE_ROUNDING_BIAS;
  resistance_scale >>= VEHICLE_SURFACE_FIXED_SHIFT;
  resistance = (resistance_scale * speed_squared);
  if(g_VEHICLE_RESISTANCE_REDUCTION_ENABLED != 0)
    {
      resistance = ((resistance >> VEHICLE_RESISTANCE_HALF_SHIFT) +
                    (resistance >> VEHICLE_RESISTANCE_QUARTER_SHIFT));
    }
  resistance += surface->rolling_resistance;

  speed_force_limit = (speed_ * VEHICLE_RESISTANCE_SPEED_MULTIPLIER);
  if(speed_force_limit < 1)
    speed_force_limit = (speed_ * -VEHICLE_RESISTANCE_SPEED_MULTIPLIER);
  if(resistance > speed_force_limit)
    resistance = speed_force_limit;
  if(speed_ >= 0)
    resistance = -resistance;

  return resistance;
}

void
warn_if_vehicle_angle_out_of_range(int32 angle_, const char *name_)
{
  if((angle_ >= 0) && (angle_ < VEHICLE_ANGLE_REVOLUTION_LIMIT))
    return;

  debug_log(VEHICLE_ANGLE_WARNING_FORMAT, name_, angle_);
}

int32
initialize_vehicle_surface_physics(void)
{
  VehicleSurfacePhysicsEntry *surface;

  surface                     = &g_VEHICLE_SURFACE_PHYSICS[VEHICLE_SURFACE_BASE_ROAD_INDEX];
  surface->lateral_grip_scale = VEHICLE_SURFACE_ROAD_GRIP_SCALE;
  surface->speed_squared_resistance_scale = VEHICLE_SURFACE_ROAD_RESISTANCE_SCALE;
  surface->rolling_resistance             = VEHICLE_SURFACE_ROAD_ROLLING_RESISTANCE;

  surface                     = &g_VEHICLE_SURFACE_PHYSICS[VEHICLE_SURFACE_BASE_LOOSE_FIRST_INDEX];
  surface->lateral_grip_scale = VEHICLE_SURFACE_BASE_LOOSE_GRIP_SCALE;
  surface->speed_squared_resistance_scale = VEHICLE_SURFACE_BASE_LOOSE_DRAG_SCALE;
  surface->rolling_resistance             = VEHICLE_SURFACE_BASE_LOOSE_RESISTANCE;

  surface                     = &g_VEHICLE_SURFACE_PHYSICS[VEHICLE_SURFACE_BASE_LOOSE_SECOND_INDEX];
  surface->lateral_grip_scale = VEHICLE_SURFACE_BASE_LOOSE_GRIP_SCALE;
  surface->speed_squared_resistance_scale = VEHICLE_SURFACE_BASE_LOOSE_DRAG_SCALE;
  surface->rolling_resistance             = VEHICLE_SURFACE_BASE_LOOSE_RESISTANCE;

  surface                     = &g_VEHICLE_SURFACE_PHYSICS[VEHICLE_SURFACE_ALT_ROAD_INDEX];
  surface->lateral_grip_scale = VEHICLE_SURFACE_ROAD_GRIP_SCALE;
  surface->speed_squared_resistance_scale = VEHICLE_SURFACE_ROAD_RESISTANCE_SCALE;
  surface->rolling_resistance             = VEHICLE_SURFACE_ROAD_ROLLING_RESISTANCE;

  surface                     = &g_VEHICLE_SURFACE_PHYSICS[VEHICLE_SURFACE_ALT_LOOSE_FIRST_INDEX];
  surface->lateral_grip_scale = VEHICLE_SURFACE_ALT_LOOSE_GRIP_SCALE;
  surface->speed_squared_resistance_scale = VEHICLE_SURFACE_ALT_LOOSE_DRAG_SCALE;
  surface->rolling_resistance             = VEHICLE_SURFACE_ALT_LOOSE_RESISTANCE;

  surface                     = &g_VEHICLE_SURFACE_PHYSICS[VEHICLE_SURFACE_ALT_LOOSE_SECOND_INDEX];
  surface->lateral_grip_scale = VEHICLE_SURFACE_ALT_LOOSE_GRIP_SCALE;
  surface->speed_squared_resistance_scale = VEHICLE_SURFACE_ALT_LOOSE_DRAG_SCALE;
  surface->rolling_resistance             = VEHICLE_SURFACE_ALT_LOOSE_RESISTANCE;

  if((g_OPPONENT_VEHICLE_INDEX == VEHICLE_SIMULATION_DISABLED_MODE) &&
     (g_VEHICLE_SPECIAL_SURFACE_TUNING_ENABLED != 0))
    {
      surface                     = &g_VEHICLE_SURFACE_PHYSICS[VEHICLE_SURFACE_ALT_ROAD_INDEX];
      surface->lateral_grip_scale = VEHICLE_SURFACE_SPECIAL_ROAD_SCALE;
      surface->speed_squared_resistance_scale = VEHICLE_SURFACE_SPECIAL_ROAD_SCALE;
      surface->rolling_resistance             = VEHICLE_SURFACE_ALT_LOOSE_RESISTANCE;
    }

  return 1;
}

/* Keep this oracle unit intact because its force clamps share sign and reduction state. */
void
limit_vehicle_tire_forces(VehicleParameterState *state_,
                          int32                 *lateral_force_,
                          int32                 *longitudinal_force_,
                          int32                  longitudinal_force_limit_,
                          uint32                *slip_flags_,
                          int32                  lateral_force_scale_)
{
  int32 bounded_combined_force;
  int32 combined_force;
  int32 combined_force_limit;
  int32 force_scale;
  int32 lateral_force;
  int32 lateral_force_limit;
  int32 lateral_force_magnitude;
  int32 longitudinal_force;
  int32 longitudinal_force_magnitude;
  int32 longitudinal_force_reduction;
  int32 overloaded_force_limit;
  int32 remaining_force;

  longitudinal_force_reduction = 0;
  lateral_force_limit          = (lateral_force_scale_ * longitudinal_force_limit_);
  if(lateral_force_limit < 0)
    lateral_force_limit += VEHICLE_RESISTANCE_ROUNDING_BIAS;
  lateral_force_limit >>= VEHICLE_FIXED8_SHIFT;
  combined_force_limit = (longitudinal_force_limit_ + lateral_force_limit);

  longitudinal_force      = *longitudinal_force_;
  lateral_force           = *lateral_force_;
  lateral_force_magnitude = lateral_force;
  if(lateral_force_magnitude <= 0)
    lateral_force_magnitude = -lateral_force_magnitude;
  longitudinal_force_magnitude = longitudinal_force;
  if(longitudinal_force_magnitude <= 0)
    longitudinal_force_magnitude = -longitudinal_force_magnitude;
  combined_force = (lateral_force_magnitude + longitudinal_force_magnitude);

  if((g_VEHICLE_PRIMARY_FORCE_CONTROL_ACTIVE != 0) ||
     (g_VEHICLE_SECONDARY_FORCE_CONTROL_ACTIVE != 0))
    {
      if(longitudinal_force_magnitude > longitudinal_force_limit_)
        {
          if(longitudinal_force <= 0)
            {
              if(g_VEHICLE_SECONDARY_FORCE_CONTROL_ACTIVE == 0)
                {
                  *longitudinal_force_         = -longitudinal_force_limit_;
                  longitudinal_force_magnitude = (longitudinal_force_limit_ - 1);
                }
              else
                {
                  *longitudinal_force_ =
                    -(longitudinal_force_limit_ / VEHICLE_TIRE_FORCE_HALF_DIVISOR);
                  longitudinal_force_magnitude =
                    (longitudinal_force_limit_ / VEHICLE_TIRE_FORCE_HALF_DIVISOR);
                }
            }
          else
            {
              *longitudinal_force_ = longitudinal_force_limit_;
              if(g_VEHICLE_SECONDARY_FORCE_CONTROL_ACTIVE == 0)
                {
                  longitudinal_force_magnitude = (longitudinal_force_limit_ - 1);
                }
              else
                {
                  longitudinal_force_magnitude =
                    (longitudinal_force_limit_ / VEHICLE_TIRE_FORCE_HALF_DIVISOR);
                }
            }
        }

      if(lateral_force_magnitude > lateral_force_limit)
        {
          if(*lateral_force_ <= 0)
            *lateral_force_ = -lateral_force_limit;
          else
            *lateral_force_ = lateral_force_limit;
          lateral_force_magnitude = (lateral_force_limit - 1);
        }

      combined_force = (lateral_force_magnitude + longitudinal_force_magnitude);
    }

  if(combined_force > combined_force_limit)
    {
      if(state_->primary_control_value > VEHICLE_WHEEL_COUPLING_CONTROL_LIMIT)
        {
          *slip_flags_ = ((uint32) (combined_force - combined_force_limit)
                          << VEHICLE_TIRE_FORCE_SLIP_FLAG_SHIFT);
          if((combined_force / VEHICLE_TIRE_FORCE_HALF_DIVISOR) < (int32) *slip_flags_)
            {
              *slip_flags_ = (uint32) (combined_force / VEHICLE_TIRE_FORCE_HALF_DIVISOR);
            }
        }
      else
        {
          *slip_flags_ |= (uint32) (combined_force - combined_force_limit);
        }

      overloaded_force_limit = (combined_force_limit * VEHICLE_TIRE_FORCE_LIMIT_NUMERATOR);
      if(overloaded_force_limit < 0)
        overloaded_force_limit += ((1 << VEHICLE_TIRE_FORCE_LIMIT_SHIFT) - 1);
      overloaded_force_limit >>= VEHICLE_TIRE_FORCE_LIMIT_SHIFT;
      bounded_combined_force = combined_force;
      if(bounded_combined_force > overloaded_force_limit)
        bounded_combined_force = overloaded_force_limit;
      longitudinal_force_reduction = (bounded_combined_force - combined_force_limit);
      remaining_force              = (combined_force_limit - longitudinal_force_reduction);
      if(combined_force < 0)
        combined_force += VEHICLE_RESISTANCE_ROUNDING_BIAS;
      force_scale = (remaining_force / (combined_force >> VEHICLE_FIXED8_SHIFT));

      lateral_force = (force_scale * *lateral_force_);
      if(lateral_force < 0)
        lateral_force += VEHICLE_RESISTANCE_ROUNDING_BIAS;
      *lateral_force_    = (lateral_force >> VEHICLE_FIXED8_SHIFT);
      longitudinal_force = (force_scale * *longitudinal_force_);
      if(longitudinal_force < 0)
        longitudinal_force += VEHICLE_RESISTANCE_ROUNDING_BIAS;
      *longitudinal_force_ = (longitudinal_force >> VEHICLE_FIXED8_SHIFT);
    }
  else
    {
      if(lateral_force_magnitude > lateral_force_limit)
        {
          *slip_flags_           = (uint32) (lateral_force_magnitude - lateral_force_limit);
          overloaded_force_limit = (lateral_force_limit * VEHICLE_TIRE_FORCE_LIMIT_NUMERATOR);
          if(overloaded_force_limit < 0)
            overloaded_force_limit += ((1 << VEHICLE_TIRE_FORCE_LIMIT_SHIFT) - 1);
          overloaded_force_limit >>= VEHICLE_TIRE_FORCE_LIMIT_SHIFT;
          if(lateral_force_magnitude > overloaded_force_limit)
            lateral_force_magnitude = overloaded_force_limit;
          lateral_force_magnitude = (lateral_force_magnitude - lateral_force_limit);
          lateral_force           = (lateral_force_limit - lateral_force_magnitude);
          if(*lateral_force_ < 0)
            lateral_force = -lateral_force;
          *lateral_force_ = lateral_force;
        }

      if(longitudinal_force_magnitude > longitudinal_force_limit_)
        {
          *slip_flags_ |= ((uint32) (longitudinal_force_magnitude - longitudinal_force_limit_)
                           << VEHICLE_TIRE_FORCE_SLIP_FLAG_SHIFT);
          overloaded_force_limit = (longitudinal_force_limit_ * VEHICLE_TIRE_FORCE_LIMIT_NUMERATOR);
          if(overloaded_force_limit < 0)
            overloaded_force_limit += ((1 << VEHICLE_TIRE_FORCE_LIMIT_SHIFT) - 1);
          overloaded_force_limit >>= VEHICLE_TIRE_FORCE_LIMIT_SHIFT;
          if(longitudinal_force_magnitude > overloaded_force_limit)
            longitudinal_force_magnitude = overloaded_force_limit;
          longitudinal_force_reduction = (longitudinal_force_magnitude - longitudinal_force_limit_);
          longitudinal_force           = (longitudinal_force_limit_ - longitudinal_force_reduction);
          if(*longitudinal_force_ < 0)
            longitudinal_force = -longitudinal_force;
          *longitudinal_force_ = longitudinal_force;
        }
    }

  if(slip_flags_ != &state_->driven_wheel_slip_flags)
    return;
  longitudinal_force = *longitudinal_force_;
  if(((longitudinal_force > 0) && (state_->vehicle_speed > 0)) ||
     ((longitudinal_force < 0) && (state_->vehicle_speed < 0)))
    {
      state_->driven_wheel_slip_adjustment = longitudinal_force_reduction;
    }
}

/* Scales caller-owned force components and writes slip excess when overloaded. */
void
limit_vehicle_tire_force_vector(VehicleParameterState *state_,
                                int32                 *lateral_force_,
                                int32                 *longitudinal_force_,
                                int32                  force_limit_,
                                uint32                *slip_amount_,
                                int32                  demanded_force_)
{
  int32 demanded_force_limit;
  int32 force_magnitude;
  int32 force_scale;
  int32 lateral_force;
  int32 lateral_force_magnitude;
  int32 longitudinal_force;
  int32 longitudinal_force_magnitude;
  int32 remaining_force;

  (void) state_;
  lateral_force_magnitude = *lateral_force_;
  if(lateral_force_magnitude <= 0)
    lateral_force_magnitude = -lateral_force_magnitude;
  longitudinal_force_magnitude = *longitudinal_force_;
  if(longitudinal_force_magnitude <= 0)
    longitudinal_force_magnitude = -longitudinal_force_magnitude;
  if(lateral_force_magnitude > longitudinal_force_magnitude)
    {
      force_magnitude = (lateral_force_magnitude +
                         (longitudinal_force_magnitude >> VEHICLE_TIRE_FORCE_VECTOR_SHIFT));
    }
  else
    {
      force_magnitude = (longitudinal_force_magnitude +
                         (lateral_force_magnitude >> VEHICLE_TIRE_FORCE_VECTOR_SHIFT));
    }
  if(force_magnitude <= force_limit_)
    return;

  *slip_amount_        = (uint32) (force_magnitude - force_limit_);
  demanded_force_limit = (force_limit_ * VEHICLE_TIRE_FORCE_DEMAND_LIMIT_NUMERATOR);
  if(demanded_force_limit < 0)
    {
      demanded_force_limit += ((1 << VEHICLE_TIRE_FORCE_VECTOR_SHIFT) - 1);
    }
  demanded_force_limit >>= VEHICLE_TIRE_FORCE_VECTOR_SHIFT;
  if(demanded_force_ > demanded_force_limit)
    demanded_force_ = demanded_force_limit;
  remaining_force = ((force_limit_ * VEHICLE_TIRE_FORCE_HALF_DIVISOR) - demanded_force_);
  if(force_magnitude < 0)
    force_magnitude += VEHICLE_RESISTANCE_ROUNDING_BIAS;
  force_scale = (remaining_force / (force_magnitude >> VEHICLE_FIXED8_SHIFT));

  lateral_force = (force_scale * *lateral_force_);
  if(lateral_force < 0)
    lateral_force += VEHICLE_RESISTANCE_ROUNDING_BIAS;
  *lateral_force_    = (lateral_force >> VEHICLE_FIXED8_SHIFT);
  longitudinal_force = (force_scale * *longitudinal_force_);
  if(longitudinal_force < 0)
    longitudinal_force += VEHICLE_RESISTANCE_ROUNDING_BIAS;
  *longitudinal_force_ = (longitudinal_force >> VEHICLE_FIXED8_SHIFT);
}

/* Keep the oracle's coupled axle demand, slip, and force-limit order intact. */
void
calculate_vehicle_axle_tire_forces(VehicleParameterState *state_,
                                   int32                 *lateral_force_,
                                   int32                 *longitudinal_force_,
                                   int32                  demanded_lateral_force_,
                                   int32                  demanded_longitudinal_force_,
                                   int32                  steering_angle_,
                                   int32                  force_limit_,
                                   int32                  applied_longitudinal_force_,
                                   int32                  axle_index_)
{
  int32   applied_force_magnitude;
  int32   combined_force_limit;
  int32   demanded_force_magnitude;
  int32   force_limit;
  int32   force_magnitude;
  int32   lateral_force;
  int32   lateral_force_limit;
  int32   lateral_force_magnitude;
  int32   lateral_force_scale;
  uint32  lateral_slip_amount;
  int32   longitudinal_force;
  int32   longitudinal_force_magnitude;
  int32   longitudinal_force_scale;
  int32   original_lateral_force;
  int32   original_longitudinal_force;
  uint32 *slip_amount;
  int32   tire_force_scale;
  uint8  *tire_state_flags;

  lateral_slip_amount = 0;
  if(state_->tire_force_calculation_enabled == 0)
    {
      *lateral_force_      = 0;
      *longitudinal_force_ = 0;
      return;
    }

  tire_state_flags = &state_->undriven_wheel_state_flags;
  if(axle_index_ == VEHICLE_FRONT_AXLE_INDEX)
    {
      rotate_coordinates_fixed16(demanded_lateral_force_,
                                 demanded_longitudinal_force_,
                                 -steering_angle_,
                                 &lateral_force,
                                 &longitudinal_force);
      force_limit              = state_->front_tire_force_limit;
      slip_amount              = &state_->undriven_wheel_slip_flags;
      lateral_force_scale      = state_->parameters->front_lateral_tire_force_scale;
      longitudinal_force_scale = state_->parameters->front_longitudinal_tire_force_scale;
    }
  else
    {
      lateral_force            = demanded_lateral_force_;
      longitudinal_force       = demanded_longitudinal_force_;
      force_limit              = state_->rear_tire_force_limit;
      tire_state_flags         = &state_->driven_wheel_state_flags;
      slip_amount              = &state_->driven_wheel_slip_flags;
      lateral_force_scale      = state_->parameters->rear_lateral_tire_force_scale;
      longitudinal_force_scale = state_->parameters->rear_longitudinal_tire_force_scale;
    }

  *tire_state_flags           = 0;
  original_lateral_force      = lateral_force;
  original_longitudinal_force = longitudinal_force;
  lateral_force_magnitude     = lateral_force;
  if(lateral_force_magnitude <= 0)
    lateral_force_magnitude = -lateral_force_magnitude;
  longitudinal_force_magnitude = longitudinal_force;
  if(longitudinal_force_magnitude <= 0)
    longitudinal_force_magnitude = -longitudinal_force_magnitude;
  demanded_force_magnitude = Atan2F16(longitudinal_force_magnitude, lateral_force_magnitude);
  if(demanded_force_magnitude > state_->parameters->tire_slip_angle_limit)
    {
      demanded_force_magnitude = state_->parameters->tire_slip_angle_limit;
      *tire_state_flags |= VEHICLE_TIRE_STATE_SLIP_ANGLE_FLAG;
    }

  applied_force_magnitude = applied_longitudinal_force_;
  if(applied_force_magnitude <= 0)
    applied_force_magnitude = -applied_force_magnitude;
  if((applied_force_magnitude > force_limit) && (g_VEHICLE_PRIMARY_FORCE_CONTROL_ACTIVE == 0))
    {
      if(longitudinal_force_magnitude > force_limit)
        *tire_state_flags |= VEHICLE_TIRE_STATE_SLIP_ANGLE_FLAG;
      limit_vehicle_tire_force_vector(state_,
                                      &lateral_force,
                                      &longitudinal_force,
                                      force_limit,
                                      slip_amount,
                                      applied_longitudinal_force_);
    }
  else
    {
      tire_force_scale = lookup_vehicle_tire_force_scale(state_,
                                                         demanded_force_magnitude,
                                                         axle_index_ - VEHICLE_FRONT_AXLE_INDEX);
      combined_force_limit = lateral_force_scale * force_limit;
      if(combined_force_limit < 0)
        combined_force_limit += ((1 << VEHICLE_TIRE_FORCE_SCALE_SHIFT) - 1);
      lateral_force_limit =
        MulSF16(tire_force_scale, combined_force_limit >> VEHICLE_TIRE_FORCE_SCALE_SHIFT);
      if(lateral_force_limit <= 0)
        lateral_force_limit = -lateral_force_limit;

      lateral_force_magnitude = lateral_force;
      if(lateral_force_magnitude <= 0)
        lateral_force_magnitude = -lateral_force_magnitude;
      if(lateral_force_magnitude > lateral_force_limit)
        {
          force_magnitude = lateral_force_limit * VEHICLE_TIRE_LATERAL_DEMAND_MULTIPLIER;
          if((lateral_force_magnitude > force_magnitude) &&
             (demanded_force_magnitude > VEHICLE_TIRE_LATERAL_SLIP_ANGLE_LIMIT))
            {
              lateral_slip_amount = (uint32) (lateral_force_magnitude - force_magnitude);
              *tire_state_flags |= VEHICLE_TIRE_STATE_LATERAL_SLIP_FLAG;
            }
          if(lateral_force <= 0)
            lateral_force = -lateral_force_limit;
          else
            lateral_force = lateral_force_limit;
        }

      if(longitudinal_force <= 0)
        applied_longitudinal_force_ = -applied_longitudinal_force_;
      longitudinal_force = force_limit_ + applied_longitudinal_force_;
      force_magnitude    = force_limit_;
      if(force_magnitude <= 0)
        force_magnitude = -force_magnitude;
      applied_force_magnitude = applied_longitudinal_force_;
      if(applied_force_magnitude <= 0)
        applied_force_magnitude = -applied_force_magnitude;

      if(applied_force_magnitude < force_magnitude)
        {
          longitudinal_force_magnitude = longitudinal_force;
          if(longitudinal_force_magnitude <= 0)
            longitudinal_force_magnitude = -longitudinal_force_magnitude;
          combined_force_limit = longitudinal_force_scale * lateral_force;
          if(combined_force_limit < 0)
            {
              combined_force_limit += ((1 << VEHICLE_TIRE_FORCE_SCALE_SHIFT) - 1);
            }
          combined_force_limit >>= VEHICLE_TIRE_FORCE_SCALE_SHIFT;
          if(combined_force_limit <= 0)
            combined_force_limit = -combined_force_limit;

          if((longitudinal_force_magnitude + combined_force_limit) > force_limit)
            {
              *tire_state_flags = VEHICLE_TIRE_STATE_LONGITUDINAL_SLIP_FLAG;
              if(longitudinal_force_magnitude > force_limit)
                {
                  if((g_DRIVEN_WHEEL_SPIN_ACTIVE == 0) || (g_TRACTION_CONTROL_ENABLED != 0))
                    {
                      combined_force_limit =
                        (force_limit * VEHICLE_TIRE_LONGITUDINAL_LIMIT_NUMERATOR) /
                        VEHICLE_TIRE_LONGITUDINAL_LIMIT_DIVISOR;
                      if(longitudinal_force_magnitude > combined_force_limit)
                        longitudinal_force_magnitude = combined_force_limit;
                      lateral_force =
                        (lateral_force * ((force_limit * VEHICLE_TIRE_AVAILABLE_FORCE_MULTIPLIER -
                                           longitudinal_force_magnitude) >>
                                          VEHICLE_TIRE_FORCE_SCALE_SHIFT)) /
                        (force_limit >> VEHICLE_TIRE_FORCE_SCALE_SHIFT);
                    }
                  else
                    {
                      if(lateral_force < 0)
                        lateral_force += (VEHICLE_TIRE_FORCE_REDUCTION_DIVISOR - 1);
                      lateral_force /= VEHICLE_TIRE_FORCE_REDUCTION_DIVISOR;
                      force_magnitude = state_->vehicle_speed;
                      if(force_magnitude <= 0)
                        force_magnitude = -force_magnitude;
                      if((force_magnitude <= VEHICLE_TIRE_FORCE_CONTROL_SPEED_LIMIT) &&
                         (state_->engine_speed > VEHICLE_TIRE_FORCE_CONTROL_ENGINE_LIMIT))
                        {
                          force_magnitude = state_->control_event_value;
                          if(force_magnitude <= 0)
                            force_magnitude = -force_magnitude;
                          if((force_magnitude > VEHICLE_TIRE_FORCE_CONTROL_ANGLE_LIMIT) &&
                             (state_->primary_control_value >
                              VEHICLE_TIRE_FORCE_CONTROL_INPUT_LIMIT) &&
                             (state_->gear_control_index == 0))
                            {
                              lateral_force = longitudinal_force_magnitude;
                              if(state_->control_event_value < 0)
                                lateral_force = -lateral_force;
                              if(lateral_force < 0)
                                {
                                  lateral_force +=
                                    ((1 << VEHICLE_TIRE_FORCE_CONTROL_REDUCTION_SHIFT) - 1);
                                }
                              lateral_force >>= VEHICLE_TIRE_FORCE_CONTROL_REDUCTION_SHIFT;
                              if(tire_state_flags == &state_->undriven_wheel_state_flags)
                                {
                                  if(lateral_force < 0)
                                    {
                                      lateral_force +=
                                        (VEHICLE_TIRE_FRONT_FORCE_REDUCTION_DIVISOR - 1);
                                    }
                                  lateral_force /= VEHICLE_TIRE_FRONT_FORCE_REDUCTION_DIVISOR;
                                }
                            }
                        }
                    }
                }
              limit_vehicle_tire_forces(state_,
                                        &lateral_force,
                                        &longitudinal_force,
                                        force_limit,
                                        slip_amount,
                                        lateral_force_scale);
            }
          else
            {
              limit_vehicle_tire_forces(state_,
                                        &lateral_force,
                                        &longitudinal_force,
                                        force_limit,
                                        slip_amount,
                                        lateral_force_scale);
            }
          if((int32) *slip_amount < (int32) lateral_slip_amount)
            *slip_amount = lateral_slip_amount;
        }
      else
        {
          force_magnitude = longitudinal_force;
          if(force_magnitude <= 0)
            force_magnitude = -force_magnitude;
          longitudinal_force_magnitude = original_longitudinal_force;
          if(longitudinal_force_magnitude <= 0)
            longitudinal_force_magnitude = -longitudinal_force_magnitude;
          if(force_magnitude < longitudinal_force_magnitude)
            {
              longitudinal_force = force_magnitude;
              if(original_longitudinal_force <= 0)
                longitudinal_force = -longitudinal_force;
            }

          lateral_force_magnitude = lateral_force;
          if(lateral_force_magnitude <= 0)
            lateral_force_magnitude = -lateral_force_magnitude;
          combined_force_limit = lateral_force_scale * longitudinal_force;
          if(combined_force_limit < 0)
            {
              combined_force_limit += ((1 << VEHICLE_TIRE_FORCE_SCALE_SHIFT) - 1);
            }
          combined_force_limit >>= VEHICLE_TIRE_FORCE_SCALE_SHIFT;
          if(combined_force_limit <= 0)
            combined_force_limit = -combined_force_limit;
          force_magnitude =
            ((lateral_force_scale * force_limit) * VEHICLE_TIRE_COMBINED_LIMIT_NUMERATOR);
          if(force_magnitude < 0)
            force_magnitude += ((1 << VEHICLE_TIRE_COMBINED_LIMIT_SHIFT) - 1);
          force_magnitude >>= VEHICLE_TIRE_COMBINED_LIMIT_SHIFT;
          if((lateral_force_magnitude + combined_force_limit) > force_magnitude)
            {
              if((state_->secondary_control_value < VEHICLE_TIRE_SECONDARY_CONTROL_LIMIT) &&
                 (g_VEHICLE_PRIMARY_FORCE_CONTROL_ACTIVE != 0))
                {
                  limit_vehicle_tire_forces(state_,
                                            &lateral_force,
                                            &longitudinal_force,
                                            force_limit,
                                            slip_amount,
                                            lateral_force_scale);
                }
              else
                {
                  limit_vehicle_tire_forces(state_,
                                            &original_lateral_force,
                                            &original_longitudinal_force,
                                            force_limit,
                                            slip_amount,
                                            lateral_force_scale);
                }
            }
          *slip_amount |= lateral_slip_amount;
        }
    }

  if(steering_angle_ == 0)
    {
      *lateral_force_      = lateral_force;
      *longitudinal_force_ = longitudinal_force;
    }
  else
    {
      rotate_coordinates_fixed16(lateral_force,
                                 longitudinal_force,
                                 steering_angle_,
                                 lateral_force_,
                                 longitudinal_force_);
    }
}

void
initialize_vehicle_dynamics_parameters(VehicleParameterState *state_)
{
  int32 adjusted_tire_coefficient;
  int32 scaled_wheelbase_squared;
  int32 tire_coefficient_adjustment;
  int32 wheelbase_product;
  int32 wheelbase_squared;

  initialize_vehicle_surface_physics();
  tire_coefficient_adjustment =
    MulSF16(state_->parameters->normal_coefficient_loss, state_->parameters->front_axle_mass);
  adjusted_tire_coefficient =
    (state_->parameters->maximum_tire_coefficient +
     (tire_coefficient_adjustment / VEHICLE_TIRE_COEFFICIENT_ADJUSTMENT_DIVISOR));
  state_->parameters->maximum_tire_coefficient = adjusted_tire_coefficient;
  debug_log("Adjusted max tire co = %d", adjusted_tire_coefficient);

  state_->front_mass_fraction =
    MulSF16(state_->parameters->front_axle_mass, state_->parameters->inverse_mass);
  state_->rear_mass_fraction =
    MulSF16(state_->parameters->rear_axle_mass, state_->parameters->inverse_mass);
  state_->mass_to_front_mass_ratio =
    MulSF16(state_->parameters->mass, state_->parameters->inverse_front_axle_mass);
  state_->mass_to_rear_mass_ratio =
    MulSF16(state_->parameters->mass, state_->parameters->inverse_rear_axle_mass);
  state_->parameters->surface_resistance_scale =
    MulSF16(state_->parameters->surface_resistance_scale, state_->parameters->inverse_mass);
  state_->center_of_gravity_to_wheelbase_ratio =
    MulSF16(state_->parameters->center_of_gravity_height, state_->parameters->inverse_wheelbase);

  state_->initial_front_tire_force_limit =
    MulSF16(state_->front_mass_fraction, VEHICLE_GRAVITY_FIXED);
  state_->initial_front_tire_force_limit =
    MulSF16(state_->parameters->front_friction_multiplier, state_->initial_front_tire_force_limit);
  state_->initial_rear_tire_force_limit =
    MulSF16(state_->rear_mass_fraction, VEHICLE_GRAVITY_FIXED);
  state_->initial_rear_tire_force_limit =
    MulSF16(state_->parameters->rear_friction_multiplier, state_->initial_rear_tire_force_limit);
  state_->rear_tire_force_limit  = state_->initial_rear_tire_force_limit;
  state_->front_tire_force_limit = state_->initial_front_tire_force_limit;

  wheelbase_product = MulSF16(state_->rear_mass_fraction, state_->parameters->wheelbase);
  wheelbase_product = MulSF16(wheelbase_product, wheelbase_product);
  (void) MulSF16(wheelbase_product, state_->front_mass_fraction);
  wheelbase_product = MulSF16(state_->front_mass_fraction, state_->parameters->wheelbase);
  wheelbase_product = MulSF16(wheelbase_product, wheelbase_product);
  (void) MulSF16(wheelbase_product, state_->rear_mass_fraction);

  wheelbase_squared        = MulSF16(state_->parameters->wheelbase, state_->parameters->wheelbase);
  scaled_wheelbase_squared = MulSF16(VEHICLE_ANGULAR_UNIT_SCALE_FIXED, wheelbase_squared);
  wheelbase_product        = MulSF16(scaled_wheelbase_squared, state_->parameters->inertia_factor);
  state_->angular_velocity_to_linear_scale =
    DivSF16(scaled_wheelbase_squared, state_->parameters->wheelbase);
  state_->angular_acceleration_scale = DivSF16(wheelbase_product, state_->parameters->wheelbase);
  state_->rear_yaw_force_scale =
    DivSF16(MulSF16(state_->rear_mass_fraction, state_->parameters->wheelbase),
            scaled_wheelbase_squared);
  state_->front_yaw_force_scale =
    DivSF16(MulSF16(state_->front_mass_fraction, state_->parameters->wheelbase),
            scaled_wheelbase_squared);
}
