#include "vehicle_collision_response.h"

#include "simulation_snapshot.h"
#include "vehicle_dynamics_state.h"
#include "vehicle_simulation_state.h"

#define VEHICLE_COLLISION_ANGLE_FULL_TURN 0x1000000
#define VEHICLE_COLLISION_ANGLE_HALF_TURN 0x800000
#define VEHICLE_COLLISION_ANGLE_QUARTER_TURN 0x400000
#define VEHICLE_COLLISION_BOUNDARY_ANGLE_OFFSET 0x50000
#define VEHICLE_COLLISION_HIGH_SPEED_THRESHOLD 0x100000
#define VEHICLE_COLLISION_STATE_SPEED_THRESHOLD 0x200000
#define VEHICLE_COLLISION_HIGH_SPEED_REDUCTION 0x60000
#define VEHICLE_COLLISION_HIGH_SPEED_LIMIT 0xE0000
#define VEHICLE_COLLISION_ANGLE_REDUCTION_DIVISOR 8
#define VEHICLE_COLLISION_ANGLE_COMPONENT_SHIFT 16
#define VEHICLE_COLLISION_PROJECTION_DIVISOR 64
#define VEHICLE_COLLISION_IMPULSE_REDUCTION_NUMERATOR 3
#define VEHICLE_COLLISION_IMPULSE_REDUCTION_DIVISOR 4
#define VEHICLE_COLLISION_LOW_ANGLE_LIMIT 0x200000
#define VEHICLE_COLLISION_SPEED_MULTIPLIER 8
#define VEHICLE_COLLISION_YAW_MULTIPLIER 2
#define VEHICLE_COLLISION_MAXIMUM_CONTROL_IMPULSE 0xA0000
#define VEHICLE_COLLISION_MINIMUM_YAW_IMPULSE 0x50000
#define VEHICLE_COLLISION_CONTROL_EVENT_LIMIT 0x1E0000
#define VEHICLE_COLLISION_IMPULSE_PROJECTION_SHIFT 16

void
apply_vehicle_boundary_collision(VehicleParameterState *state_,
                                 int32                  boundary_heading_,
                                 int32                  collision_speed_,
                                 int32                  collision_side_,
                                 int32                  collision_mode_)
{
  SimulationSource *active_source;
  int32             collision_angle;
  int32             collision_angle_magnitude;
  int32             control_impulse;
  int32             lateral_impulse;
  int32             longitudinal_impulse;
  int32             yaw_impulse;

  if(collision_side_ <= 0)
    boundary_heading_ += VEHICLE_COLLISION_BOUNDARY_ANGLE_OFFSET;
  else
    boundary_heading_ -= VEHICLE_COLLISION_BOUNDARY_ANGLE_OFFSET;

  collision_angle = boundary_heading_ - state_->heading_angle;
  if(collision_angle < 0)
    collision_angle += VEHICLE_COLLISION_ANGLE_FULL_TURN;
  collision_angle_magnitude = collision_angle;
  if(collision_angle > VEHICLE_COLLISION_ANGLE_HALF_TURN)
    {
      collision_angle_magnitude = (VEHICLE_COLLISION_ANGLE_FULL_TURN - collision_angle);
    }
  if(collision_angle_magnitude > VEHICLE_COLLISION_ANGLE_QUARTER_TURN)
    {
      collision_angle += VEHICLE_COLLISION_ANGLE_HALF_TURN;
      collision_angle_magnitude = (VEHICLE_COLLISION_ANGLE_FULL_TURN - collision_angle);
    }

  if((collision_speed_ > VEHICLE_COLLISION_HIGH_SPEED_THRESHOLD) &&
     (state_->speed_magnitude > VEHICLE_COLLISION_STATE_SPEED_THRESHOLD))
    {
      active_source = (SimulationSource *) G_ACTIVE_SIMULATION_SOURCE;
      collision_speed_ -= VEHICLE_COLLISION_HIGH_SPEED_REDUCTION;
      if(collision_angle > VEHICLE_COLLISION_ANGLE_FULL_TURN)
        collision_angle -= VEHICLE_COLLISION_ANGLE_FULL_TURN;
      if(collision_angle < VEHICLE_COLLISION_ANGLE_HALF_TURN)
        {
          collision_angle /= VEHICLE_COLLISION_ANGLE_REDUCTION_DIVISOR;
        }
      else
        {
          collision_angle = (VEHICLE_COLLISION_ANGLE_FULL_TURN -
                             ((VEHICLE_COLLISION_ANGLE_FULL_TURN - collision_angle) /
                              VEHICLE_COLLISION_ANGLE_REDUCTION_DIVISOR));
        }
      if(collision_angle > VEHICLE_COLLISION_ANGLE_HALF_TURN)
        collision_angle -= VEHICLE_COLLISION_ANGLE_FULL_TURN;
      if(collision_speed_ > VEHICLE_COLLISION_HIGH_SPEED_LIMIT)
        collision_speed_ = VEHICLE_COLLISION_HIGH_SPEED_LIMIT;

      lateral_impulse = ((collision_speed_ *
                          (collision_angle_magnitude >> VEHICLE_COLLISION_ANGLE_COMPONENT_SHIFT)) /
                         VEHICLE_COLLISION_PROJECTION_DIVISOR);
      longitudinal_impulse = collision_speed_ - lateral_impulse;
      if(collision_angle <= 0)
        {
          collision_speed_     = -collision_speed_;
          longitudinal_impulse = -longitudinal_impulse;
        }
      if(collision_mode_ == 0)
        longitudinal_impulse = -longitudinal_impulse;
      else
        lateral_impulse = -lateral_impulse;
      g_VEHICLE_COLLISION_SLIDE_IMPULSE = lateral_impulse;
      if(collision_speed_ == 0)
        return;

      lateral_impulse =
        ((g_VEHICLE_COLLISION_SLIDE_IMPULSE * VEHICLE_COLLISION_IMPULSE_REDUCTION_NUMERATOR) /
         VEHICLE_COLLISION_IMPULSE_REDUCTION_DIVISOR);
      longitudinal_impulse =
        ((longitudinal_impulse * VEHICLE_COLLISION_IMPULSE_REDUCTION_NUMERATOR) /
         VEHICLE_COLLISION_IMPULSE_REDUCTION_DIVISOR);
      collision_speed_ /= VEHICLE_COLLISION_IMPULSE_REDUCTION_DIVISOR;
      apply_simulation_local_impulse(active_source,
                                     collision_speed_,
                                     longitudinal_impulse,
                                     lateral_impulse);
      return;
    }

  if(collision_mode_ == 0)
    {
      if(collision_angle > VEHICLE_COLLISION_ANGLE_FULL_TURN)
        collision_angle -= VEHICLE_COLLISION_ANGLE_FULL_TURN;
      if(collision_angle < VEHICLE_COLLISION_ANGLE_HALF_TURN)
        {
          collision_angle /= VEHICLE_COLLISION_ANGLE_REDUCTION_DIVISOR;
        }
      else
        {
          collision_angle = (VEHICLE_COLLISION_ANGLE_FULL_TURN -
                             ((VEHICLE_COLLISION_ANGLE_FULL_TURN - collision_angle) /
                              VEHICLE_COLLISION_ANGLE_REDUCTION_DIVISOR));
        }
      if(collision_angle > VEHICLE_COLLISION_ANGLE_HALF_TURN)
        collision_angle -= VEHICLE_COLLISION_ANGLE_FULL_TURN;

      lateral_impulse = ((collision_speed_ *
                          (collision_angle_magnitude >> VEHICLE_COLLISION_ANGLE_COMPONENT_SHIFT) *
                          VEHICLE_COLLISION_SPEED_MULTIPLIER) /
                         VEHICLE_COLLISION_PROJECTION_DIVISOR);
      yaw_impulse     = ((collision_speed_ * VEHICLE_COLLISION_SPEED_MULTIPLIER) - lateral_impulse);
      control_impulse = yaw_impulse * VEHICLE_COLLISION_YAW_MULTIPLIER;
      if(control_impulse > VEHICLE_COLLISION_MAXIMUM_CONTROL_IMPULSE)
        control_impulse = VEHICLE_COLLISION_MAXIMUM_CONTROL_IMPULSE;
      if(yaw_impulse < VEHICLE_COLLISION_MINIMUM_YAW_IMPULSE)
        yaw_impulse = VEHICLE_COLLISION_MINIMUM_YAW_IMPULSE;

      if(collision_angle < 0)
        {
          state_->yaw_velocity -= yaw_impulse;
          if(state_->vehicle_speed > 0)
            {
              state_->control_event_value -= control_impulse;
              if(state_->control_event_value < -VEHICLE_COLLISION_CONTROL_EVENT_LIMIT)
                {
                  state_->control_event_value = -VEHICLE_COLLISION_CONTROL_EVENT_LIMIT;
                }
            }
        }
      else
        {
          state_->yaw_velocity += yaw_impulse;
          if(state_->vehicle_speed > 0)
            {
              state_->control_event_value += control_impulse;
              if(state_->control_event_value > VEHICLE_COLLISION_CONTROL_EVENT_LIMIT)
                {
                  state_->control_event_value = VEHICLE_COLLISION_CONTROL_EVENT_LIMIT;
                }
            }
        }
    }

  if(collision_angle_magnitude >= VEHICLE_COLLISION_LOW_ANGLE_LIMIT)
    return;
  if(collision_angle > VEHICLE_COLLISION_ANGLE_FULL_TURN)
    collision_angle -= VEHICLE_COLLISION_ANGLE_FULL_TURN;
  if(collision_angle < VEHICLE_COLLISION_ANGLE_HALF_TURN)
    {
      collision_angle /= VEHICLE_COLLISION_ANGLE_REDUCTION_DIVISOR;
    }
  else
    {
      collision_angle = (VEHICLE_COLLISION_ANGLE_FULL_TURN -
                         ((VEHICLE_COLLISION_ANGLE_FULL_TURN - collision_angle) /
                          VEHICLE_COLLISION_ANGLE_REDUCTION_DIVISOR));
    }
  state_->heading_angle += collision_angle;
}

void
apply_nonplayer_vehicle_boundary_collision(SimulationSource *source_,
                                           int32             boundary_heading_,
                                           int32             collision_speed_,
                                           int32             collision_side_,
                                           int32             collision_mode_)
{
  int32 collision_angle;
  int32 collision_angle_magnitude;
  int32 lateral_impulse;
  int32 longitudinal_impulse;

  (void) boundary_heading_;
  (void) collision_side_;
  collision_angle = -source_->heading_rotation;
  if(collision_angle < 0)
    collision_angle += VEHICLE_COLLISION_ANGLE_FULL_TURN;

  collision_angle_magnitude = collision_angle;
  if(collision_angle > VEHICLE_COLLISION_ANGLE_HALF_TURN)
    {
      collision_angle_magnitude = (VEHICLE_COLLISION_ANGLE_FULL_TURN - collision_angle);
    }
  if(collision_angle_magnitude > VEHICLE_COLLISION_ANGLE_QUARTER_TURN)
    {
      collision_angle += VEHICLE_COLLISION_ANGLE_HALF_TURN;
      collision_angle_magnitude = (VEHICLE_COLLISION_ANGLE_FULL_TURN - collision_angle);
    }

  if((collision_speed_ >= VEHICLE_COLLISION_HIGH_SPEED_THRESHOLD) &&
     (source_->speed_magnitude > VEHICLE_COLLISION_STATE_SPEED_THRESHOLD))
    {
      collision_speed_ -= VEHICLE_COLLISION_HIGH_SPEED_REDUCTION;
      if(collision_angle > VEHICLE_COLLISION_ANGLE_FULL_TURN)
        collision_angle -= VEHICLE_COLLISION_ANGLE_FULL_TURN;
      if(collision_angle < VEHICLE_COLLISION_ANGLE_HALF_TURN)
        collision_angle /= VEHICLE_COLLISION_ANGLE_REDUCTION_DIVISOR;
      else
        {
          collision_angle = (VEHICLE_COLLISION_ANGLE_FULL_TURN -
                             ((VEHICLE_COLLISION_ANGLE_FULL_TURN - collision_angle) /
                              VEHICLE_COLLISION_ANGLE_REDUCTION_DIVISOR));
        }
      if(collision_angle > VEHICLE_COLLISION_ANGLE_HALF_TURN)
        collision_angle -= VEHICLE_COLLISION_ANGLE_FULL_TURN;
      if(collision_speed_ > VEHICLE_COLLISION_HIGH_SPEED_LIMIT)
        collision_speed_ = VEHICLE_COLLISION_HIGH_SPEED_LIMIT;

      lateral_impulse      = ((collision_speed_ * (collision_angle_magnitude >>
                                                   VEHICLE_COLLISION_IMPULSE_PROJECTION_SHIFT)) /
                              VEHICLE_COLLISION_PROJECTION_DIVISOR);
      longitudinal_impulse = (collision_speed_ - lateral_impulse);
      if(collision_angle <= 0)
        {
          collision_speed_     = -collision_speed_;
          longitudinal_impulse = -longitudinal_impulse;
        }
      if(collision_mode_ == 0)
        longitudinal_impulse = -longitudinal_impulse;
      else
        lateral_impulse = -lateral_impulse;
      if(collision_speed_ == 0)
        return;

      lateral_impulse = ((lateral_impulse * VEHICLE_COLLISION_IMPULSE_REDUCTION_NUMERATOR) /
                         VEHICLE_COLLISION_IMPULSE_REDUCTION_DIVISOR);
      longitudinal_impulse =
        ((longitudinal_impulse * VEHICLE_COLLISION_IMPULSE_REDUCTION_NUMERATOR) /
         VEHICLE_COLLISION_IMPULSE_REDUCTION_DIVISOR);
      collision_speed_ /= VEHICLE_COLLISION_IMPULSE_REDUCTION_DIVISOR;
      apply_simulation_local_impulse((SimulationSource *) source_->runtime_record,
                                     collision_speed_,
                                     longitudinal_impulse,
                                     lateral_impulse);
      return;
    }

  if(collision_mode_ == 0)
    {
      if(collision_angle > VEHICLE_COLLISION_ANGLE_FULL_TURN)
        collision_angle -= VEHICLE_COLLISION_ANGLE_FULL_TURN;
      if(collision_angle < VEHICLE_COLLISION_ANGLE_HALF_TURN)
        collision_angle /= VEHICLE_COLLISION_ANGLE_REDUCTION_DIVISOR;
      else
        {
          collision_angle = (VEHICLE_COLLISION_ANGLE_FULL_TURN -
                             ((VEHICLE_COLLISION_ANGLE_FULL_TURN - collision_angle) /
                              VEHICLE_COLLISION_ANGLE_REDUCTION_DIVISOR));
        }
      if(collision_angle > VEHICLE_COLLISION_ANGLE_HALF_TURN)
        collision_angle -= VEHICLE_COLLISION_ANGLE_FULL_TURN;

      longitudinal_impulse =
        ((collision_speed_ *
          (collision_angle_magnitude >> VEHICLE_COLLISION_IMPULSE_PROJECTION_SHIFT) *
          VEHICLE_COLLISION_SPEED_MULTIPLIER) /
         VEHICLE_COLLISION_PROJECTION_DIVISOR);
      longitudinal_impulse =
        ((collision_speed_ * VEHICLE_COLLISION_SPEED_MULTIPLIER) - longitudinal_impulse);
      if(collision_angle < 0)
        longitudinal_impulse = -longitudinal_impulse;
      source_->heading_velocity += longitudinal_impulse;
    }

  if(collision_angle_magnitude >= VEHICLE_COLLISION_LOW_ANGLE_LIMIT)
    return;
  if(collision_angle > VEHICLE_COLLISION_ANGLE_FULL_TURN)
    collision_angle -= VEHICLE_COLLISION_ANGLE_FULL_TURN;
  if(collision_angle < VEHICLE_COLLISION_ANGLE_HALF_TURN)
    collision_angle /= VEHICLE_COLLISION_ANGLE_REDUCTION_DIVISOR;
  else
    {
      collision_angle = (VEHICLE_COLLISION_ANGLE_FULL_TURN -
                         ((VEHICLE_COLLISION_ANGLE_FULL_TURN - collision_angle) /
                          VEHICLE_COLLISION_ANGLE_REDUCTION_DIVISOR));
    }
  source_->heading_rotation += collision_angle;
}
