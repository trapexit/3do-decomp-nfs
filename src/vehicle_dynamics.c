#include "fixed_math.h"
#include "track_position.h"
#include "vehicle_simulation_state.h"
#include "vehicle_traffic_randomization.h"

#include "operamath.h"

#define VEHICLE_FIXED8_SHIFT 8
#define VEHICLE_FIXED16_SHIFT 16
#define VEHICLE_SPEED_TABLE_COUNT 100
#define VEHICLE_SPEED_TABLE_MAXIMUM_INDEX 99
#define VEHICLE_SPEED_TABLE_FIRST_VALUE 0x640000
#define VEHICLE_NONPLAYER_SPEED_RATE 0xB333
#define VEHICLE_STANDARD_UPDATE_COUNT 8
#define VEHICLE_PLAYER_UPDATE_COUNT 4
#define VEHICLE_TRAFFIC_UPDATE_COUNT 6
#define VEHICLE_STANDARD_STEERING_RATE 0x2000
#define VEHICLE_PLAYER_STEERING_RATE 0x4000
#define VEHICLE_TRAFFIC_STEERING_RATE 0x2AAA
#define VEHICLE_MINIMUM_DRIVE_SPEED 0x70001
#define VEHICLE_STANDARD_STEERING_DRAG 0x1745
#define VEHICLE_PLAYER_STEERING_DRAG 0x1249
#define VEHICLE_TRACK_NEAR_DISTANCE 54
#define VEHICLE_TRACK_WIDTH_SHIFT 13
#define VEHICLE_REFRESH_DELAY_RESET (-20)
#define VEHICLE_TRAFFIC_BRAKE_INITIAL 17
#define VEHICLE_TRAFFIC_BRAKE_STEP 4
#define VEHICLE_HEADING_VECTOR_LIMIT 0x20000
#define VEHICLE_HEADING_FORCE_LIMIT 0x80000
#define VEHICLE_LATERAL_FORCE_DEFAULT 0x8000
#define VEHICLE_LATERAL_FORCE_SMOOTH_SHIFT 4
#define VEHICLE_LATERAL_FORCE_COMPARE_SHIFT 5
#define VEHICLE_LATERAL_FORCE_SCALE_SHIFT 5
#define VEHICLE_NEGATIVE_LANE_CENTER 3
#define VEHICLE_LANE_CENTER 4
#define VEHICLE_LANE_COUNT 8
#define VEHICLE_LANE_MAXIMUM 7
#define VEHICLE_LANE_POSITION_SCALE 51
#define VEHICLE_LANE_SCALE_SHIFT 16
#define VEHICLE_TRACK_NIBBLE_SHIFT 4
#define VEHICLE_TRACK_NIBBLE_MASK 0x0F
#define VEHICLE_RESISTANCE_SPEED_DIVISOR 5
#define VEHICLE_RESISTANCE_PROFILE_MAXIMUM 11
#define VEHICLE_RESISTANCE_PROFILE_ZERO 0x10000
#define VEHICLE_RESISTANCE_PROFILE_ONE 0x18CCC
#define VEHICLE_RESISTANCE_PROFILE_MIDDLE 0x15999
#define VEHICLE_RESISTANCE_PROFILE_HIGH 0x11999
#define VEHICLE_RESISTANCE_PROFILE_SEVEN 0xCCCC
#define VEHICLE_RESISTANCE_PROFILE_NINE 0x8000
#define VEHICLE_RESISTANCE_PROFILE_MAX 0x4CCC
#define VEHICLE_STATUS_PLAYER_FLAG 0x00000004
#define VEHICLE_STATUS_TRAFFIC_FLAG 0x00000008
#define VEHICLE_STATUS_REVERSE_FLAG 0x00001000
#define VEHICLE_REFRESH_PERIOD 0xA0
#define VEHICLE_LATERAL_ACCELERATION_SCALE_SHIFT 2
#define VEHICLE_NONPLAYER_LATERAL_ACCELERATION_LIMIT 0x33333

typedef struct VehicleRuntimeCallbackRecord
{
  uint8 reserved_000[0x4C4];
  void  (*update_callback)(SimulationSource *source_);
} VehicleRuntimeCallbackRecord;

typedef enum VehicleResistanceIndex
{
  VEHICLE_RESISTANCE_INDEX_TWO    = 2,
  VEHICLE_RESISTANCE_INDEX_THREE  = 3,
  VEHICLE_RESISTANCE_INDEX_FOUR   = 4,
  VEHICLE_RESISTANCE_INDEX_FIVE   = 5,
  VEHICLE_RESISTANCE_INDEX_SIX    = 6,
  VEHICLE_RESISTANCE_INDEX_SEVEN  = 7,
  VEHICLE_RESISTANCE_INDEX_EIGHT  = 8,
  VEHICLE_RESISTANCE_INDEX_NINE   = 9,
  VEHICLE_RESISTANCE_INDEX_TEN    = 10,
  VEHICLE_RESISTANCE_INDEX_ELEVEN = 11
} VehicleResistanceIndex;

int32 g_PLAYER_VEHICLE_ACCELERATION_RATE;

static int32
_absolute_value(int32 value_);

static int32
_clamp_vehicle_lane(int32 lane_);

static int32
_select_vehicle_resistance_profile(int32 speed_);

static int32
_is_vehicle_acceleration_allowed(SimulationSource *source_);

static void
_update_vehicle_speed(SimulationSource *source_,
                      int32             update_scale_,
                      int32             speed_rate_,
                      int32             speed_gate_,
                      int32             acceleration_rate_,
                      int32             acceleration_allowed_);

static Vector3
_select_vehicle_forward_vector(const SimulationSource *source_);

static void
_update_vehicle_lateral_force(SimulationSource *source_,
                              int32             update_scale_,
                              int32             steering_rate_,
                              int32             speed_magnitude_);

static void
_project_vehicle_position_to_track(SimulationSource *source_);

int32
limit_vehicle_lateral_acceleration(SimulationSource *source_, int32 direction_, int32 acceleration_)
{
  int32 limit;
  int32 scaled_acceleration;

  if((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) != 0)
    limit = g_VEHICLE_SPAWN_CONFIGURATION.maximum_track_position_adjustment;
  else
    limit = VEHICLE_NONPLAYER_LATERAL_ACCELERATION_LIMIT;

  scaled_acceleration = acceleration_ >> VEHICLE_LATERAL_ACCELERATION_SCALE_SHIFT;
  if(scaled_acceleration < limit)
    {
      if(direction_ < 1)
        scaled_acceleration = -scaled_acceleration;
      return scaled_acceleration;
    }

  if(direction_ < 1)
    return -limit;
  return limit;
}

void
update_vehicle_dynamics(SimulationSource *source_, int32 control_state_)
{
  VehicleSimulationRefreshState *refresh_state;
  Vector3                        forward_vector;
  Vector3                        acceleration_vector;
  int32                          speed_index;
  int32                          speed_rate;
  int32                          speed_gate;
  int32                          acceleration_rate;
  int32                          steering_rate;
  int32                          update_count;
  int32                          update_scale;
  int32                          acceleration_allowed;
  int32                          steering_drag;
  int32                          speed_magnitude;
  int32                          resistance_profile;

  refresh_state = (VehicleSimulationRefreshState *) source_->refresh_state;
  speed_index   = (_absolute_value(source_->minimum_track_speed >> VEHICLE_FIXED16_SHIFT) - 1);
  if(speed_index < 0)
    speed_index = 0;
  else if(speed_index >= VEHICLE_SPEED_TABLE_COUNT)
    speed_index = VEHICLE_SPEED_TABLE_MAXIMUM_INDEX;

  speed_rate = source_->speed_acceleration_table[speed_index];
  if(speed_index == 0)
    speed_gate = VEHICLE_SPEED_TABLE_FIRST_VALUE;
  else
    speed_gate = source_->speed_acceleration_table[speed_index - 1];

  if((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) != 0)
    acceleration_rate = g_PLAYER_VEHICLE_ACCELERATION_RATE;
  else
    acceleration_rate = VEHICLE_NONPLAYER_SPEED_RATE;

  if((source_->status_flags & VEHICLE_STATUS_TRAFFIC_FLAG) != 0)
    {
      update_count  = VEHICLE_TRAFFIC_UPDATE_COUNT;
      steering_rate = VEHICLE_TRAFFIC_STEERING_RATE;
    }
  else if((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) != 0)
    {
      update_count  = VEHICLE_PLAYER_UPDATE_COUNT;
      steering_rate = VEHICLE_PLAYER_STEERING_RATE;
    }
  else
    {
      update_count  = VEHICLE_STANDARD_UPDATE_COUNT;
      steering_rate = VEHICLE_STANDARD_STEERING_RATE;
    }

  update_scale  = (update_count - control_state_);
  steering_drag = 0;
  if((source_->heading_rotation != source_->direction.y) &&
     (source_->minimum_track_speed > VEHICLE_MINIMUM_DRIVE_SPEED))
    {
      steering_drag = _absolute_value(source_->heading_rotation - source_->direction.y);
      steering_drag = ((steering_drag >> VEHICLE_FIXED16_SHIFT) *
                       (((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) != 0) ?
                          VEHICLE_PLAYER_STEERING_DRAG :
                          VEHICLE_STANDARD_STEERING_DRAG));
    }

  acceleration_allowed = _is_vehicle_acceleration_allowed(source_);
  if(refresh_state->pending_state > 0)
    {
      refresh_state->pending_state -= 1;
      if(refresh_state->pending_state == 0)
        refresh_state->pending_state = VEHICLE_REFRESH_DELAY_RESET;
    }
  else if(refresh_state->pending_state < 0)
    refresh_state->pending_state += 1;

  _update_vehicle_speed(source_,
                        update_scale,
                        speed_rate,
                        speed_gate,
                        acceleration_rate,
                        acceleration_allowed);
  if(steering_drag != 0)
    {
      speed_magnitude = _absolute_value(source_->minimum_track_speed);
      if(speed_magnitude < steering_drag)
        source_->minimum_track_speed = 0;
      else if(source_->minimum_track_speed < 0)
        source_->minimum_track_speed += steering_drag;
      else
        source_->minimum_track_speed -= steering_drag;
    }

  forward_vector   = _select_vehicle_forward_vector(source_);
  speed_magnitude  = _absolute_value(source_->minimum_track_speed);
  forward_vector.x = ((forward_vector.x >> VEHICLE_FIXED8_SHIFT) *
                      (source_->minimum_track_speed >> VEHICLE_FIXED8_SHIFT));
  forward_vector.y = ((forward_vector.y >> VEHICLE_FIXED8_SHIFT) *
                      (source_->minimum_track_speed >> VEHICLE_FIXED8_SHIFT));
  forward_vector.z = ((forward_vector.z >> VEHICLE_FIXED8_SHIFT) *
                      (source_->minimum_track_speed >> VEHICLE_FIXED8_SHIFT));

  _update_vehicle_lateral_force(source_, update_scale, steering_rate, speed_magnitude);
  acceleration_vector.x =
    MulSF16(source_->longitudinal_acceleration, source_->track_basis.m00);
  acceleration_vector.y =
    MulSF16(source_->longitudinal_acceleration, source_->track_basis.m01);
  acceleration_vector.z =
    MulSF16(source_->longitudinal_acceleration, source_->track_basis.m02);
  if((source_->status_flags & VEHICLE_STATUS_REVERSE_FLAG) == 0)
    {
      resistance_profile = _select_vehicle_resistance_profile(speed_magnitude);
      source_->direction.y =
        MulSF16(source_->longitudinal_acceleration, resistance_profile);
    }
  else
    source_->direction.y = 0;

  forward_vector.x += acceleration_vector.x;
  forward_vector.y += acceleration_vector.y;
  forward_vector.z += acceleration_vector.z;
  _project_vehicle_position_to_track(source_);
  if((refresh_state->activation_period == 0) ||
     (refresh_state->activation_period == VEHICLE_REFRESH_PERIOD))
    {
      source_->motion   = forward_vector;
      source_->motion.x = -source_->motion.x;
    }
  else
    source_->motion.y = forward_vector.y;
  refresh_state->control_state = update_count;
}

static int32
_absolute_value(int32 value_)
{
  if(value_ < 0)
    return -value_;

  return value_;
}

static int32
_clamp_vehicle_lane(int32 lane_)
{
  if(lane_ < 0)
    return 0;
  if(lane_ >= VEHICLE_LANE_COUNT)
    return VEHICLE_LANE_MAXIMUM;

  return lane_;
}

static int32
_select_vehicle_resistance_profile(int32 speed_)
{
  int32 profile;

  profile = ((speed_ >> VEHICLE_FIXED16_SHIFT) / VEHICLE_RESISTANCE_SPEED_DIVISOR);
  if(profile > VEHICLE_RESISTANCE_PROFILE_MAXIMUM)
    profile = VEHICLE_RESISTANCE_PROFILE_MAXIMUM;
  switch(profile)
    {
      case 0:
        return VEHICLE_RESISTANCE_PROFILE_ZERO;
      case 1:
        return VEHICLE_RESISTANCE_PROFILE_ONE;
      case VEHICLE_RESISTANCE_INDEX_TWO:
      case VEHICLE_RESISTANCE_INDEX_THREE:
        return VEHICLE_RESISTANCE_PROFILE_MIDDLE;
      case VEHICLE_RESISTANCE_INDEX_FOUR:
      case VEHICLE_RESISTANCE_INDEX_FIVE:
        return VEHICLE_RESISTANCE_PROFILE_HIGH;
      case VEHICLE_RESISTANCE_INDEX_SIX:
      case VEHICLE_RESISTANCE_INDEX_SEVEN:
        return VEHICLE_RESISTANCE_PROFILE_SEVEN;
      case VEHICLE_RESISTANCE_INDEX_EIGHT:
      case VEHICLE_RESISTANCE_INDEX_NINE:
        return VEHICLE_RESISTANCE_PROFILE_NINE;
      case VEHICLE_RESISTANCE_INDEX_TEN:
      case VEHICLE_RESISTANCE_INDEX_ELEVEN:
        return VEHICLE_RESISTANCE_PROFILE_MAX;
      default:
        return 0;
    }
}

static int32
_is_vehicle_acceleration_allowed(SimulationSource *source_)
{
  VehicleSimulationRefreshState *refresh_state;
  TrackSegment                  *segment;
  int32                          track_distance;
  int32                          negative_limit;
  int32                          positive_limit;

  refresh_state = (VehicleSimulationRefreshState *) source_->refresh_state;
  track_distance =
    _absolute_value(G_ACTIVE_SIMULATION_SOURCE->simulation_source.track_segment_index -
                    source_->track_segment_index);
  if(track_distance >= VEHICLE_TRACK_NEAR_DISTANCE)
    return 1;

  segment        = &g_TRACK_SEGMENTS[source_->track_segment_index];
  negative_limit = -(segment->negative_boundary_width << VEHICLE_TRACK_WIDTH_SHIFT);
  positive_limit = (segment->positive_boundary_width << VEHICLE_TRACK_WIDTH_SHIFT);
  if((source_->lateral_position < negative_limit) || (source_->lateral_position > positive_limit))
    return 1;

  return (refresh_state->pending_state <= 0);
}

static void
_update_vehicle_speed(SimulationSource *source_,
                      int32             update_scale_,
                      int32             speed_rate_,
                      int32             speed_gate_,
                      int32             acceleration_rate_,
                      int32             acceleration_allowed_)
{
  VehicleRuntimeCallbackRecord *record;
  int32                         current_speed;
  int32                         target_speed;

  current_speed = source_->minimum_track_speed;
  target_speed  = source_->target_track_speed;
  if((source_->status_flags & VEHICLE_STATUS_REVERSE_FLAG) != 0)
    {
      if(target_speed > current_speed)
        {
          current_speed += (update_scale_ * acceleration_rate_);
          if(current_speed > target_speed)
            current_speed = target_speed;
        }
      else if((speed_gate_ == 0) || (acceleration_allowed_ != 0))
        {
          current_speed -= (update_scale_ * speed_rate_);
          if(current_speed < target_speed)
            current_speed = target_speed;
        }
    }
  else if(target_speed < current_speed)
    {
      current_speed -= (update_scale_ * acceleration_rate_);
      if(current_speed < target_speed)
        current_speed = target_speed;
      if((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) != 0)
        {
          record = (VehicleRuntimeCallbackRecord *) source_->runtime_record;
          if(record->update_callback == update_vehicle_simulation_transform)
            source_->traffic_brake_indicator = VEHICLE_TRAFFIC_BRAKE_INITIAL;
        }
    }
  else if((speed_gate_ == 0) || (acceleration_allowed_ != 0))
    {
      current_speed += (update_scale_ * speed_rate_);
      if(current_speed > target_speed)
        current_speed = target_speed;
      if((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) != 0)
        {
          source_->traffic_brake_indicator -= VEHICLE_TRAFFIC_BRAKE_STEP;
          if(source_->traffic_brake_indicator < 0)
            source_->traffic_brake_indicator = 0;
        }
    }
  source_->minimum_track_speed = current_speed;
}

static Vector3
_select_vehicle_forward_vector(const SimulationSource *source_)
{
  const VehicleSimulationRefreshState *refresh_state;
  Vector3                              forward_vector;

  refresh_state    = (const VehicleSimulationRefreshState *) source_->refresh_state;
  forward_vector.x = source_->track_basis.m20;
  forward_vector.y = source_->track_basis.m21;
  forward_vector.z = source_->track_basis.m22;
  if(((refresh_state->activation_period == 0) ||
      (refresh_state->activation_period == VEHICLE_REFRESH_PERIOD)) &&
     (_absolute_value(source_->heading_rotation - source_->direction.y) >=
      VEHICLE_HEADING_VECTOR_LIMIT))
    {
      forward_vector.x = source_->track_transform.m20;
      forward_vector.y = source_->track_transform.m21;
      forward_vector.z = source_->track_transform.m22;
      if((source_->status_flags & VEHICLE_STATUS_REVERSE_FLAG) != 0)
        {
          forward_vector.x = -forward_vector.x;
          forward_vector.y = -forward_vector.y;
          forward_vector.z = -forward_vector.z;
        }
    }
  return forward_vector;
}

static void
_update_vehicle_lateral_force(SimulationSource *source_,
                              int32             update_scale_,
                              int32             steering_rate_,
                              int32             speed_magnitude_)
{
  TrackSegment *segment;
  int32         lateral_delta;
  int32         lane;
  int32         negative_lane;
  int32         positive_lane;
  int32         desired_force;
  int32         force_step;

  lateral_delta = (source_->track_position_offset - source_->lateral_position);
  segment       = &g_TRACK_SEGMENTS[source_->track_segment_index];
  lane          = _clamp_vehicle_lane(
    VEHICLE_LANE_CENTER +
    ((((source_->lateral_position >> VEHICLE_FIXED8_SHIFT) * VEHICLE_LANE_POSITION_SCALE) >>
      VEHICLE_LANE_SCALE_SHIFT)));
  negative_lane =
    (VEHICLE_NEGATIVE_LANE_CENTER - (segment->lane_boundary_offsets >> VEHICLE_TRACK_NIBBLE_SHIFT));
  positive_lane =
    (VEHICLE_LANE_CENTER + (segment->lane_boundary_offsets & VEHICLE_TRACK_NIBBLE_MASK));
  if((lane != negative_lane) && (lane != positive_lane) &&
     ((lateral_delta == 0) || (_absolute_value(source_->heading_rotation - source_->direction.y) >=
                               VEHICLE_HEADING_FORCE_LIMIT)))
    return;

  desired_force = limit_vehicle_lateral_acceleration(source_, lateral_delta, speed_magnitude_);
  if(_absolute_value(lateral_delta) <
     (_absolute_value(update_scale_ * desired_force) >> VEHICLE_LATERAL_FORCE_COMPARE_SHIFT))
    desired_force =
      (((steering_rate_ >> VEHICLE_FIXED8_SHIFT) * (lateral_delta >> VEHICLE_FIXED8_SHIFT))
       << VEHICLE_LATERAL_FORCE_SCALE_SHIFT);

  if(source_->longitudinal_acceleration < desired_force)
    {
      if(source_->longitudinal_acceleration <= 0)
        force_step = VEHICLE_LATERAL_FORCE_DEFAULT;
      else
        force_step = (_absolute_value(desired_force) >> VEHICLE_LATERAL_FORCE_SMOOTH_SHIFT);
      source_->longitudinal_acceleration += (update_scale_ * force_step);
      if(source_->longitudinal_acceleration > desired_force)
        source_->longitudinal_acceleration = desired_force;
    }
  else if(source_->longitudinal_acceleration > desired_force)
    {
      if(source_->longitudinal_acceleration >= 0)
        force_step = VEHICLE_LATERAL_FORCE_DEFAULT;
      else
        force_step = (_absolute_value(desired_force) >> VEHICLE_LATERAL_FORCE_SMOOTH_SHIFT);
      source_->longitudinal_acceleration -= (update_scale_ * force_step);
      if(source_->longitudinal_acceleration < desired_force)
        source_->longitudinal_acceleration = desired_force;
    }
}

static void
_project_vehicle_position_to_track(SimulationSource *source_)
{
  Vector3 displacement;
  Vector3 track_normal;
  int32   projection;
  int32   projection_fixed8;

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
