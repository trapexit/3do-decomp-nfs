#include "player_vehicle_dynamics.h"

#include "driving_warning_display.h"
#include "frontend_transition.h"
#include "game_bootstrap.h"
#include "game_frame_count.h"
#include "geometry.h"
#include "simulation_thread_signal.h"
#include "sound_spatial.h"
#include "vehicle_drive_statistics.h"
#include "vehicle_dynamics_state.h"
#include "vehicle_parameter_loader.h"
#include "vehicle_simulation_state.h"
#include "vehicle_track_boundary.h"

#include "operamath.h"

#define PLAYER_DYNAMICS_TIMER_FLAG 4
#define PLAYER_DYNAMICS_FRAME_SCALE 0x888
#define PLAYER_DYNAMICS_FIXED8_SHIFT 8
#define PLAYER_DYNAMICS_APPROXIMATE_DISTANCE_SHIFT 2
#define PLAYER_DYNAMICS_HALF_DIVISOR 2
#define PLAYER_DYNAMICS_FORCE_SAMPLE_MULTIPLIER 30
#define PLAYER_DYNAMICS_BRAKE_FORCE_SCALE 0x14A
#define PLAYER_DYNAMICS_TIRE_FORCE_LIMIT_NUMERATOR 9
#define PLAYER_DYNAMICS_TIRE_FORCE_LIMIT_SHIFT 3
#define PLAYER_DYNAMICS_DIRECTION_CHANGE_SPEED_LIMIT 0x1999
#define PLAYER_DYNAMICS_DIRECTION_CHANGE_LONG_LIMIT 0x10000
#define PLAYER_DYNAMICS_PRIMARY_CONTROL_LOW_LIMIT 0x28
#define PLAYER_DYNAMICS_PRIMARY_CONTROL_FORCE_LIMIT 0x70000
#define PLAYER_DYNAMICS_PRIMARY_CONTROL_INPUT_LIMIT 0x53
#define PLAYER_DYNAMICS_PRIMARY_CONTROL_REDUCTION 0x0C
#define PLAYER_DYNAMICS_GEAR_AUTOMATIC_STAGE 3
#define PLAYER_DYNAMICS_GEAR_REVERSE_STAGE 2
#define PLAYER_DYNAMICS_GEAR_NEUTRAL_STAGE 1
#define PLAYER_DYNAMICS_REVERSE_GEAR_INDEX -2
#define PLAYER_DYNAMICS_NEUTRAL_GEAR_INDEX -1
#define PLAYER_DYNAMICS_GEAR_SOUND_COUNTDOWN 16
#define PLAYER_DYNAMICS_GEAR_SOUND_FRAME 11
#define PLAYER_DYNAMICS_GEAR_SOUND_DELAY_LIMIT 12
#define PLAYER_DYNAMICS_GEAR_SOUND_CLASS 12
#define PLAYER_DYNAMICS_GEAR_SOUND_DISTANCE 100
#define PLAYER_DYNAMICS_GEAR_SOUND_PAN 0xF00000
#define PLAYER_DYNAMICS_AXLE_HALF_DIVISOR 2
#define PLAYER_DYNAMICS_FRONT_AXLE_INDEX 1
#define PLAYER_DYNAMICS_REAR_AXLE_INDEX 2
#define PLAYER_DYNAMICS_SECONDARY_FORCE_CLAMP_LIMIT 100
#define PLAYER_DYNAMICS_LATERAL_FORCE_CLAMP_NUMERATOR 3
#define PLAYER_DYNAMICS_LATERAL_FORCE_CLAMP_DIVISOR 2
#define PLAYER_DYNAMICS_RESISTANCE_SPEED_LIMIT 0x4CCC
#define PLAYER_DYNAMICS_IDLE_SPEED_LIMIT 0x1999
#define PLAYER_DYNAMICS_STATISTICS_SPEED_SOUND_LIMIT 0x8000
#define PLAYER_DYNAMICS_STATISTICS_SPEED_SOUND_ID 87
#define PLAYER_DYNAMICS_STATISTICS_SLIP_SPEED_LIMIT 0x140000
#define PLAYER_DYNAMICS_STATISTICS_SLIP_SOUND_ID 40
#define PLAYER_DYNAMICS_STATISTICS_LONG_SPEED_LIMIT 0xCCC
#define PLAYER_DYNAMICS_STATISTICS_LONG_SPEED_SOUND_ID 120
#define PLAYER_DYNAMICS_STATISTICS_FIRST_SPEED_LIMIT 0x1AC937
#define PLAYER_DYNAMICS_STATISTICS_SECOND_SPEED_LIMIT 0x2CA49B
#define PLAYER_DYNAMICS_STATISTICS_YAW_LIMIT 0x320000
#define PLAYER_DYNAMICS_STATISTICS_FRAME_SCORE_INCREMENT 2
#define PLAYER_DYNAMICS_STATISTICS_NOTIFICATION_FRAMES 30
#define PLAYER_DYNAMICS_DRIVEN_LONGITUDINAL_SLIP_FLAG 2

void
update_player_vehicle_dynamics(SimulationSource *source_)
{
  const VehicleParameters          *parameters;
  VehicleGearShiftStatistics       *statistics;
  VehicleParameterState            *state;
  const VehicleSurfacePhysicsEntry *surface_physics;
  int32                             drive_force;
  int32                             force_magnitude;
  int32                             front_lateral_force;
  int32                             front_longitudinal_force;
  int32                             front_velocity_force;
  int32                             lateral_force;
  int32                             lateral_resistance;
  int32                             combined_speed_magnitude;
  int32                             longitudinal_force;
  int32                             longitudinal_resistance;
  int32                             rear_lateral_force;
  int32                             rear_longitudinal_force;
  int32                             rear_velocity_force;
  int32                             surface_index;
  int32                             yaw_force;

  state                      = (VehicleParameterState *) source_;
  parameters                 = state->parameters;
  g_DRIVEN_WHEEL_SPIN_ACTIVE = 0;
  g_VEHICLE_DYNAMICS_PHASE += PLAYER_DYNAMICS_FRAME_SCALE;

  force_magnitude = state->world_lateral_velocity;
  if(force_magnitude <= 0)
    force_magnitude = -force_magnitude;
  combined_speed_magnitude = state->world_longitudinal_velocity;
  if(combined_speed_magnitude <= 0)
    combined_speed_magnitude = -combined_speed_magnitude;
  if(combined_speed_magnitude < force_magnitude)
    {
      state->speed_magnitude =
        force_magnitude + (combined_speed_magnitude >> PLAYER_DYNAMICS_APPROXIMATE_DISTANCE_SHIFT);
    }
  else
    {
      state->speed_magnitude =
        combined_speed_magnitude + (force_magnitude >> PLAYER_DYNAMICS_APPROXIMATE_DISTANCE_SHIFT);
    }

  if(((G_DRIVING_WARNING_TIMER_FLAGS & PLAYER_DYNAMICS_TIMER_FLAG) == 0) ||
     (state->auxiliary_control_active != 0))
    {
      g_VEHICLE_PRIMARY_FORCE_CONTROL_ACTIVE   = 0;
      g_VEHICLE_SECONDARY_FORCE_CONTROL_ACTIVE = 0;
    }
  else
    {
      g_VEHICLE_PRIMARY_FORCE_CONTROL_ACTIVE   = g_ANTI_LOCK_BRAKES_ENABLED;
      g_VEHICLE_SECONDARY_FORCE_CONTROL_ACTIVE = g_TRACTION_CONTROL_ENABLED;
    }

  if(((G_DRIVING_WARNING_TIMER_FLAGS & PLAYER_DYNAMICS_TIMER_FLAG) != 0) ||
     (state->gear_control_index == PLAYER_DYNAMICS_NEUTRAL_GEAR_INDEX))
    {
      update_vehicle_engine_speed(state);
      if(state->gear_control_stage > 0)
        {
          if(state->gear_control_stage == PLAYER_DYNAMICS_GEAR_AUTOMATIC_STAGE)
            {
              update_vehicle_automatic_transmission(state);
            }
          else if(state->gear_control_stage == PLAYER_DYNAMICS_GEAR_REVERSE_STAGE)
            {
              state->gear_control_index = PLAYER_DYNAMICS_NEUTRAL_GEAR_INDEX;
            }
          else if(state->gear_control_stage == PLAYER_DYNAMICS_GEAR_NEUTRAL_STAGE)
            {
              state->gear_control_index = PLAYER_DYNAMICS_REVERSE_GEAR_INDEX;
            }
        }
    }

  drive_force                         = calculate_vehicle_drive_force(state);
  state->drive_force                  = drive_force;
  g_VEHICLE_DRIVE_FORCE               = drive_force;
  state->driven_wheel_slip_adjustment = 0;
  g_VEHICLE_FRONT_DRIVE_FORCE = ((parameters->front_drive_ratio >> PLAYER_DYNAMICS_FIXED8_SHIFT) *
                                 (drive_force >> PLAYER_DYNAMICS_FIXED8_SHIFT));
  g_VEHICLE_REAR_DRIVE_FORCE  = drive_force - g_VEHICLE_FRONT_DRIVE_FORCE;
  longitudinal_force = ((state->secondary_control_value * PLAYER_DYNAMICS_BRAKE_FORCE_SCALE) >>
                        PLAYER_DYNAMICS_FIXED8_SHIFT) *
                       ((state->front_tire_force_limit + state->rear_tire_force_limit) >>
                        PLAYER_DYNAMICS_FIXED8_SHIFT);
  g_VEHICLE_FRONT_BRAKE_FORCE = ((parameters->front_brake_ratio >> PLAYER_DYNAMICS_FIXED8_SHIFT) *
                                 (longitudinal_force >> PLAYER_DYNAMICS_FIXED8_SHIFT));
  g_VEHICLE_REAR_BRAKE_FORCE  = longitudinal_force - g_VEHICLE_FRONT_BRAKE_FORCE;

  state->tire_force_overload = 0;
  force_magnitude = state->front_tire_force_limit * PLAYER_DYNAMICS_TIRE_FORCE_LIMIT_NUMERATOR;
  if(force_magnitude < 0)
    {
      force_magnitude += ((1 << PLAYER_DYNAMICS_TIRE_FORCE_LIMIT_SHIFT) - 1);
    }
  if((force_magnitude >> PLAYER_DYNAMICS_TIRE_FORCE_LIMIT_SHIFT) < g_VEHICLE_FRONT_BRAKE_FORCE)
    {
      state->tire_force_overload = 1;
    }
  else
    {
      force_magnitude = state->rear_tire_force_limit * PLAYER_DYNAMICS_TIRE_FORCE_LIMIT_NUMERATOR;
      if(force_magnitude < 0)
        {
          force_magnitude += ((1 << PLAYER_DYNAMICS_TIRE_FORCE_LIMIT_SHIFT) - 1);
        }
      if((force_magnitude >> PLAYER_DYNAMICS_TIRE_FORCE_LIMIT_SHIFT) < g_VEHICLE_REAR_BRAKE_FORCE)
        {
          state->tire_force_overload = 1;
        }
    }

  if(state->auxiliary_control_active == 1)
    {
      g_VEHICLE_REAR_BRAKE_FORCE = parameters->auxiliary_brake_force;
      apply_control_sequence_vehicle_impulses();
    }

  force_magnitude = state->track_lateral_velocity;
  if(force_magnitude <= 0)
    force_magnitude = -force_magnitude;
  if((force_magnitude < PLAYER_DYNAMICS_DIRECTION_CHANGE_SPEED_LIMIT) &&
     (((state->vehicle_speed > PLAYER_DYNAMICS_DIRECTION_CHANGE_LONG_LIMIT) &&
       (state->gear_control_index == PLAYER_DYNAMICS_REVERSE_GEAR_INDEX)) ||
      ((state->vehicle_speed < -PLAYER_DYNAMICS_DIRECTION_CHANGE_LONG_LIMIT) &&
       (state->gear_control_index >= 0))))
    {
      if((g_VEHICLE_FRONT_DRIVE_FORCE != 0) &&
         (g_VEHICLE_FRONT_DRIVE_FORCE < state->front_tire_force_limit))
        {
          g_VEHICLE_FRONT_DRIVE_FORCE = 0;
          g_VEHICLE_FRONT_BRAKE_FORCE = parameters->auxiliary_brake_force;
        }
      if((g_VEHICLE_REAR_DRIVE_FORCE != 0) &&
         (g_VEHICLE_REAR_DRIVE_FORCE < state->rear_tire_force_limit))
        {
          g_VEHICLE_REAR_DRIVE_FORCE = 0;
          g_VEHICLE_REAR_BRAKE_FORCE = parameters->auxiliary_brake_force;
        }
    }

  if(state->primary_control_value < PLAYER_DYNAMICS_PRIMARY_CONTROL_LOW_LIMIT)
    {
      g_VEHICLE_SECONDARY_FORCE_CONTROL_ACTIVE = 0;
    }
  else if(g_VEHICLE_SECONDARY_FORCE_CONTROL_ACTIVE != 0)
    {
      force_magnitude = g_VEHICLE_DRIVE_FORCE;
      if(force_magnitude <= 0)
        force_magnitude = -force_magnitude;
      if((force_magnitude > PLAYER_DYNAMICS_PRIMARY_CONTROL_FORCE_LIMIT) &&
         (state->primary_control_value > PLAYER_DYNAMICS_PRIMARY_CONTROL_INPUT_LIMIT))
        {
          state->primary_control_value -= PLAYER_DYNAMICS_PRIMARY_CONTROL_REDUCTION;
        }
    }
  if(state->secondary_control_value < PLAYER_DYNAMICS_PRIMARY_CONTROL_LOW_LIMIT)
    {
      g_VEHICLE_PRIMARY_FORCE_CONTROL_ACTIVE = 0;
    }

  if(state->gear_control_index == state->gear_sound_current_index)
    {
      if(state->gear_sound_countdown > 0)
        {
          if((state->gear_sound_countdown > PLAYER_DYNAMICS_GEAR_SOUND_DELAY_LIMIT) ||
             (state->gear_shift_delay < 1))
            {
              state->gear_sound_countdown--;
            }
        }
    }
  else
    {
      state->gear_sound_previous_index = state->gear_sound_current_index;
      state->gear_sound_countdown      = PLAYER_DYNAMICS_GEAR_SOUND_COUNTDOWN;
      state->gear_sound_current_index  = state->gear_control_index;
      state->gear_sound_countdown--;
    }
  if((state->gear_sound_countdown == PLAYER_DYNAMICS_GEAR_SOUND_FRAME) &&
     (g_SIMULATION_RENDER_STATE.mode == 0) && (state->gear_shift_delay < 1))
    {
      play_spatial_sound_effect(-1,
                                PLAYER_DYNAMICS_GEAR_SOUND_CLASS,
                                0,
                                0,
                                PLAYER_DYNAMICS_GEAR_SOUND_DISTANCE,
                                PLAYER_DYNAMICS_GEAR_SOUND_PAN);
    }

  g_VEHICLE_LATERAL_RESISTANCE_FORCE =
    calculate_vehicle_surface_resistance(state, state->track_lateral_velocity);
  longitudinal_resistance = calculate_vehicle_surface_resistance(state, state->vehicle_speed);
  g_VEHICLE_LONGITUDINAL_RESISTANCE_FORCE = longitudinal_resistance;
  if(parameters->reverse_resistance_speed_threshold < state->vehicle_speed)
    {
      if((longitudinal_resistance > 0) && (g_VEHICLE_DRIVE_FORCE > longitudinal_resistance))
        {
          force_magnitude = g_VEHICLE_DRIVE_FORCE;
          if(force_magnitude <= 0)
            force_magnitude = -force_magnitude;
          g_VEHICLE_LONGITUDINAL_RESISTANCE_FORCE = force_magnitude;
        }
      else if(longitudinal_resistance < 0)
        {
          force_magnitude = -longitudinal_resistance;
          drive_force     = g_VEHICLE_DRIVE_FORCE;
          if(drive_force <= 0)
            drive_force = -drive_force;
          if(force_magnitude < drive_force)
            g_VEHICLE_LONGITUDINAL_RESISTANCE_FORCE = -drive_force;
        }
    }

  /* Steering affects tire forces; only integrated yaw changes body heading. */
  (void) normalize_angle24(state->heading_angle + state->control_event_value);
  rotate_coordinates_fixed16(state->world_lateral_velocity,
                             state->world_longitudinal_velocity,
                             -state->heading_angle,
                             &state->track_lateral_velocity,
                             &state->vehicle_speed);
  lateral_force      = state->track_lateral_velocity;
  longitudinal_force = state->vehicle_speed;
  front_velocity_force =
    MulSF16(lateral_force * PLAYER_DYNAMICS_FORCE_SAMPLE_MULTIPLIER, state->front_mass_fraction);
  yaw_force           = MulSF16(state->yaw_velocity * PLAYER_DYNAMICS_FORCE_SAMPLE_MULTIPLIER,
                                state->angular_velocity_to_linear_scale);
  rear_velocity_force = MulSF16(longitudinal_force * PLAYER_DYNAMICS_FORCE_SAMPLE_MULTIPLIER,
                                state->front_mass_fraction);
  lateral_resistance  = g_VEHICLE_LATERAL_RESISTANCE_FORCE / PLAYER_DYNAMICS_AXLE_HALF_DIVISOR;
  longitudinal_resistance =
    g_VEHICLE_LONGITUDINAL_RESISTANCE_FORCE / PLAYER_DYNAMICS_AXLE_HALF_DIVISOR;
  state->undriven_wheel_slip_flags = 0;
  state->driven_wheel_slip_flags   = 0;
  calculate_vehicle_axle_tire_forces(state,
                                     &front_lateral_force,
                                     &front_longitudinal_force,
                                     (-front_velocity_force - lateral_resistance) + yaw_force,
                                     -rear_velocity_force - longitudinal_resistance,
                                     state->control_event_value,
                                     g_VEHICLE_FRONT_DRIVE_FORCE,
                                     g_VEHICLE_FRONT_BRAKE_FORCE,
                                     PLAYER_DYNAMICS_FRONT_AXLE_INDEX);
  calculate_vehicle_axle_tire_forces(
    state,
    &rear_lateral_force,
    &rear_longitudinal_force,
    (-lateral_resistance -
     (lateral_force * PLAYER_DYNAMICS_FORCE_SAMPLE_MULTIPLIER - front_velocity_force)) -
      yaw_force,
    -longitudinal_resistance -
      (longitudinal_force * PLAYER_DYNAMICS_FORCE_SAMPLE_MULTIPLIER - rear_velocity_force),
    0,
    g_VEHICLE_REAR_DRIVE_FORCE,
    g_VEHICLE_REAR_BRAKE_FORCE,
    PLAYER_DYNAMICS_REAR_AXLE_INDEX);

  lateral_force      = front_lateral_force + rear_lateral_force;
  longitudinal_force = front_longitudinal_force + rear_longitudinal_force;
  if(state->secondary_control_value > PLAYER_DYNAMICS_SECONDARY_FORCE_CLAMP_LIMIT)
    {
      force_magnitude = longitudinal_force;
      if(force_magnitude <= 0)
        force_magnitude = -force_magnitude;
      if(force_magnitude > parameters->auxiliary_brake_force)
        {
          longitudinal_force = parameters->auxiliary_brake_force;
          if((front_longitudinal_force + rear_longitudinal_force) < 0)
            longitudinal_force = -longitudinal_force;
        }
      force_magnitude = lateral_force;
      if(force_magnitude <= 0)
        force_magnitude = -force_magnitude;
      drive_force = parameters->auxiliary_brake_force +
                    parameters->auxiliary_brake_force / PLAYER_DYNAMICS_LATERAL_FORCE_CLAMP_DIVISOR;
      if(force_magnitude > drive_force)
        {
          lateral_force = drive_force;
          if((front_lateral_force + rear_lateral_force) < 0)
            lateral_force = -lateral_force;
        }
    }
  state->combined_lateral_force      = lateral_force;
  state->combined_longitudinal_force = longitudinal_force;

  combined_speed_magnitude = state->vehicle_speed;
  if(combined_speed_magnitude <= 0)
    combined_speed_magnitude = -combined_speed_magnitude;
  force_magnitude = state->track_lateral_velocity;
  if(force_magnitude <= 0)
    force_magnitude = -force_magnitude;
  combined_speed_magnitude += force_magnitude;
  if(combined_speed_magnitude >= PLAYER_DYNAMICS_RESISTANCE_SPEED_LIMIT)
    {
      state->track_lateral_velocity +=
        MulSF16(lateral_force + g_VEHICLE_LATERAL_RESISTANCE_FORCE + state->lateral_force_offset,
                PLAYER_DYNAMICS_FRAME_SCALE);
      state->vehicle_speed += MulSF16(longitudinal_force + g_VEHICLE_LONGITUDINAL_RESISTANCE_FORCE +
                                        state->longitudinal_force_offset,
                                      PLAYER_DYNAMICS_FRAME_SCALE);
    }
  else
    {
      state->track_lateral_velocity += MulSF16(lateral_force, PLAYER_DYNAMICS_FRAME_SCALE);
      state->vehicle_speed += MulSF16(longitudinal_force, PLAYER_DYNAMICS_FRAME_SCALE);
      if((state->gear_control_index == PLAYER_DYNAMICS_NEUTRAL_GEAR_INDEX) ||
         (state->primary_control_value == 0))
        {
          state->vehicle_speed          = 0;
          state->track_lateral_velocity = 0;
        }
    }

  rotate_coordinates_fixed16(state->track_lateral_velocity,
                             state->vehicle_speed,
                             state->heading_angle,
                             &state->world_lateral_velocity,
                             &state->world_longitudinal_velocity);
  source_->position.z += MulSF16(state->world_longitudinal_velocity, PLAYER_DYNAMICS_FRAME_SCALE);
  source_->position.x -= MulSF16(state->world_lateral_velocity, PLAYER_DYNAMICS_FRAME_SCALE);

  combined_speed_magnitude = state->vehicle_speed + state->track_lateral_velocity;
  if(combined_speed_magnitude < PLAYER_DYNAMICS_IDLE_SPEED_LIMIT)
    {
      state->body_roll -= state->body_roll / PLAYER_DYNAMICS_HALF_DIVISOR;
      state->body_pitch -= state->body_pitch / PLAYER_DYNAMICS_HALF_DIVISOR;
      yaw_force = 0;
    }
  else
    {
      state->body_roll +=
        (-state->body_roll - ((parameters->body_roll_scale >> PLAYER_DYNAMICS_FIXED8_SHIFT) *
                              (state->combined_lateral_force >> PLAYER_DYNAMICS_FIXED8_SHIFT))) /
        PLAYER_DYNAMICS_HALF_DIVISOR;
      state->body_pitch +=
        (-state->body_pitch -
         ((parameters->body_pitch_scale >> PLAYER_DYNAMICS_FIXED8_SHIFT) *
          (state->combined_longitudinal_force >> PLAYER_DYNAMICS_FIXED8_SHIFT))) /
        PLAYER_DYNAMICS_HALF_DIVISOR;
      yaw_force = (longitudinal_force >> PLAYER_DYNAMICS_FIXED8_SHIFT) *
                  (state->center_of_gravity_to_wheelbase_ratio >> PLAYER_DYNAMICS_FIXED8_SHIFT);
    }

  surface_index = g_VEHICLE_SURFACE_INDEX;
  if(g_VEHICLE_ALTERNATE_SURFACE_BANK_ACTIVE == 0)
    surface_index += g_VEHICLE_SURFACE_BANK_OFFSET;
  surface_physics = &g_VEHICLE_SURFACE_PHYSICS[surface_index];
  force_magnitude =
    surface_physics->lateral_grip_scale * (state->initial_front_tire_force_limit - yaw_force);
  if(force_magnitude < 0)
    force_magnitude += ((1 << PLAYER_DYNAMICS_FIXED8_SHIFT) - 1);
  state->front_tire_force_limit = force_magnitude >> PLAYER_DYNAMICS_FIXED8_SHIFT;
  force_magnitude =
    surface_physics->lateral_grip_scale * (state->initial_rear_tire_force_limit + yaw_force);
  if(force_magnitude < 0)
    force_magnitude += ((1 << PLAYER_DYNAMICS_FIXED8_SHIFT) - 1);
  state->rear_tire_force_limit = force_magnitude >> PLAYER_DYNAMICS_FIXED8_SHIFT;

  yaw_force       = MulSF16(front_lateral_force, state->rear_yaw_force_scale);
  force_magnitude = MulSF16(rear_lateral_force, state->front_yaw_force_scale);
  yaw_force       = MulSF16(force_magnitude - yaw_force, PLAYER_DYNAMICS_FRAME_SCALE);
  state->yaw_velocity += yaw_force;
  state->heading_angle += MulSF16(state->yaw_velocity, PLAYER_DYNAMICS_FRAME_SCALE);
  state->heading_angle = normalize_angle24(state->heading_angle);
  update_player_vehicle_track_boundary(state);

  if(((G_DRIVING_WARNING_TIMER_FLAGS & PLAYER_DYNAMICS_TIMER_FLAG) == 0) ||
     (G_FRONTEND_EXIT_REQUESTED != 1))
    {
      return;
    }

  statistics      = g_VEHICLE_GEAR_SHIFT_STATISTICS;
  force_magnitude = state->track_lateral_velocity;
  if(force_magnitude <= 0)
    force_magnitude = -force_magnitude;
  drive_force = state->vehicle_speed / PLAYER_DYNAMICS_HALF_DIVISOR;
  if(drive_force <= 0)
    drive_force = -drive_force;
  if((force_magnitude > drive_force) &&
     (state->track_lateral_velocity > PLAYER_DYNAMICS_STATISTICS_SPEED_SOUND_LIMIT))
    {
      request_sound_effect(PLAYER_DYNAMICS_STATISTICS_SPEED_SOUND_ID);
    }

  if((state->undriven_wheel_slip_flags != 0) || (state->driven_wheel_slip_flags != 0))
    {
      statistics->tire_slip_score += PLAYER_DYNAMICS_STATISTICS_FRAME_SCORE_INCREMENT;
      if(state->speed_magnitude > PLAYER_DYNAMICS_STATISTICS_SLIP_SPEED_LIMIT)
        {
          if(((state->driven_wheel_state_flags & 1) != 0) ||
             ((state->undriven_wheel_state_flags & 1) != 0))
            {
              request_sound_effect(PLAYER_DYNAMICS_STATISTICS_SLIP_SOUND_ID);
            }
        }
    }

  if(g_GAME_RUNTIME_STATE.race_start_frame == 0)
    {
      force_magnitude = state->vehicle_speed;
      if(force_magnitude <= 0)
        force_magnitude = -force_magnitude;
      if(force_magnitude > PLAYER_DYNAMICS_STATISTICS_LONG_SPEED_LIMIT)
        {
          request_sound_effect(PLAYER_DYNAMICS_STATISTICS_LONG_SPEED_SOUND_ID);
          g_GAME_RUNTIME_STATE.race_start_frame = G_GAME_FRAME_COUNTER + 1;
        }
    }
  if((statistics->first_speed_threshold_frame == 0) &&
     (state->vehicle_speed > PLAYER_DYNAMICS_STATISTICS_FIRST_SPEED_LIMIT))
    {
      statistics->first_speed_threshold_frame = G_GAME_FRAME_COUNTER;
    }
  if((statistics->second_speed_threshold_frame == 0) &&
     (state->vehicle_speed > PLAYER_DYNAMICS_STATISTICS_SECOND_SPEED_LIMIT))
    {
      statistics->second_speed_threshold_frame = G_GAME_FRAME_COUNTER;
    }
  if(statistics->maximum_longitudinal_speed < state->vehicle_speed)
    {
      statistics->maximum_longitudinal_speed = state->vehicle_speed;
    }
  if(state->driven_wheel_state_flags != 0)
    {
      if((state->driven_wheel_state_flags & PLAYER_DYNAMICS_DRIVEN_LONGITUDINAL_SLIP_FLAG) != 0)
        {
          statistics->longitudinal_slip_frame_count++;
        }
      force_magnitude = state->yaw_velocity;
      if(force_magnitude <= 0)
        force_magnitude = -force_magnitude;
      if(force_magnitude > PLAYER_DYNAMICS_STATISTICS_YAW_LIMIT)
        {
          statistics->high_yaw_slip_count++;
          request_sound_effect(PLAYER_DYNAMICS_STATISTICS_SPEED_SOUND_ID);
        }
    }

  if(((G_GAME_FRAME_COUNTER - g_GAME_RUNTIME_STATE.race_start_frame) <
      g_GAME_RUNTIME_STATE.statistics_frame_window) &&
     (g_GAME_RUNTIME_STATE.statistics_track_segment_threshold <= source_->track_segment_index) &&
     (g_GAME_RUNTIME_STATE.statistics_capture_complete == 0))
    {
      g_GAME_RUNTIME_STATE.statistics_capture_complete = 1;
      g_VEHICLE_DRIVE_STATISTICS.summary.lane_search_limit++;
      g_VEHICLE_STATISTICS_NOTIFICATION_TIMER = PLAYER_DYNAMICS_STATISTICS_NOTIFICATION_FRAMES;
    }
}
