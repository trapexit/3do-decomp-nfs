#pragma once

#include "distance_sample.h"

#include "types.h"

#define VEHICLE_GEAR_RATIO_COUNT 8
#define VEHICLE_GEAR_SHIFT_THRESHOLD_COUNT 9
#define VEHICLE_ENGINE_TORQUE_SAMPLE_COUNT 59
#define VEHICLE_SURFACE_PHYSICS_ENTRY_COUNT 6
#define VEHICLE_TIRE_FORCE_SCALE_TABLE_COUNT 2

typedef struct VehicleSurfacePhysicsEntry
{
  int32 lateral_grip_scale;
  int32 speed_squared_resistance_scale;
  int32 rolling_resistance;
} VehicleSurfacePhysicsEntry;

typedef struct VehicleParameters
{
  int32          front_axle_mass;
  int32          rear_axle_mass;
  int32          mass;
  int32          inverse_front_axle_mass;
  int32          inverse_rear_axle_mass;
  int32          inverse_mass;
  int32          front_drive_ratio;
  int32          front_brake_ratio;
  int32          rear_brake_ratio;
  int32          center_of_gravity_height;
  int32          auxiliary_brake_force;
  int32          maximum_tire_coefficient;
  int32          surface_resistance_scale;
  int32          reverse_resistance_speed_threshold;
  int32          drivetrain_efficiency;
  int32          wheelbase;
  int32          inverse_wheelbase;
  int32          wheel_track;
  int32          inverse_wheel_track;
  int32          rear_mass_ratio;
  int32          wheel_speed_to_engine_speed_scale;
  int32          gear_count;
  int32          final_drive_ratio;
  int32          rolling_radius;
  int32          inverse_rolling_radius;
  int32          gear_ratios[VEHICLE_GEAR_RATIO_COUNT];
  int32          engine_torque_sample_count;
  int32          front_roll_stiffness;
  int32          rear_roll_stiffness;
  int32          roll_axis_height;
  int32          front_roll_stiffness_ratio;
  int32          rear_roll_stiffness_ratio;
  int32          center_of_gravity_above_roll_axis;
  int32          tire_slip_angle_limit;
  int32          normal_coefficient_loss;
  int32          maximum_engine_speed;
  int32          minimum_engine_speed;
  DistanceSample engine_torque_samples[VEHICLE_ENGINE_TORQUE_SAMPLE_COUNT];
  int32          gear_shift_thresholds[VEHICLE_GEAR_SHIFT_THRESHOLD_COUNT];
  int32          inertia_factor;
  int32          body_roll_scale;
  int32          body_pitch_scale;
  int32          front_friction_multiplier;
  int32          rear_friction_multiplier;
  int32          body_length;
  int32          body_width;
  int32          maximum_automatic_steering_angle;
  int32          automatic_steering_ramp_multiply_shift;
  int32          automatic_steering_ramp_divide_shift;
  int32          automatic_steering_model;
  int32          automatic_steering_speed_thresholds[4];
  int32          automatic_steering_ramp_scale;
  int32          automatic_steering_attenuation;
  int32          alternate_steering_ramp_multiply_shift;
  int32          alternate_steering_ramp_divide_shift;
  int32          lateral_acceleration_limit;
  int32          front_lateral_tire_force_scale;
  int32          front_longitudinal_tire_force_scale;
  int32          rear_lateral_tire_force_scale;
  int32          rear_longitudinal_tire_force_scale;
  int32          burnout_lateral_force_divisor;
  int32          anti_lock_brakes_enabled;
  int32          traction_control_enabled;
  int32          primary_control_rise_limit;
  int32          primary_control_fall_limit;
  int32          secondary_control_low_rise_limit;
  int32          secondary_control_high_rise_limit;
  int32          secondary_control_low_fall_limit;
  int32          secondary_control_high_fall_limit;
  int32          gear_shift_delay;
  int32          disengaged_engine_speed_adjustment;
  int32          disengaged_engine_speed_acceleration;
  int32          engaged_engine_speed_deceleration;
  int32          engaged_engine_speed_acceleration;
  int32          drivetrain_force_scale;
  int32          ride_height;
  int32          visual_center_y;
} VehicleParameters;

typedef struct VehicleParameterState
{
  uint8              reserved_000[0x10];
  int32              heading_angle;
  uint8              reserved_014[4];
  int32              control_event_value;
  uint8              reserved_01_c[0x2C];
  int32              world_lateral_velocity;
  int32              world_vertical_velocity;
  int32              world_longitudinal_velocity;
  int32              track_lateral_velocity;
  int32              track_vertical_velocity;
  int32              vehicle_speed;
  int32              driven_wheel_speed;
  int32              speed_magnitude;
  int32              yaw_velocity;
  uint8              reserved_06_c[0x334];
  int32              body_roll;
  int32              body_pitch;
  uint8              reserved_3_a8[0x44];
  int32              primary_control_value;
  int32              deferred_primary_control_value;
  int32              secondary_control_value;
  int32              gear_shift_delay;
  int32              engine_speed;
  int32              target_engine_speed;
  uint8              reserved_404[4];
  int32              rear_tire_force_limit;
  int32              front_tire_force_limit;
  int32              gear_control_mode;
  int32              lateral_force_offset;
  uint8              reserved_418[4];
  int32              longitudinal_force_offset;
  int32              drive_force;
  int32              gear_control_stage;
  int32              gear_control_index;
  int32              gear_control_ready;
  int32              auxiliary_control_active;
  int32              gear_control_pending;
  int32              tire_force_calculation_enabled;
  uint8              reserved_43_c[8];
  uint32             undriven_wheel_slip_flags;
  uint32             driven_wheel_slip_flags;
  int32              driven_wheel_slip_adjustment;
  int32              combined_lateral_force;
  int32              combined_longitudinal_force;
  int32              gear_sound_current_index;
  int32              gear_sound_previous_index;
  int32              gear_sound_countdown;
  uint8              undriven_wheel_state_flags;
  uint8              driven_wheel_state_flags;
  uint8              reserved_466[0x0E];
  int32              front_mass_fraction;
  int32              rear_mass_fraction;
  int32              mass_to_front_mass_ratio;
  int32              mass_to_rear_mass_ratio;
  int32              center_of_gravity_to_wheelbase_ratio;
  int32              initial_rear_tire_force_limit;
  int32              initial_front_tire_force_limit;
  int32              angular_velocity_to_linear_scale;
  int32              angular_acceleration_scale;
  int32              rear_yaw_force_scale;
  int32              front_yaw_force_scale;
  uint8              reserved_4_a0[4];
  int32              tire_force_overload;
  const uint8       *tire_force_scale_tables[VEHICLE_TIRE_FORCE_SCALE_TABLE_COUNT];
  VehicleParameters *parameters;
} VehicleParameterState;

typedef struct VehicleGearShiftStatistics
{
  int32 reserved_000;
  int32 recovery_frame_duration;
  int32 recovery_track_position;
  int32 maximum_longitudinal_speed;
  uint8 reserved_010[4];
  int32 first_speed_threshold_frame;
  int32 second_speed_threshold_frame;
  uint8 reserved_01_c[0x20];
  int32 secondary_vehicle_collision_count;
  uint8 reserved_040[0x0C];
  int32 longitudinal_slip_frame_count;
  int32 tire_slip_score;
  int32 upshift_engine_speed_total;
  int32 upshift_count;
  int32 downshift_engine_speed_total;
  int32 downshift_count;
  uint8 reserved_064[8];
  int32 high_engine_speed_shift_count;
  int32 off_surface_frame_count;
  uint8 reserved_074[4];
  int32 vehicle_collision_count;
  int32 recovery_count;
  int32 high_yaw_slip_count;
  uint8 reserved_084[0x10];
  int32 profile_time_limit_frames;
  uint8 reserved_098[4];
  int32 first_recovery_track_position;
  int32 latest_recovery_track_position;
} VehicleGearShiftStatistics;

extern VehicleGearShiftStatistics *g_VEHICLE_GEAR_SHIFT_STATISTICS;
extern VehicleParameters          *g_VEHICLE_PARAMETERS;
extern int32                       g_SIMULATION_MODE_INPUT_LATCH;
extern VehicleSurfacePhysicsEntry  g_VEHICLE_SURFACE_PHYSICS[VEHICLE_SURFACE_PHYSICS_ENTRY_COUNT];

int32
load_vehicle_parameter_state(VehicleParameterState *state_, const char *path_);
void
scale_vehicle_engine_torque_curve(VehicleParameters *parameters_);
VehicleParameters *
load_vehicle_parameters_from_file(const char *path_);
void
adjust_vehicle_primary_control_value(VehicleParameterState *state_, int32 target_value_);
void
adjust_vehicle_secondary_control_value(VehicleParameterState *state_, int32 target_value_);
void
record_vehicle_gear_shift(VehicleParameterState *state_, int32 direction_);
void
adjust_vehicle_automatic_gear(VehicleParameterState *state_, const VehicleParameters *parameters_);
void
update_vehicle_automatic_transmission(VehicleParameterState *state_);
void
adjust_vehicle_gear_index(VehicleParameterState *state_, int32 delta_);
void
update_vehicle_engine_speed(VehicleParameterState *state_);
int32
calculate_vehicle_drive_force(VehicleParameterState *state_);
int32
calculate_vehicle_surface_resistance(VehicleParameterState *state_, int32 speed_);
void
warn_if_vehicle_angle_out_of_range(int32 angle_, const char *name_);
int32
initialize_vehicle_surface_physics(void);
int32
lookup_vehicle_tire_force_scale(const VehicleParameterState *state_,
                                int32                        force_magnitude_,
                                int32                        table_index_);
void
limit_vehicle_tire_forces(VehicleParameterState *state_,
                          int32                 *lateral_force_,
                          int32                 *longitudinal_force_,
                          int32                  longitudinal_force_limit_,
                          uint32                *slip_flags_,
                          int32                  lateral_force_scale_);
void
limit_vehicle_tire_force_vector(VehicleParameterState *state_,
                                int32                 *lateral_force_,
                                int32                 *longitudinal_force_,
                                int32                  force_limit_,
                                uint32                *slip_amount_,
                                int32                  demanded_force_);
void
calculate_vehicle_axle_tire_forces(VehicleParameterState *state_,
                                   int32                 *lateral_force_,
                                   int32                 *longitudinal_force_,
                                   int32                  demanded_lateral_force_,
                                   int32                  demanded_longitudinal_force_,
                                   int32                  steering_angle_,
                                   int32                  force_limit_,
                                   int32                  applied_longitudinal_force_,
                                   int32                  axle_index_);
void
initialize_vehicle_dynamics_parameters(VehicleParameterState *state_);
void
adjust_vehicle_gear_state(VehicleParameterState *state_, int32 action_);
