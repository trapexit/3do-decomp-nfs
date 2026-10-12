#include "fixed_math.h"
#include "geometry.h"
#include "track_position.h"
#include "vehicle_simulation_state.h"
#include "vehicle_track_boundary.h"

#include "operamath.h"

#define VEHICLE_FIXED8_SHIFT 8
#define VEHICLE_BIG_ENDIAN_BYTE_SHIFT 8
#define VEHICLE_TRACK_MATRIX_DIMENSION 3
#define VEHICLE_TRACK_NEAR_DISTANCE 54
#define VEHICLE_TRACK_MINIMUM_DISTANCE (-20)
#define VEHICLE_TRACK_MINIMUM_SEGMENT 2
#define VEHICLE_HEADING_LIMIT 0x12C0000
#define VEHICLE_HEADING_COLLISION_LIMIT 0xA0000
#define VEHICLE_SPEED_COLLISION_LIMIT 0x90000
#define VEHICLE_SPEED_UPDATE_LIMIT 0x30000
#define VEHICLE_HEADING_SNAP_MARGIN 0x20000
#define VEHICLE_HEADING_RESPONSE_SNAP_MARGIN 0x30000
#define VEHICLE_HEADING_REFRESH_LIMIT 0xF0000
#define VEHICLE_HEADING_ACCELERATION 0x18000
#define VEHICLE_PLAYER_HEADING_ACCELERATION 0x13333
#define VEHICLE_HEADING_IMPULSE_SCALE 0x444
#define VEHICLE_COLLISION_TYPE_HARD 0x407
#define VEHICLE_COLLISION_TYPE_SOFT 0x401
#define VEHICLE_COLLISION_SCALE 20
#define VEHICLE_COLLISION_SPEED_LIMIT 0x160000
#define VEHICLE_COLLISION_OVERRIDE_LIMIT 0x200000
#define VEHICLE_COLLISION_OVERRIDE_FLAG 0x00020000
#define VEHICLE_STATUS_PLAYER_FLAG 0x00000004
#define VEHICLE_STATUS_TRAFFIC_FLAG 0x00000008
#define VEHICLE_STATUS_REVERSE_FLAG 0x00001000
#define VEHICLE_STATUS_OVERRIDE_MASK 0x00000404
#define VEHICLE_REFRESH_PERIOD 0xA0
#define VEHICLE_SOUND_COLLISION 0x52
#define VEHICLE_SOUND_DISTANCE_MINIMUM 5
#define VEHICLE_SOUND_DISTANCE_MAXIMUM 15
#define VEHICLE_NEGATIVE_LANE_CENTER 3
#define VEHICLE_LANE_CENTER 4
#define VEHICLE_LANE_COUNT 8
#define VEHICLE_LANE_MAXIMUM 7
#define VEHICLE_LANE_POSITION_SCALE 51
#define VEHICLE_LANE_SPEED_DAMPING_DISTANCE_MINIMUM (-9)
#define VEHICLE_LANE_SPEED_DAMPING_DISTANCE_MAXIMUM 5
#define VEHICLE_LANE_SPEED_DAMPING_SCALE 253
#define VEHICLE_LANE_SPEED_DAMPING_SHIFT 8
#define VEHICLE_TRACK_WIDTH_SHIFT 13
#define VEHICLE_HEADING_RESPONSE_MULTIPLIER 9
#define VEHICLE_MATRIX_INTEGRATION_SHIFT 5
#define VEHICLE_MATRIX_DRIFT_SHIFT 9
#define VEHICLE_TRACK_NIBBLE_MASK 0x0F
#define VEHICLE_STEERING_RELEASE_LIMIT (-0x40000)
#define VEHICLE_LANE_SCALE_SHIFT 16
#define VEHICLE_TRACK_NIBBLE_SHIFT 4
#define VEHICLE_LATERAL_DAMPING_SCALE 0x14000
#define VEHICLE_FIXED_ONE 0x10000
#define VEHICLE_FIXED_NEGATIVE_ONE (-0x10000)
#define VEHICLE_TRACK_BOUNDARY_LANE_SCALE 0x50000
#define VEHICLE_TRACK_BOUNDARY_EDGE_MARGIN 0x28000
#define VEHICLE_TRACK_BOUNDARY_WIDTH_SCALE 0x2000
#define VEHICLE_TRACK_DIRECTION_COMPONENT_SCALE 0x400
#define VEHICLE_TRACK_DIRECTION_FULL_TURN 0x1000000
#define VEHICLE_TRACK_DIRECTION_HALF_TURN 0x800000
#define VEHICLE_TRACK_NEGATIVE_LANE_SHIFT 4
#define VEHICLE_TRACK_LANE_MASK 0xF

typedef struct VehicleTrackBoundaryReferenceState
{
  uint8 reserved_00[0x74];
  int32 lateral_reference;
} VehicleTrackBoundaryReferenceState;

extern void
rebuild_vehicle_track_frame(SimulationSource *source_);
extern void
reposition_vehicle_simulation(SimulationSource *source_, int32 activate_record_);
extern void
update_vehicle_dynamics(SimulationSource *source_, int32 control_state_);
extern void
request_sound_effect(int32 sound_id_);

void
reposition_vehicle_at_track_boundary(SimulationSource *source_, int32 use_positive_boundary_)
{
  const TrackSegment                        *record;
  const VehicleTrackBoundaryReferenceState *reference_state;
  const uint8                               *component_bytes;
  Vector3                                   displacement;
  int32                                     boundary_position;
  int32                                     direction_x;
  int32                                     position_correction;
  int32                                     projected_position;

  record          = &g_TRACK_SEGMENTS[source_->track_segment_index];
  reference_state = (const VehicleTrackBoundaryReferenceState *) source_->refresh_state;
  if(use_positive_boundary_ == 0)
    {
      boundary_position  = -((record->lane_boundary_offsets >> VEHICLE_TRACK_NEGATIVE_LANE_SHIFT) *
                             VEHICLE_TRACK_BOUNDARY_LANE_SCALE) -
                           VEHICLE_TRACK_BOUNDARY_EDGE_MARGIN;
      projected_position =
        (reference_state->lateral_reference -
         (record->negative_boundary_width * VEHICLE_TRACK_BOUNDARY_WIDTH_SCALE));
      if(boundary_position < projected_position)
        boundary_position = projected_position;
    }
  else
    {
      boundary_position  = ((record->lane_boundary_offsets & VEHICLE_TRACK_LANE_MASK) *
                            VEHICLE_TRACK_BOUNDARY_LANE_SCALE) +
                           VEHICLE_TRACK_BOUNDARY_EDGE_MARGIN;
      projected_position =
        ((record->positive_boundary_width * VEHICLE_TRACK_BOUNDARY_WIDTH_SCALE) -
         reference_state->lateral_reference);
      if(projected_position < boundary_position)
        boundary_position = projected_position;
    }

  displacement.x = source_->position.x - source_->track_reference_position.x;
  displacement.y = source_->position.y - source_->track_reference_position.y;
  displacement.z = source_->position.z - source_->track_reference_position.z;
  projected_position =
    ((displacement.x >> VEHICLE_FIXED8_SHIFT) *
     (source_->track_basis.m00 >> VEHICLE_FIXED8_SHIFT)) +
    ((displacement.y >> VEHICLE_FIXED8_SHIFT) *
     (source_->track_basis.m01 >> VEHICLE_FIXED8_SHIFT)) +
    ((displacement.z >> VEHICLE_FIXED8_SHIFT) * (source_->track_basis.m02 >> VEHICLE_FIXED8_SHIFT));
  position_correction = (boundary_position - projected_position) >> VEHICLE_FIXED8_SHIFT;

  source_->track_transform.m00 = source_->track_basis.m00;
  source_->track_transform.m01 = source_->track_basis.m01;
  source_->track_transform.m02 = source_->track_basis.m02;
  source_->track_transform.m10 = source_->track_basis.m10;
  source_->track_transform.m11 = source_->track_basis.m11;
  source_->track_transform.m12 = source_->track_basis.m12;
  source_->track_transform.m20 = source_->track_basis.m20;
  source_->track_transform.m21 = source_->track_basis.m21;
  source_->track_transform.m22 = source_->track_basis.m22;
  source_->heading_velocity    = 0;
  component_bytes = (const uint8 *) &record->heading;
  source_->direction.y =
    ((int32) (int16) ((((uint16) component_bytes[0]) << VEHICLE_BIG_ENDIAN_BYTE_SHIFT) |
                      (uint16) component_bytes[1]) *
     VEHICLE_TRACK_DIRECTION_COMPONENT_SCALE);
  component_bytes = (const uint8 *) &record->surface_incline;
  direction_x =
    ((int32) (int16) ((((uint16) component_bytes[0]) << VEHICLE_BIG_ENDIAN_BYTE_SHIFT) |
                      (uint16) component_bytes[1]) *
     VEHICLE_TRACK_DIRECTION_COMPONENT_SCALE);
  if(direction_x > VEHICLE_TRACK_DIRECTION_HALF_TURN)
    direction_x -= VEHICLE_TRACK_DIRECTION_FULL_TURN;
  else if(direction_x < -VEHICLE_TRACK_DIRECTION_HALF_TURN)
    direction_x += VEHICLE_TRACK_DIRECTION_FULL_TURN;
  source_->direction.x = -direction_x;
  component_bytes = (const uint8 *) &record->surface_bank;
  source_->direction.z =
    ((int32) (int16) ((((uint16) component_bytes[0]) << VEHICLE_BIG_ENDIAN_BYTE_SHIFT) |
                      (uint16) component_bytes[1]) *
     VEHICLE_TRACK_DIRECTION_COMPONENT_SCALE);

  source_->pitch_motion_scale        = 0;
  source_->longitudinal_acceleration = 0;
  source_->motion.x                  = 0;
  source_->motion.y                  = 0;
  source_->motion.z                  = 0;
  source_->position.x += position_correction * (source_->track_basis.m00 >> VEHICLE_FIXED8_SHIFT);
  source_->position.y += position_correction * (source_->track_basis.m01 >> VEHICLE_FIXED8_SHIFT);
  source_->position.z += position_correction * (source_->track_basis.m02 >> VEHICLE_FIXED8_SHIFT);
}

void
adjust_vehicle_lateral_velocity(SimulationSource *source_)
{
  Vector3 lateral_basis;
  Vector3 motion;
  Vector3 correction;
  int32   lateral_velocity;
  int32   correction_scale;

  motion.x        = source_->motion.x;
  motion.y        = 0;
  motion.z        = source_->motion.z;
  lateral_basis.x = -source_->track_basis.m00;
  lateral_basis.y = source_->track_basis.m01;
  lateral_basis.z = source_->track_basis.m02;

  lateral_velocity =
    ((lateral_basis.x >> VEHICLE_FIXED8_SHIFT) * (motion.x >> VEHICLE_FIXED8_SHIFT)) +
    ((lateral_basis.z >> VEHICLE_FIXED8_SHIFT) * (motion.z >> VEHICLE_FIXED8_SHIFT));
  correction_scale = multiply_fixed16(lateral_velocity, VEHICLE_LATERAL_DAMPING_SCALE);
  if(((source_->lateral_position > 0) && (correction_scale < 0)) ||
     ((source_->lateral_position < 0) && (correction_scale > 0)))
    correction_scale = 0;

  correction.x = multiply_fixed16(correction_scale, lateral_basis.x);
  correction.y = multiply_fixed16(correction_scale, lateral_basis.y);
  correction.z = multiply_fixed16(correction_scale, lateral_basis.z);
  motion.x -= correction.x;
  motion.y -= correction.y;
  motion.z -= correction.z;

  lateral_velocity =
    ((lateral_basis.x >> VEHICLE_FIXED8_SHIFT) * (motion.x >> VEHICLE_FIXED8_SHIFT)) +
    ((lateral_basis.z >> VEHICLE_FIXED8_SHIFT) * (motion.z >> VEHICLE_FIXED8_SHIFT));
  correction_scale = 0;
  if((source_->lateral_position > 0) && (lateral_velocity <= VEHICLE_FIXED_NEGATIVE_ONE))
    correction_scale = VEHICLE_FIXED_NEGATIVE_ONE;
  else if((source_->lateral_position < 0) && (lateral_velocity < VEHICLE_FIXED_ONE))
    correction_scale = VEHICLE_FIXED_ONE;

  if(correction_scale != 0)
    {
      motion.x += multiply_fixed16(correction_scale, lateral_basis.x);
      motion.y += multiply_fixed16(correction_scale, lateral_basis.y);
      motion.z += multiply_fixed16(correction_scale, lateral_basis.z);
    }

  source_->motion.x = motion.x;
  source_->motion.z = motion.z;
}

void
update_vehicle_simulation_transform(SimulationSource *source_)
{
  VehicleSimulationRefreshState *refresh_state;
  TrackSegment                  *segment;
  Matrix3x3                      rotation;
  Vector3                        displacement;
  Vector3                        track_normal;
  int32                          player_track_segment;
  int32                          control_state;
  int32                          adjusted_segment_index;
  int32                          track_distance;
  int32                          lane;
  int32                          negative_lane;
  int32                          positive_lane;
  int32                          heading_delta;
  int32                          acceleration;
  int32                          heading_distance;
  int32                          lateral_position;
  int32                          negative_limit;
  int32                          positive_limit;
  int32                          projection;
  int32                          projection_fixed8;
  int32                          signed_track_distance;

  refresh_state             = (VehicleSimulationRefreshState *) source_->refresh_state;
  control_state             = refresh_state->control_state;
  source_->transform_active = 1;
  player_track_segment      = G_ACTIVE_SIMULATION_SOURCE->simulation_source.track_segment_index;
  if(((source_->track_segment_index - player_track_segment) < VEHICLE_TRACK_MINIMUM_DISTANCE) ||
     ((source_->track_segment_index - player_track_segment) > VEHICLE_TRACK_NEAR_DISTANCE) ||
     (source_->track_segment_index < VEHICLE_TRACK_MINIMUM_SEGMENT))
    {
      if(((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) == 0) ||
         (source_->track_segment_index < VEHICLE_TRACK_MINIMUM_SEGMENT))
        reposition_vehicle_simulation(source_, 1);
    }

  adjusted_segment_index = source_->track_segment_index;
  if(adjust_track_segment_index(&source_->position, &adjusted_segment_index) != 0)
    {
      source_->track_segment_index = adjusted_segment_index;
      rebuild_vehicle_track_frame(source_);
      if((source_->status_flags & VEHICLE_STATUS_REVERSE_FLAG) == 0)
        {
          source_->track_transform.m00 = source_->track_basis.m00;
          source_->track_transform.m01 = source_->track_basis.m01;
          source_->track_transform.m02 = source_->track_basis.m02;
          source_->track_transform.m10 = source_->track_basis.m10;
          source_->track_transform.m11 = source_->track_basis.m11;
          source_->track_transform.m12 = source_->track_basis.m12;
          source_->track_transform.m20 = source_->track_basis.m20;
          source_->track_transform.m21 = source_->track_basis.m21;
          source_->track_transform.m22 = source_->track_basis.m22;
        }
      else
        {
          source_->track_transform.m00 = -source_->track_basis.m00;
          source_->track_transform.m01 = -source_->track_basis.m01;
          source_->track_transform.m02 = -source_->track_basis.m02;
          source_->track_transform.m10 = source_->track_basis.m10;
          source_->track_transform.m11 = source_->track_basis.m11;
          source_->track_transform.m12 = source_->track_basis.m12;
          source_->track_transform.m20 = -source_->track_basis.m20;
          source_->track_transform.m21 = -source_->track_basis.m21;
          source_->track_transform.m22 = -source_->track_basis.m22;
        }
      if(source_->heading_rotation != 0)
        {
          initialize_matrix3x3_rotation_y(&rotation, source_->heading_rotation);
          MulMat33Mat33_F16((frac16(*)[VEHICLE_TRACK_MATRIX_DIMENSION]) & source_->track_transform,
                            (frac16(*)[VEHICLE_TRACK_MATRIX_DIMENSION]) & rotation,
                            (frac16(*)[VEHICLE_TRACK_MATRIX_DIMENSION]) & source_->track_transform);
        }
      refresh_state->control_state = 0;
    }

  if(source_->heading_rotation != 0)
    {
      source_->collision_impulse = 0;
      source_->collision_type    = 0;
      track_distance             = (source_->track_segment_index - player_track_segment);
      if((track_distance < VEHICLE_TRACK_MINIMUM_DISTANCE) ||
         (track_distance > VEHICLE_TRACK_NEAR_DISTANCE))
        {
          source_->heading_rotation = 0;
          source_->direction.y      = 0;
          source_->heading_velocity = 0;
        }
      else if((((source_->heading_rotation < 0) ?
                  -source_->heading_rotation :
                  source_->heading_rotation) > VEHICLE_HEADING_COLLISION_LIMIT) &&
              (((source_->minimum_track_speed < 0) ?
                  -source_->minimum_track_speed :
                  source_->minimum_track_speed) > VEHICLE_SPEED_COLLISION_LIMIT))
        {
          segment = &g_TRACK_SEGMENTS[source_->track_segment_index];
          lane =
            VEHICLE_LANE_CENTER +
            ((((source_->lateral_position >> VEHICLE_FIXED8_SHIFT) * VEHICLE_LANE_POSITION_SCALE) >>
              VEHICLE_LANE_SCALE_SHIFT));
          if(lane < 0)
            lane = 0;
          else if(lane >= VEHICLE_LANE_COUNT)
            lane = VEHICLE_LANE_MAXIMUM;
          negative_lane = (VEHICLE_NEGATIVE_LANE_CENTER -
                           (segment->lane_boundary_offsets >> VEHICLE_TRACK_NIBBLE_SHIFT));
          positive_lane =
            (VEHICLE_LANE_CENTER + (segment->lane_boundary_offsets & VEHICLE_TRACK_NIBBLE_MASK));
          if((lane == negative_lane) || (lane == positive_lane) || (lane < negative_lane) ||
             (lane > positive_lane))
            source_->collision_type = VEHICLE_COLLISION_TYPE_HARD;
          else
            source_->collision_type = VEHICLE_COLLISION_TYPE_SOFT;
          heading_delta = (source_->heading_rotation - source_->direction.y);
          source_->collision_impulse =
            (((heading_delta < 0) ? -heading_delta : heading_delta) * VEHICLE_COLLISION_SCALE);
          if((((source_->heading_rotation < 0) ? -source_->heading_rotation :
                                                 source_->heading_rotation) >
              (((source_->status_flags & VEHICLE_STATUS_OVERRIDE_MASK) == 0) ?
                 VEHICLE_COLLISION_SPEED_LIMIT :
                 VEHICLE_COLLISION_OVERRIDE_LIMIT)) &&
             ((source_->status_flags & VEHICLE_COLLISION_OVERRIDE_FLAG) == 0))
            {
              prepare_simulation_collision_transform((SimulationSource *) source_->runtime_record);
              if((source_->status_flags & VEHICLE_STATUS_TRAFFIC_FLAG) != 0)
                {
                  if((track_distance > -VEHICLE_SOUND_DISTANCE_MINIMUM) &&
                     (track_distance < VEHICLE_SOUND_DISTANCE_MAXIMUM))
                    request_sound_effect(VEHICLE_SOUND_COLLISION);
                  clear_vehicle_simulation_contact_flags(0);
                }
            }
        }
    }
  else
    {
      source_->collision_impulse = 0;
      source_->collision_type    = 0;
    }

  if((source_->heading_rotation == source_->direction.y) ||
     (((source_->heading_rotation < 0) ? -source_->heading_rotation : source_->heading_rotation) >=
      VEHICLE_HEADING_REFRESH_LIMIT))
    refresh_state->activation_period = VEHICLE_REFRESH_PERIOD;
  else if(refresh_state->activation_period != 0)
    refresh_state->activation_period--;

  if((((source_->heading_velocity < 0) ? -source_->heading_velocity : source_->heading_velocity) >
      VEHICLE_HEADING_LIMIT) ||
     (((source_->heading_rotation < 0) ? -source_->heading_rotation : source_->heading_rotation) >
      VEHICLE_HEADING_LIMIT))
    {
      source_->heading_rotation = 0;
      source_->heading_velocity = 0;
    }
  heading_delta = MulSF16(source_->heading_velocity, VEHICLE_HEADING_IMPULSE_SCALE);
  source_->heading_rotation += heading_delta;
  initialize_matrix3x3_rotation_y(&rotation, heading_delta);
  MulMat33Mat33_F16((frac16(*)[VEHICLE_TRACK_MATRIX_DIMENSION]) & source_->track_transform,
                    (frac16(*)[VEHICLE_TRACK_MATRIX_DIMENSION]) & rotation,
                    (frac16(*)[VEHICLE_TRACK_MATRIX_DIMENSION]) & source_->track_transform);

  heading_distance = (source_->heading_rotation - source_->direction.y);
  acceleration     = VEHICLE_HEADING_ACCELERATION;
  if((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) == 0)
    acceleration = VEHICLE_PLAYER_HEADING_ACCELERATION;
  if(heading_distance < 0)
    {
      if(source_->heading_velocity < 0)
        source_->heading_velocity += acceleration;
      else if(((source_->minimum_track_speed < 0) ?
                 -source_->minimum_track_speed :
                 source_->minimum_track_speed) > VEHICLE_SPEED_UPDATE_LIMIT)
        {
          source_->heading_velocity =
            -(source_->heading_rotation * VEHICLE_HEADING_RESPONSE_MULTIPLIER);
          if((source_->direction.y - VEHICLE_HEADING_RESPONSE_SNAP_MARGIN) <
             source_->heading_rotation)
            {
              source_->heading_rotation = source_->direction.y;
              source_->heading_velocity = 0;
            }
        }
    }
  else if(heading_distance > 0)
    {
      if(source_->heading_velocity > 0)
        source_->heading_velocity -= acceleration;
      else if(((source_->minimum_track_speed < 0) ?
                 -source_->minimum_track_speed :
                 source_->minimum_track_speed) > VEHICLE_SPEED_UPDATE_LIMIT)
        {
          source_->heading_velocity =
            -(source_->heading_rotation * VEHICLE_HEADING_RESPONSE_MULTIPLIER);
          if((source_->direction.y + VEHICLE_HEADING_RESPONSE_SNAP_MARGIN) >
             source_->heading_rotation)
            {
              source_->heading_rotation = source_->direction.y;
              source_->heading_velocity = 0;
            }
        }
    }
  else if(((source_->heading_rotation < 0) ? -source_->heading_rotation :
                                             source_->heading_rotation) <
          (((source_->direction.y < 0) ? -source_->direction.y : source_->direction.y) +
           VEHICLE_HEADING_SNAP_MARGIN))
    {
      source_->heading_rotation = source_->direction.y;
      source_->heading_velocity = 0;
    }

  if(refresh_state->control_state == 0)
    update_vehicle_dynamics(source_, control_state);
  refresh_state->control_state -= 1;

  signed_track_distance = (player_track_segment - source_->track_segment_index);
  track_distance        = signed_track_distance;
  if(track_distance < 0)
    track_distance = -track_distance;
  segment          = &g_TRACK_SEGMENTS[source_->track_segment_index];
  lateral_position = source_->lateral_position;
  if(lateral_position < 0)
    lateral_position = -lateral_position;
  if(!((track_distance < VEHICLE_TRACK_NEAR_DISTANCE) &&
       ((lateral_position > (segment->midpoint_negative_width << VEHICLE_TRACK_WIDTH_SHIFT)) ||
        (lateral_position > (segment->midpoint_positive_width << VEHICLE_TRACK_WIDTH_SHIFT))) &&
       (is_vehicle_outside_track_bounds(source_) != 0)))
    {
      if(track_distance < VEHICLE_TRACK_NEAR_DISTANCE)
        {
          lane = VEHICLE_LANE_CENTER +
                 ((((source_->lateral_position >> VEHICLE_FIXED8_SHIFT) *
                    VEHICLE_LANE_POSITION_SCALE) >>
                   VEHICLE_LANE_SCALE_SHIFT));
          if(lane < 0)
            lane = 0;
          else if(lane >= VEHICLE_LANE_COUNT)
            lane = VEHICLE_LANE_MAXIMUM;
          negative_lane =
            (VEHICLE_NEGATIVE_LANE_CENTER -
             (segment->lane_boundary_offsets >> VEHICLE_TRACK_NIBBLE_SHIFT));
          positive_lane =
            (VEHICLE_LANE_CENTER +
             (segment->lane_boundary_offsets & VEHICLE_TRACK_NIBBLE_MASK));
          if(((lane == negative_lane) || (lane == positive_lane)) &&
             (signed_track_distance < VEHICLE_LANE_SPEED_DAMPING_DISTANCE_MAXIMUM) &&
             (signed_track_distance > VEHICLE_LANE_SPEED_DAMPING_DISTANCE_MINIMUM))
            {
              source_->minimum_track_speed =
                ((source_->minimum_track_speed >> VEHICLE_LANE_SPEED_DAMPING_SHIFT) *
                 VEHICLE_LANE_SPEED_DAMPING_SCALE);
            }
        }

      negative_limit = -(segment->negative_boundary_width << VEHICLE_TRACK_WIDTH_SHIFT);
      positive_limit = (segment->positive_boundary_width << VEHICLE_TRACK_WIDTH_SHIFT);
      if((source_->lateral_position < negative_limit) ||
         (source_->lateral_position > positive_limit))
        {
          if(track_distance < VEHICLE_TRACK_NEAR_DISTANCE)
            {
              if(source_->direction.z < VEHICLE_STEERING_RELEASE_LIMIT)
                {
                  source_->target_track_speed  = 0;
                  source_->minimum_track_speed = 0;
                  source_->motion.x            = 0;
                  source_->motion.y            = 0;
                  source_->motion.z            = 0;
                }
              else
                adjust_vehicle_lateral_velocity(source_);
            }
          else
            {
              source_->direction.y      = 0;
              source_->heading_rotation = 0;
            }
        }
    }

  source_->position.x -= ((source_->motion.x >> VEHICLE_MATRIX_INTEGRATION_SHIFT) +
                          (source_->motion.x >> VEHICLE_MATRIX_DRIFT_SHIFT));
  source_->position.y += ((source_->motion.y >> VEHICLE_MATRIX_INTEGRATION_SHIFT) +
                          (source_->motion.y >> VEHICLE_MATRIX_DRIFT_SHIFT));
  source_->position.z += ((source_->motion.z >> VEHICLE_MATRIX_INTEGRATION_SHIFT) +
                          (source_->motion.z >> VEHICLE_MATRIX_DRIFT_SHIFT));

  if((refresh_state->activation_period != 0) &&
     (refresh_state->activation_period != VEHICLE_REFRESH_PERIOD))
    {
      displacement.x    = (source_->position.x - source_->track_reference_position.x);
      displacement.y    = (source_->position.y - source_->track_reference_position.y);
      displacement.z    = (source_->position.z - source_->track_reference_position.z);
      track_normal.x    = source_->track_basis.m10;
      track_normal.y    = source_->track_basis.m11;
      track_normal.z    = source_->track_basis.m12;
      projection        = Dot3_F16((frac16 *) &displacement, (frac16 *) &track_normal);
      projection_fixed8 = (projection >> VEHICLE_FIXED8_SHIFT);
      source_->position.x -= ((track_normal.x >> VEHICLE_FIXED8_SHIFT) * projection_fixed8);
      source_->position.y -= ((track_normal.y >> VEHICLE_FIXED8_SHIFT) * projection_fixed8);
      source_->position.z -= ((track_normal.z >> VEHICLE_FIXED8_SHIFT) * projection_fixed8);
    }
}
