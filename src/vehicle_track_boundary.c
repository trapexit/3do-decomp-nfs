#include "vehicle_track_boundary.h"

#include "game_bootstrap.h"
#include "simulation_snapshot.h"
#include "sound_spatial.h"
#include "track_position.h"
#include "vehicle_collision_response.h"
#include "vehicle_dynamics_state.h"
#include "vehicle_runtime_pool.h"
#include "vehicle_simulation_state.h"

#include "operamath.h"

#define VEHICLE_TRACK_HEADING_SHIFT 10
#define VEHICLE_TRACK_WIDTH_SHIFT 13
#define VEHICLE_TRACK_MOTION_SHIFT 8
#define VEHICLE_TRACK_BOUNDARY_MARGIN 0x8000
#define VEHICLE_TRACK_AXLE_HALF_DIVISOR 2
#define VEHICLE_TRACK_SURFACE_NIBBLE_MASK 0xF
#define VEHICLE_TRACK_SURFACE_NIBBLE_SHIFT 4
#define VEHICLE_TRACK_POSITIVE_COLLISION_SIDE 0x100
#define VEHICLE_TRACK_NEGATIVE_COLLISION_SIDE -0x100
#define VEHICLE_TRACK_BOUNDARY_FORCE_SHIFT 12
#define VEHICLE_TRACK_BOUNDARY_FORCE_ROUNDING 0xFFF
#define VEHICLE_TRACK_COLLISION_SOUND_THRESHOLD 0x8000
#define VEHICLE_TRACK_COLLISION_SOUND_PAN_LEFT 0x280000
#define VEHICLE_TRACK_COLLISION_SOUND_PAN_RIGHT 0xD70000
#define VEHICLE_TRACK_COLLISION_SOUND_DISTANCE 1
#define VEHICLE_TRACK_COLLISION_SOUND_CLASS 2
#define VEHICLE_TRACK_COLLISION_SOUND_VARIANT 8
#define VEHICLE_TRACK_COLLISION_SOUND_GAIN_LIMIT 0x80000
#define VEHICLE_TRACK_COLLISION_SOUND_DISTANCE_SCALE 500
#define VEHICLE_TRACK_NONPLAYER_SOUND_DISTANCE_SCALE 1000
#define VEHICLE_TRACK_COLLISION_SOUND_DISTANCE_SHIFT 19
#define VEHICLE_TRACK_COLLISION_SOUND_DISTANCE_BIAS 0x7FFFF
#define VEHICLE_TRACK_COLLISION_FORCE_THRESHOLD 0x30000
#define VEHICLE_TRACK_COLLISION_FORCE_REDUCTION_SHIFT 2
#define VEHICLE_TRACK_COLLISION_FORCE_FIXED_SHIFT 8
#define VEHICLE_TRACK_COLLISION_FORCE_FIXED_BIAS 0xFF
#define VEHICLE_TRACK_LONGITUDINAL_DAMPING_SHIFT 3
#define VEHICLE_TRACK_NONPLAYER_DAMPING_SHIFT 2
#define VEHICLE_TRACK_LONGITUDINAL_DAMPING_BIAS 7
#define VEHICLE_TRACK_HARD_COLLISION_THRESHOLD 0x100000
#define VEHICLE_TRACK_HARD_COLLISION_SOUND_ID 50
#define VEHICLE_TRACK_PRIMARY_CONTROL_LIMIT 0x46
#define VEHICLE_TRACK_BARRIER_BEHAVIOR_COLLISION 0
#define VEHICLE_TRACK_BARRIER_BEHAVIOR_SLIDE 1
#define VEHICLE_TRACK_BARRIER_BEHAVIOR_TRANSITION 2
#define VEHICLE_TRACK_SLIDE_BOUNDARY_MARGIN 0x30000
#define VEHICLE_TRACK_SLIDE_LONGITUDINAL_LIMIT 0xA0000
#define VEHICLE_TRACK_SLIDE_DAMPING_SHIFT 5
#define VEHICLE_TRACK_SLIDE_DAMPING_BIAS 0x1F
#define VEHICLE_TRACK_SLIDE_FORCE_LIMIT 0x100000
#define VEHICLE_TRACK_SLIDE_SPEED_LIMIT 0x200000
#define VEHICLE_TRACK_SLIDE_FORCE_REDUCTION_SHIFT 2
#define VEHICLE_TRACK_GEOMETRY_BANK_FIRST 4
#define VEHICLE_TRACK_GEOMETRY_SPECIAL_AUDIO 5
#define VEHICLE_TRACK_GEOMETRY_BANK_SECOND 7
#define VEHICLE_TRACK_GEOMETRY_ALTERNATE 8
#define VEHICLE_TRACK_GEOMETRY_BANK_THIRD 9
#define READ_SIGNED_BIG_ENDIAN_INT16(value_)                                                       \
  ((int32) (int16) ((((uint32) ((const uint8 *) (value_))[0]) << 8) |                              \
                    (uint32) ((const uint8 *) (value_))[1]))

void
update_player_vehicle_track_boundary(VehicleParameterState *state_)
{
  SimulationSource   *source;
  const TrackSegment *segment;
  int32               boundary_behavior;
  int32               boundary_position;
  int32               collision_force;
  int32               collision_side;
  int32               collision_speed;
  int32               correction_x;
  int32               correction_z;
  int32               distance;
  int32               force_magnitude;
  int32               heading;
  int32               lateral_motion;
  int32               lateral_position;
  int32               longitudinal_motion;
  int32               pan;

  source  = (SimulationSource *) state_;
  segment = &g_TRACK_SEGMENTS[source->track_segment_index];
  heading = READ_SIGNED_BIG_ENDIAN_INT16(&segment->heading) << VEHICLE_TRACK_HEADING_SHIFT;
  lateral_position =
    (((segment->surface_reference.z - source->position.z) >> VEHICLE_TRACK_MOTION_SHIFT) *
     (SinF16(heading) >> VEHICLE_TRACK_MOTION_SHIFT)) -
    (((segment->surface_reference.x - source->position.x) >> VEHICLE_TRACK_MOTION_SHIFT) *
     (CosF16(heading) >> VEHICLE_TRACK_MOTION_SHIFT));

  if((segment->road_geometry_type == VEHICLE_TRACK_GEOMETRY_BANK_FIRST) ||
     (segment->road_geometry_type == VEHICLE_TRACK_GEOMETRY_BANK_SECOND) ||
     (segment->road_geometry_type == VEHICLE_TRACK_GEOMETRY_BANK_THIRD))
    {
      g_VEHICLE_ALTERNATE_SURFACE_BANK_ACTIVE = 1;
      g_VEHICLE_ALTERNATE_SURFACE_ACTIVE      = 1;
    }
  else
    {
      g_VEHICLE_ALTERNATE_SURFACE_BANK_ACTIVE = 0;
      g_VEHICLE_ALTERNATE_SURFACE_ACTIVE =
        (segment->road_geometry_type == VEHICLE_TRACK_GEOMETRY_ALTERNATE);
    }
  g_VEHICLE_SPECIAL_SURFACE_AUDIO_ACTIVE =
    (segment->road_geometry_type == VEHICLE_TRACK_GEOMETRY_SPECIAL_AUDIO);

  boundary_position = -((int32) segment->midpoint_negative_width << VEHICLE_TRACK_WIDTH_SHIFT);
  if(lateral_position >= boundary_position)
    {
      boundary_position =
        ((int32) segment->midpoint_positive_width << VEHICLE_TRACK_WIDTH_SHIFT);
      if(lateral_position <= boundary_position)
        {
          g_VEHICLE_SURFACE_INDEX             = g_VEHICLE_DEFAULT_SURFACE_INDEX;
          g_VEHICLE_SURFACE_TRANSITION_ACTIVE = 0;
          return;
        }

      g_VEHICLE_SURFACE_INDEX =
        (int32) segment->surface_index_codes & VEHICLE_TRACK_SURFACE_NIBBLE_MASK;
      boundary_position =
        ((int32) segment->positive_boundary_width << VEHICLE_TRACK_WIDTH_SHIFT);
      boundary_position -= source->lateral_axle_span / VEHICLE_TRACK_AXLE_HALF_DIVISOR;
      if(lateral_position <= boundary_position)
        {
          g_VEHICLE_SURFACE_TRANSITION_ACTIVE = 0;
          return;
        }
      collision_side  = VEHICLE_TRACK_POSITIVE_COLLISION_SIDE;
      collision_force = lateral_position - (boundary_position - VEHICLE_TRACK_BOUNDARY_MARGIN);
      boundary_behavior =
        (int32) segment->boundary_behavior_codes & VEHICLE_TRACK_SURFACE_NIBBLE_MASK;
    }
  else
    {
      g_VEHICLE_SURFACE_INDEX =
        (int32) segment->surface_index_codes >> VEHICLE_TRACK_SURFACE_NIBBLE_SHIFT;
      boundary_position =
        -((int32) segment->negative_boundary_width << VEHICLE_TRACK_WIDTH_SHIFT);
      boundary_position += source->lateral_axle_span / VEHICLE_TRACK_AXLE_HALF_DIVISOR;
      if(boundary_position <= lateral_position)
        {
          g_VEHICLE_SURFACE_TRANSITION_ACTIVE = 0;
          return;
        }
      collision_side  = VEHICLE_TRACK_NEGATIVE_COLLISION_SIDE;
      collision_force = (boundary_position + VEHICLE_TRACK_BOUNDARY_MARGIN) - lateral_position;
      boundary_behavior =
        ((int32) segment->boundary_behavior_codes >> VEHICLE_TRACK_SURFACE_NIBBLE_SHIFT) &
        VEHICLE_TRACK_SURFACE_NIBBLE_MASK;
    }

  if(boundary_behavior == VEHICLE_TRACK_BARRIER_BEHAVIOR_COLLISION)
    {
      longitudinal_motion = 0;
      if(collision_force <= 0)
        collision_force = -collision_force;
      collision_force *= collision_side;
      if(collision_force < 0)
        collision_force += VEHICLE_TRACK_BOUNDARY_FORCE_ROUNDING;
      collision_force >>= VEHICLE_TRACK_BOUNDARY_FORCE_SHIFT;
      rotate_coordinates_fixed16(collision_force, 0, heading, &correction_x, &correction_z);
      source->position.x -= correction_x;
      source->position.z += correction_z;
      rotate_coordinates_fixed16(source->motion.x,
                                 source->motion.z,
                                 -heading,
                                 &lateral_motion,
                                 &longitudinal_motion);
      collision_speed = lateral_motion;
      if(collision_speed <= 0)
        collision_speed = -collision_speed;
      if(collision_speed > VEHICLE_TRACK_COLLISION_SOUND_THRESHOLD)
        {
          if(g_SIMULATION_RENDER_STATE.mode == 0)
            {
              if(lateral_position <= 0)
                pan = VEHICLE_TRACK_COLLISION_SOUND_PAN_LEFT;
              else
                pan = VEHICLE_TRACK_COLLISION_SOUND_PAN_RIGHT;
              distance = VEHICLE_TRACK_COLLISION_SOUND_DISTANCE;
            }
          else
            {
              calculate_spatial_sound_parameters(
                (const Vector3 *) g_VEHICLE_RUNTIME_POOL_STATE.player_object,
                &pan,
                &distance);
            }
          force_magnitude = collision_speed;
          if(force_magnitude > VEHICLE_TRACK_COLLISION_SOUND_GAIN_LIMIT)
            force_magnitude = VEHICLE_TRACK_COLLISION_SOUND_GAIN_LIMIT;
          force_magnitude = ((VEHICLE_TRACK_COLLISION_SOUND_GAIN_LIMIT - force_magnitude) *
                             VEHICLE_TRACK_COLLISION_SOUND_DISTANCE_SCALE);
          if(force_magnitude < 0)
            {
              force_magnitude += VEHICLE_TRACK_COLLISION_SOUND_DISTANCE_BIAS;
            }
          distance += force_magnitude >> VEHICLE_TRACK_COLLISION_SOUND_DISTANCE_SHIFT;
          play_spatial_sound_effect_scaled(-1,
                                           VEHICLE_TRACK_COLLISION_SOUND_CLASS,
                                           VEHICLE_TRACK_COLLISION_SOUND_VARIANT,
                                           collision_speed,
                                           distance,
                                           pan);
        }

      collision_force = collision_speed - VEHICLE_TRACK_COLLISION_FORCE_THRESHOLD;
      if(collision_force < 0)
        collision_force = 0;
      force_magnitude = collision_force;
      if(force_magnitude < 0)
        force_magnitude += ((1 << VEHICLE_TRACK_COLLISION_FORCE_REDUCTION_SHIFT) - 1);
      lateral_motion =
        collision_side * (force_magnitude >> VEHICLE_TRACK_COLLISION_FORCE_REDUCTION_SHIFT);
      if(lateral_motion < 0)
        lateral_motion += VEHICLE_TRACK_COLLISION_FORCE_FIXED_BIAS;
      lateral_motion >>= VEHICLE_TRACK_COLLISION_FORCE_FIXED_SHIFT;
      if(longitudinal_motion > 0)
        collision_force = -collision_force;
      longitudinal_motion += collision_force;
      force_magnitude = longitudinal_motion;
      if(force_magnitude < 0)
        force_magnitude += VEHICLE_TRACK_LONGITUDINAL_DAMPING_BIAS;
      longitudinal_motion -= force_magnitude >> VEHICLE_TRACK_LONGITUDINAL_DAMPING_SHIFT;
      if((collision_speed - VEHICLE_TRACK_COLLISION_FORCE_THRESHOLD) >
         VEHICLE_TRACK_HARD_COLLISION_THRESHOLD)
        {
          request_sound_effect(VEHICLE_TRACK_HARD_COLLISION_SOUND_ID);
        }
      if((state_->primary_control_value > VEHICLE_TRACK_PRIMARY_CONTROL_LIMIT) &&
         (state_->gear_control_ready != 0))
        {
          state_->primary_control_value = VEHICLE_TRACK_PRIMARY_CONTROL_LIMIT;
        }
      rotate_coordinates_fixed16(lateral_motion,
                                 longitudinal_motion,
                                 heading,
                                 &source->motion.x,
                                 &source->motion.z);
      apply_vehicle_boundary_collision(state_, heading, collision_speed, collision_side, 0);
      return;
    }

  if(boundary_behavior == VEHICLE_TRACK_BARRIER_BEHAVIOR_TRANSITION)
    {
      g_VEHICLE_SURFACE_TRANSITION_ACTIVE = 1;
      return;
    }
  if(boundary_behavior != VEHICLE_TRACK_BARRIER_BEHAVIOR_SLIDE)
    return;

  if(collision_force <= 0)
    collision_force -= VEHICLE_TRACK_SLIDE_BOUNDARY_MARGIN;
  else
    collision_force += VEHICLE_TRACK_SLIDE_BOUNDARY_MARGIN;
  longitudinal_motion = 0;
  if(collision_force <= 0)
    collision_force = -collision_force;
  collision_force *= collision_side;
  if(collision_force < 0)
    {
      collision_force += VEHICLE_TRACK_BOUNDARY_FORCE_ROUNDING;
    }
  collision_force >>= VEHICLE_TRACK_BOUNDARY_FORCE_SHIFT;
  rotate_coordinates_fixed16(collision_force, 0, heading, &correction_x, &correction_z);
  source->position.x -= correction_x;
  source->position.z += correction_z;
  rotate_coordinates_fixed16(source->motion.x,
                             source->motion.z,
                             -heading,
                             &lateral_motion,
                             &longitudinal_motion);
  collision_speed = 0;
  if(((lateral_motion > 0) && (collision_side < 0)) ||
     ((lateral_motion < 0) && (collision_side > 0)))
    {
      force_magnitude = lateral_motion;
      if(force_magnitude <= 0)
        force_magnitude = -force_magnitude;
      if(longitudinal_motion < 0)
        {
          longitudinal_motion += force_magnitude;
          if(longitudinal_motion > 0)
            longitudinal_motion = 0;
        }
      else
        {
          longitudinal_motion -= force_magnitude;
          if(longitudinal_motion < 0)
            longitudinal_motion = 0;
        }
      force_magnitude = longitudinal_motion;
      if(force_magnitude <= 0)
        force_magnitude = -force_magnitude;
      if(force_magnitude > VEHICLE_TRACK_SLIDE_LONGITUDINAL_LIMIT)
        {
          force_magnitude = longitudinal_motion;
          if(force_magnitude < 0)
            force_magnitude += VEHICLE_TRACK_SLIDE_DAMPING_BIAS;
          longitudinal_motion -= force_magnitude >> VEHICLE_TRACK_SLIDE_DAMPING_SHIFT;
        }

      collision_speed = lateral_motion;
      if(collision_speed <= 0)
        collision_speed = -collision_speed;
      if((collision_speed < VEHICLE_TRACK_SLIDE_FORCE_LIMIT) ||
         (state_->speed_magnitude < VEHICLE_TRACK_SLIDE_SPEED_LIMIT))
        {
          lateral_motion = 0;
        }
      else
        {
          if(lateral_motion < 0)
            {
              lateral_motion += ((1 << VEHICLE_TRACK_SLIDE_FORCE_REDUCTION_SHIFT) - 1);
            }
          lateral_motion >>= VEHICLE_TRACK_SLIDE_FORCE_REDUCTION_SHIFT;
        }
      if((state_->primary_control_value > VEHICLE_TRACK_PRIMARY_CONTROL_LIMIT) &&
         (state_->gear_control_ready != 0))
        {
          state_->primary_control_value = VEHICLE_TRACK_PRIMARY_CONTROL_LIMIT;
        }
    }

  rotate_coordinates_fixed16(lateral_motion,
                             longitudinal_motion,
                             heading,
                             &source->motion.x,
                             &source->motion.z);
  apply_vehicle_boundary_collision(state_, heading, collision_speed, collision_side, 1);
}

int32
is_vehicle_outside_track_bounds(SimulationSource *source_)
{
  const TrackSegment *segment;
  int32               boundary_behavior;
  int32               boundary_position;
  int32               collision_force;
  int32               collision_side;
  int32               collision_speed;
  int32               correction_x;
  int32               correction_z;
  int32               distance;
  int32               force_magnitude;
  int32               heading;
  int32               lateral_motion;
  int32               lateral_position;
  int32               longitudinal_motion;
  int32               pan;

  segment          = &g_TRACK_SEGMENTS[source_->track_segment_index];
  heading          = READ_SIGNED_BIG_ENDIAN_INT16(&segment->heading) << VEHICLE_TRACK_HEADING_SHIFT;
  lateral_position = source_->lateral_position;
  boundary_position = -((int32) segment->midpoint_negative_width << VEHICLE_TRACK_WIDTH_SHIFT);

  if(lateral_position >= boundary_position)
    {
      boundary_position = ((int32) segment->midpoint_positive_width << VEHICLE_TRACK_WIDTH_SHIFT);
      if(lateral_position <= boundary_position)
        return 0;

      boundary_position = ((int32) segment->positive_boundary_width << VEHICLE_TRACK_WIDTH_SHIFT);
      boundary_position -= (source_->lateral_axle_span / VEHICLE_TRACK_AXLE_HALF_DIVISOR);
      if(lateral_position <= boundary_position)
        return 0;

      collision_side  = VEHICLE_TRACK_POSITIVE_COLLISION_SIDE;
      collision_force = (lateral_position - (boundary_position - VEHICLE_TRACK_BOUNDARY_MARGIN));
      boundary_behavior =
        ((int32) segment->boundary_behavior_codes & VEHICLE_TRACK_SURFACE_NIBBLE_MASK);
    }
  else
    {
      boundary_position = -((int32) segment->negative_boundary_width << VEHICLE_TRACK_WIDTH_SHIFT);
      boundary_position += (source_->lateral_axle_span / VEHICLE_TRACK_AXLE_HALF_DIVISOR);
      if(boundary_position <= lateral_position)
        return 0;

      collision_side  = VEHICLE_TRACK_NEGATIVE_COLLISION_SIDE;
      collision_force = ((boundary_position + VEHICLE_TRACK_BOUNDARY_MARGIN) - lateral_position);
      boundary_behavior =
        (((int32) segment->boundary_behavior_codes >> VEHICLE_TRACK_SURFACE_NIBBLE_SHIFT) &
         VEHICLE_TRACK_SURFACE_NIBBLE_MASK);
    }

  if(boundary_behavior != VEHICLE_TRACK_BARRIER_BEHAVIOR_COLLISION)
    return 0;

  longitudinal_motion = 0;
  if(collision_force <= 0)
    collision_force = -collision_force;
  collision_force *= collision_side;
  if(collision_force < 0)
    collision_force += VEHICLE_TRACK_BOUNDARY_FORCE_ROUNDING;
  collision_force >>= VEHICLE_TRACK_BOUNDARY_FORCE_SHIFT;
  rotate_coordinates_fixed16(collision_force, 0, heading, &correction_x, &correction_z);
  source_->position.x -= correction_x;
  source_->position.z += correction_z;
  rotate_coordinates_fixed16(source_->motion.x,
                             source_->motion.z,
                             -heading,
                             &lateral_motion,
                             &longitudinal_motion);

  collision_speed = lateral_motion;
  if(collision_speed <= 0)
    collision_speed = -collision_speed;
  if(collision_speed > VEHICLE_TRACK_COLLISION_SOUND_THRESHOLD)
    {
      calculate_spatial_sound_parameters((const Vector3 *) source_->runtime_record,
                                         &pan,
                                         &distance);
      force_magnitude = collision_speed;
      if(force_magnitude > VEHICLE_TRACK_COLLISION_SOUND_GAIN_LIMIT)
        force_magnitude = VEHICLE_TRACK_COLLISION_SOUND_GAIN_LIMIT;
      force_magnitude = ((VEHICLE_TRACK_COLLISION_SOUND_GAIN_LIMIT - force_magnitude) *
                         VEHICLE_TRACK_NONPLAYER_SOUND_DISTANCE_SCALE);
      if(force_magnitude < 0)
        force_magnitude += VEHICLE_TRACK_COLLISION_SOUND_DISTANCE_BIAS;
      distance += (force_magnitude >> VEHICLE_TRACK_COLLISION_SOUND_DISTANCE_SHIFT);
      play_spatial_sound_effect_scaled(-1,
                                       VEHICLE_TRACK_COLLISION_SOUND_CLASS,
                                       VEHICLE_TRACK_COLLISION_SOUND_VARIANT,
                                       collision_speed,
                                       distance,
                                       pan);
    }

  collision_force = (collision_speed - VEHICLE_TRACK_COLLISION_FORCE_THRESHOLD);
  if(collision_force < 0)
    collision_force = 0;
  force_magnitude = collision_force;
  if(force_magnitude < 0)
    {
      force_magnitude += ((1 << VEHICLE_TRACK_COLLISION_FORCE_REDUCTION_SHIFT) - 1);
    }
  lateral_motion =
    (collision_side * (force_magnitude >> VEHICLE_TRACK_COLLISION_FORCE_REDUCTION_SHIFT));
  if(lateral_motion < 0)
    lateral_motion += VEHICLE_TRACK_COLLISION_FORCE_FIXED_BIAS;
  lateral_motion >>= VEHICLE_TRACK_COLLISION_FORCE_FIXED_SHIFT;
  if(longitudinal_motion > 0)
    collision_force = -collision_force;
  longitudinal_motion += collision_force;
  force_magnitude = longitudinal_motion;
  if(force_magnitude < 0)
    {
      force_magnitude += ((1 << VEHICLE_TRACK_NONPLAYER_DAMPING_SHIFT) - 1);
    }
  longitudinal_motion -= (force_magnitude >> VEHICLE_TRACK_NONPLAYER_DAMPING_SHIFT);
  rotate_coordinates_fixed16(lateral_motion,
                             longitudinal_motion,
                             heading,
                             &source_->motion.x,
                             &source_->motion.z);
  apply_nonplayer_vehicle_boundary_collision(source_, heading, collision_speed, collision_side, 0);
  return 1;
}
