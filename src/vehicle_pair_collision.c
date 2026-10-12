#include "vehicle_pair_collision.h"

#include "matrix_collision.h"
#include "player_vehicle_state.h"
#include "road_object_table.h"
#include "signal_status.h"
#include "simulation_snapshot.h"
#include "sound_spatial.h"
#include "track_position.h"
#include "vehicle_drive_statistics.h"
#include "vehicle_motion_projection.h"
#include "vehicle_parameter_loader.h"
#include "vehicle_simulation_state.h"
#include "vehicle_traffic_lane.h"

#include "operamath.h"

#define VEHICLE_COLLISION_SOUND_CODE_COUNT 16
#define VEHICLE_COLLISION_STRONG_SOUND_OFFSET 8
#define VEHICLE_COLLISION_PLAYER_SOUND_SHIFT 2
#define VEHICLE_COLLISION_SECONDARY_SOUND_SHIFT 1
#define VEHICLE_COLLISION_PROJECTION_LIMIT 0x100000
#define VEHICLE_COLLISION_MOTION_LIMIT 0x8000
#define VEHICLE_COLLISION_CONTACT_COUNTDOWN 400
#define VEHICLE_COLLISION_TRACKED_STATE_INDEX 2
#define VEHICLE_COLLISION_STATISTICS_INCREMENT 1
#define VEHICLE_COLLISION_STRONG_STATISTICS_INCREMENT 50
#define VEHICLE_COLLISION_TRACK_SOURCE_OFFSET 0x0C
#define VEHICLE_COLLISION_SOUND_CLASS 2
#define VEHICLE_COLLISION_SOUND_VARIANT 2
#define VEHICLE_COLLISION_ANONYMOUS_SOUND_ID -1
#define VEHICLE_COLLISION_SOUND_COOLDOWN_INCREMENT 0x8000
#define VEHICLE_COLLISION_SOUND_COUNTDOWN 10
#define VEHICLE_COLLISION_SIMULATION_MODE 2
#define VEHICLE_COLLISION_MODE_TRANSITION_COOLDOWN 60
#define VEHICLE_COLLISION_SOUND_TIMER_RESET 0x6666
#define VEHICLE_COLLISION_TRAFFIC_UPDATE_PERIOD 4
#define VEHICLE_COLLISION_TRACKING_STATUS_FLAG 0x10000U
#define VEHICLE_COLLISION_TRACKING_ANGLE_MASK 0xFFFFFF
#define VEHICLE_COLLISION_TRACKING_HALF_TURN 0x800000
#define VEHICLE_COLLISION_TRACKING_FULL_TURN 0x1000000
#define VEHICLE_COLLISION_TRACKING_ANGLE_LIMIT 0x140000
#define VEHICLE_COLLISION_ROAD_WIDTH_SCALE 5
#define VEHICLE_COLLISION_ROAD_WIDTH_MASK 0x0F
#define VEHICLE_COLLISION_NEGATIVE_ROAD_WIDTH_SHIFT 4
#define VEHICLE_COLLISION_FIXED16_ONE 0x10000
#define VEHICLE_COLLISION_OUTER_TRACK_RANGE 39
#define VEHICLE_COLLISION_INNER_TRACK_RANGE 2
#define VEHICLE_COLLISION_OUTER_RUNTIME_FLAGS 6U
#define VEHICLE_COLLISION_INNER_RUNTIME_FLAGS 5U
#define VEHICLE_COLLISION_IMPULSE_FIXED8_SHIFT 8
#define VEHICLE_COLLISION_ANGULAR_DAMPING_SCALE 89
#define VEHICLE_COLLISION_BASIS_ROW_ZERO_SCALE -986
#define VEHICLE_COLLISION_BASIS_ROW_TWO_SCALE -474
#define VEHICLE_COLLISION_UPWARD_VELOCITY_SCALE 1459

#define SCALE_VEHICLE_COLLISION_COMPONENT(value_, scale_)                                          \
  ((int32) ((uint32) ((value_) >> VEHICLE_COLLISION_IMPULSE_FIXED8_SHIFT) * (uint32) (scale_)))
#define ADD_VEHICLE_COLLISION_COMPONENT(value_, addition_)                                         \
  ((int32) ((uint32) (value_) + (uint32) (addition_)))

int32 g_VEHICLE_COLLISION_SOUND_TIME;
int32 g_VEHICLE_COLLISION_STATISTICS_SUPPRESSED;
int32 g_VEHICLE_COLLISION_SCORE;
int32 g_VEHICLE_COLLISION_SOUND_CODES[VEHICLE_COLLISION_SOUND_CODE_COUNT] = {
  0x14, 0x1E, 0x20, 0x22, 0x3C, 0x46, 0x50, 0, 0x25, 0x26, 0x26, 0x27, 0x5C, 0x5F, 0x62, 0
};

typedef char
  VehicleCollisionSoundCodesSize[(sizeof(g_VEHICLE_COLLISION_SOUND_CODES) == 0x40) ? 1 : -1];

int32
process_vehicle_pair_collision(VehicleRuntimeObjectState *first_vehicle_,
                               VehicleRuntimeObjectState *second_vehicle_)
{
  const SimulationSource *sound_source;
  int32                   pan;
  int32                   distance;

  if(first_vehicle_->render_update_callback != (void *) update_player_vehicle_state)
    {
      rebuild_simulation_transform(&((VehicleSimulationState *) first_vehicle_)->simulation_source);
    }
  if(second_vehicle_->render_update_callback != (void *) update_player_vehicle_state)
    {
      rebuild_simulation_transform(
        &((VehicleSimulationState *) second_vehicle_)->simulation_source);
    }

  if(resolve_vehicle_pair_collision(first_vehicle_, second_vehicle_) == 0)
    return 0;

  if(g_VEHICLE_COLLISION_SOUND_TIME >= g_RUNTIME_STATE.collision_sound_cooldown_time)
    {
      sound_source = &((const VehicleSimulationState *) first_vehicle_)->simulation_source;
      if((void *) first_vehicle_ == (void *) G_ACTIVE_SIMULATION_SOURCE)
        {
          sound_source = &((const VehicleSimulationState *) second_vehicle_)->simulation_source;
        }
      calculate_spatial_sound_parameters(&sound_source->position, &pan, &distance);
      play_spatial_sound_effect_scaled(VEHICLE_COLLISION_ANONYMOUS_SOUND_ID,
                                       VEHICLE_COLLISION_SOUND_CLASS,
                                       VEHICLE_COLLISION_SOUND_VARIANT,
                                       g_VEHICLE_COLLISION_SOUND_TIME,
                                       distance,
                                       pan);
      g_RUNTIME_STATE.collision_sound_cooldown_time =
        g_VEHICLE_COLLISION_SOUND_TIME + VEHICLE_COLLISION_SOUND_COOLDOWN_INCREMENT;
      g_RUNTIME_STATE.collision_sound_countdown = VEHICLE_COLLISION_SOUND_COUNTDOWN;
    }

  if(g_RUNTIME_STATE.collision_simulation_enabled != 0)
    {
      if(g_SIMULATION_RENDER_STATE.mode == 0)
        {
          set_simulation_mode(&g_SIMULATION_RENDER_STATE, VEHICLE_COLLISION_SIMULATION_MODE);
        }
      g_SIMULATION_MODE_TRANSITION_COOLDOWN = VEHICLE_COLLISION_MODE_TRANSITION_COOLDOWN;
    }
  return 1;
}

void
update_vehicle_runtime_interactions(DoublyLinkedListStorage *storage_)
{
  VehicleSimulationState    *active_vehicle;
  VehicleSimulationState    *tracking_vehicle;
  VehicleSimulationState    *outer_vehicle;
  VehicleSimulationState    *inner_vehicle;
  VehicleRuntimeObjectState *active_runtime;
  VehicleRuntimeObjectState *outer_runtime;
  VehicleRuntimeObjectState *inner_runtime;
  SimulationSource          *active_source;
  SimulationSource          *outer_source;
  SimulationSource          *inner_source;
  DoublyLinkedNode          *outer_node;
  DoublyLinkedNode          *inner_node;
  const TrackSegment        *segment;
  int32                      angle;
  int32                      lateral_limit;
  int32                      collision_limit;
  int32                      difference_x;
  int32                      difference_z;

  if(g_RUNTIME_STATE.collision_sound_countdown == 0)
    {
      g_RUNTIME_STATE.collision_sound_cooldown_time = VEHICLE_COLLISION_SOUND_TIMER_RESET;
    }
  else
    {
      g_RUNTIME_STATE.collision_sound_countdown--;
    }

  active_vehicle = (VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE;
  active_runtime = (VehicleRuntimeObjectState *) active_vehicle;
  active_source  = &active_vehicle->simulation_source;
  if((active_runtime->status_flags & VEHICLE_COLLISION_TRACKING_STATUS_FLAG) != 0)
    {
      tracking_vehicle = g_VEHICLE_SIMULATION_STATES[VEHICLE_COLLISION_TRACKED_STATE_INDEX];
      angle = Atan2F16(tracking_vehicle->simulation_source.position.z - active_source->position.z,
                       tracking_vehicle->simulation_source.position.x - active_source->position.x) -
              active_source->direction.y;
      angle &= VEHICLE_COLLISION_TRACKING_ANGLE_MASK;
      if(angle > VEHICLE_COLLISION_TRACKING_HALF_TURN)
        angle -= VEHICLE_COLLISION_TRACKING_FULL_TURN;
      active_source->heading_rotation = (active_source->heading_rotation >> 1) + (angle >> 1);
      if(active_source->heading_rotation > VEHICLE_COLLISION_TRACKING_ANGLE_LIMIT)
        {
          active_source->heading_rotation = VEHICLE_COLLISION_TRACKING_ANGLE_LIMIT;
        }
      else if(active_source->heading_rotation < -VEHICLE_COLLISION_TRACKING_ANGLE_LIMIT)
        {
          active_source->heading_rotation = -VEHICLE_COLLISION_TRACKING_ANGLE_LIMIT;
        }
      active_source->audio_pan_target = 0;
    }

  g_RUNTIME_STATE.traffic_coordination_countdown--;
  if(g_RUNTIME_STATE.traffic_coordination_countdown == 0)
    {
      coordinate_vehicle_traffic_lanes(&storage_->list);
      g_RUNTIME_STATE.traffic_coordination_countdown = VEHICLE_COLLISION_TRAFFIC_UPDATE_PERIOD;
    }

  outer_node = storage_->list.head_sentinel->next;
  if(outer_node->next == NULL)
    return;

  while(outer_node->next != NULL)
    {
      outer_vehicle = (VehicleSimulationState *) outer_node;
      outer_runtime = (VehicleRuntimeObjectState *) outer_node;
      outer_source  = &outer_vehicle->simulation_source;
      if(outer_vehicle == active_vehicle)
        {
          segment       = &g_TRACK_SEGMENTS[outer_source->track_segment_index];
          lateral_limit = (segment->lane_boundary_offsets & VEHICLE_COLLISION_ROAD_WIDTH_MASK) *
                          VEHICLE_COLLISION_ROAD_WIDTH_SCALE;
          if(outer_source->lateral_position > (lateral_limit * VEHICLE_COLLISION_FIXED16_ONE))
            {
              activate_nearby_road_object();
            }
          else
            {
              lateral_limit =
                -((segment->lane_boundary_offsets >> VEHICLE_COLLISION_NEGATIVE_ROAD_WIDTH_SHIFT) *
                  VEHICLE_COLLISION_ROAD_WIDTH_SCALE);
              if(outer_source->lateral_position < (lateral_limit * VEHICLE_COLLISION_FIXED16_ONE))
                {
                  activate_nearby_road_object();
                }
            }
        }

      if((outer_source->track_segment_index - active_source->track_segment_index) <=
         VEHICLE_COLLISION_OUTER_TRACK_RANGE)
        {
          if((outer_runtime->render_update_callback !=
              (void *) update_traffic_vehicle_simulation) &&
             ((outer_runtime->runtime_flags & VEHICLE_COLLISION_OUTER_RUNTIME_FLAGS) ==
              VEHICLE_COLLISION_OUTER_RUNTIME_FLAGS))
            {
              inner_node = outer_node->next;
              while(
                (inner_node->next != NULL) &&
                ((outer_source->track_segment_index + VEHICLE_COLLISION_INNER_TRACK_RANGE) >=
                 ((VehicleSimulationState *) inner_node)->simulation_source.track_segment_index))
                {
                  inner_vehicle = (VehicleSimulationState *) inner_node;
                  inner_runtime = (VehicleRuntimeObjectState *) inner_node;
                  inner_source  = &inner_vehicle->simulation_source;
                  if((inner_runtime->runtime_flags & VEHICLE_COLLISION_INNER_RUNTIME_FLAGS) ==
                     VEHICLE_COLLISION_INNER_RUNTIME_FLAGS)
                    {
                      collision_limit = outer_runtime->render_metadata->collision_radius +
                                        inner_runtime->render_metadata->collision_radius;
                      difference_x    = outer_source->position.x - inner_source->position.x;
                      if(difference_x < 0)
                        difference_x = -difference_x;
                      difference_z = outer_source->position.z - inner_source->position.z;
                      if(difference_z < 0)
                        difference_z = -difference_z;
                      if((difference_x < collision_limit) && (difference_z < collision_limit))
                        {
                          process_vehicle_pair_collision(outer_runtime, inner_runtime);
                        }
                    }
                  inner_node = inner_node->next;
                }
            }
        }
      outer_node = outer_node->next;
    }
}

void
apply_vehicle_collision_recovery_impulse(VehicleRuntimeObjectState *vehicle_)
{
  VehicleSimulationState *state;
  SimulationSource       *source;
  MatrixControllerState  *controller;

  state      = (VehicleSimulationState *) vehicle_;
  source     = &state->simulation_source;
  controller = (MatrixControllerState *) vehicle_->render_metadata;

  controller->angular_velocity.x =
    SCALE_VEHICLE_COLLISION_COMPONENT(controller->angular_velocity.x,
                                      VEHICLE_COLLISION_ANGULAR_DAMPING_SCALE);
  controller->angular_velocity.y =
    SCALE_VEHICLE_COLLISION_COMPONENT(controller->angular_velocity.y,
                                      VEHICLE_COLLISION_ANGULAR_DAMPING_SCALE);
  controller->angular_velocity.z =
    SCALE_VEHICLE_COLLISION_COMPONENT(controller->angular_velocity.z,
                                      VEHICLE_COLLISION_ANGULAR_DAMPING_SCALE);

  controller->angular_velocity.x = ADD_VEHICLE_COLLISION_COMPONENT(
    controller->angular_velocity.x,
    SCALE_VEHICLE_COLLISION_COMPONENT(source->track_basis.m00,
                                      VEHICLE_COLLISION_BASIS_ROW_ZERO_SCALE));
  controller->angular_velocity.y = ADD_VEHICLE_COLLISION_COMPONENT(
    controller->angular_velocity.y,
    SCALE_VEHICLE_COLLISION_COMPONENT(source->track_basis.m01,
                                      VEHICLE_COLLISION_BASIS_ROW_ZERO_SCALE));
  controller->angular_velocity.z = ADD_VEHICLE_COLLISION_COMPONENT(
    controller->angular_velocity.z,
    SCALE_VEHICLE_COLLISION_COMPONENT(source->track_basis.m02,
                                      VEHICLE_COLLISION_BASIS_ROW_ZERO_SCALE));

  controller->angular_velocity.x = ADD_VEHICLE_COLLISION_COMPONENT(
    controller->angular_velocity.x,
    SCALE_VEHICLE_COLLISION_COMPONENT(source->track_basis.m20,
                                      VEHICLE_COLLISION_BASIS_ROW_TWO_SCALE));
  controller->angular_velocity.y = ADD_VEHICLE_COLLISION_COMPONENT(
    controller->angular_velocity.y,
    SCALE_VEHICLE_COLLISION_COMPONENT(source->track_basis.m21,
                                      VEHICLE_COLLISION_BASIS_ROW_TWO_SCALE));
  controller->angular_velocity.z = ADD_VEHICLE_COLLISION_COMPONENT(
    controller->angular_velocity.z,
    SCALE_VEHICLE_COLLISION_COMPONENT(source->track_basis.m22,
                                      VEHICLE_COLLISION_BASIS_ROW_TWO_SCALE));

  controller->velocity.x = ADD_VEHICLE_COLLISION_COMPONENT(
    controller->velocity.x,
    SCALE_VEHICLE_COLLISION_COMPONENT(controller->matrix.m10,
                                      VEHICLE_COLLISION_UPWARD_VELOCITY_SCALE));
  controller->velocity.y = ADD_VEHICLE_COLLISION_COMPONENT(
    controller->velocity.y,
    SCALE_VEHICLE_COLLISION_COMPONENT(controller->matrix.m11,
                                      VEHICLE_COLLISION_UPWARD_VELOCITY_SCALE));
  controller->velocity.z = ADD_VEHICLE_COLLISION_COMPONENT(
    controller->velocity.z,
    SCALE_VEHICLE_COLLISION_COMPONENT(controller->matrix.m12,
                                      VEHICLE_COLLISION_UPWARD_VELOCITY_SCALE));
}

int32
resolve_vehicle_pair_collision(VehicleRuntimeObjectState *first_vehicle_,
                               VehicleRuntimeObjectState *second_vehicle_)
{
  MatrixControllerState *first_transform;
  MatrixControllerState *second_transform;
  Vector3                contact_point;
  Vector3                collision_normal;
  int32                  player_involved;
  int32                  secondary_involved;
  int32                  tracked_vehicle_involved;
  int32                  collision_threshold;
  int32                  sound_index;
  int32                  projection;
  int32                  motion;

  first_transform  = (MatrixControllerState *) first_vehicle_->render_metadata;
  second_transform = (MatrixControllerState *) second_vehicle_->render_metadata;
  if(resolve_matrix_controller_collision(first_transform,
                                         second_transform,
                                         &contact_point,
                                         &collision_normal) == 0)
    {
      return 0;
    }
  if(apply_matrix_collision_impulse(first_transform,
                                    second_transform,
                                    &contact_point,
                                    &collision_normal) == 0)
    {
      return 0;
    }

  player_involved    = ((void *) first_vehicle_ == (void *) G_ACTIVE_SIMULATION_SOURCE) ||
                       ((void *) second_vehicle_ == (void *) G_ACTIVE_SIMULATION_SOURCE);
  secondary_involved = ((void *) first_vehicle_ == G_ACTIVE_VEHICLE_RUNTIME_RECORD) ||
                       ((void *) second_vehicle_ == G_ACTIVE_VEHICLE_RUNTIME_RECORD);
  tracked_vehicle_involved =
    ((void *) first_vehicle_ ==
     (void *) g_VEHICLE_SIMULATION_STATES[VEHICLE_COLLISION_TRACKED_STATE_INDEX]) ||
    ((void *) second_vehicle_ ==
     (void *) g_VEHICLE_SIMULATION_STATES[VEHICLE_COLLISION_TRACKED_STATE_INDEX]);
  collision_threshold = g_RUNTIME_STATE.collision_threshold_default;
  if(player_involved != 0)
    collision_threshold = g_RUNTIME_STATE.collision_threshold_player;

  sound_index = (player_involved << VEHICLE_COLLISION_PLAYER_SOUND_SHIFT) +
                (secondary_involved << VEHICLE_COLLISION_SECONDARY_SOUND_SHIFT) +
                tracked_vehicle_involved;

  if((g_VEHICLE_COLLISION_SOUND_TIME <= collision_threshold) &&
     (g_RUNTIME_STATE.collision_simulation_enabled == 0))
    {
      if(first_vehicle_->render_update_callback != (void *) update_player_vehicle_state)
        {
          update_simulation_from_transform(
            (SimulationSource *) first_vehicle_,
            (SimulationUpdateCallback) first_vehicle_->render_update_callback);
          update_simulation_from_transform(
            (SimulationSource *) second_vehicle_,
            (SimulationUpdateCallback) second_vehicle_->render_update_callback);
        }

      first_vehicle_->collision_motion_projection = calculate_vehicle_motion_projection(
        (const VehicleMotionProjectionState *) ((uint8 *) first_vehicle_ +
                                                VEHICLE_COLLISION_TRACK_SOURCE_OFFSET));
      second_vehicle_->collision_motion_projection = calculate_vehicle_motion_projection(
        (const VehicleMotionProjectionState *) ((uint8 *) second_vehicle_ +
                                                VEHICLE_COLLISION_TRACK_SOURCE_OFFSET));

      if(player_involved != 0)
        {
          g_VEHICLE_GEAR_SHIFT_STATISTICS->vehicle_collision_count +=
            VEHICLE_COLLISION_STATISTICS_INCREMENT;
          if((secondary_involved != 0) && (g_VEHICLE_COLLISION_STATISTICS_SUPPRESSED == 0))
            {
              g_VEHICLE_GEAR_SHIFT_STATISTICS->secondary_vehicle_collision_count +=
                VEHICLE_COLLISION_STATISTICS_INCREMENT;
              g_VEHICLE_COLLISION_SCORE += VEHICLE_COLLISION_STATISTICS_INCREMENT;
            }
        }

      projection = first_vehicle_->collision_motion_projection;
      if(projection < 0)
        projection = -projection;
      if(projection > VEHICLE_COLLISION_PROJECTION_LIMIT)
        {
          projection = second_vehicle_->collision_motion_projection;
          if(projection < 0)
            projection = -projection;
          if(projection > VEHICLE_COLLISION_PROJECTION_LIMIT)
            request_sound_effect(g_VEHICLE_COLLISION_SOUND_CODES[sound_index]);
        }
      return 1;
    }

  request_sound_effect(
    g_VEHICLE_COLLISION_SOUND_CODES[sound_index + VEHICLE_COLLISION_STRONG_SOUND_OFFSET]);
  first_vehicle_->render_update_callback  = (void *) update_player_vehicle_state;
  second_vehicle_->render_update_callback = (void *) update_player_vehicle_state;

  motion = first_transform->velocity.z;
  if(motion < 0)
    motion = -motion;
  if(motion <= VEHICLE_COLLISION_MOTION_LIMIT)
    {
      motion = first_transform->velocity.x;
      if(motion < 0)
        motion = -motion;
    }
  if(motion <= VEHICLE_COLLISION_MOTION_LIMIT)
    {
      motion = first_transform->velocity.y;
      if(motion < 0)
        motion = -motion;
    }
  if(motion > VEHICLE_COLLISION_MOTION_LIMIT)
    first_transform->contact_countdown = VEHICLE_COLLISION_CONTACT_COUNTDOWN;

  motion = second_transform->velocity.z;
  if(motion < 0)
    motion = -motion;
  if(motion <= VEHICLE_COLLISION_MOTION_LIMIT)
    {
      motion = second_transform->velocity.x;
      if(motion < 0)
        motion = -motion;
    }
  if(motion <= VEHICLE_COLLISION_MOTION_LIMIT)
    {
      motion = second_transform->velocity.y;
      if(motion < 0)
        motion = -motion;
    }
  if(motion > VEHICLE_COLLISION_MOTION_LIMIT)
    second_transform->contact_countdown = VEHICLE_COLLISION_CONTACT_COUNTDOWN;

  if((player_involved != 0) && (g_RUNTIME_STATE.collision_simulation_enabled == 0))
    {
      rebuild_vehicle_track_frame((SimulationSource *) ((uint8 *) G_ACTIVE_SIMULATION_SOURCE +
                                                        VEHICLE_COLLISION_TRACK_SOURCE_OFFSET));
      g_VEHICLE_DRIVE_STATISTICS.summary.collision_event_time = g_VEHICLE_COLLISION_SOUND_TIME;
      if(secondary_involved != 0)
        {
          g_VEHICLE_GEAR_SHIFT_STATISTICS->secondary_vehicle_collision_count +=
            VEHICLE_COLLISION_STRONG_STATISTICS_INCREMENT;
          g_VEHICLE_COLLISION_SCORE += VEHICLE_COLLISION_STRONG_STATISTICS_INCREMENT;
        }
      g_RUNTIME_STATE.collision_simulation_enabled = 1;
      apply_vehicle_collision_recovery_impulse(
        (VehicleRuntimeObjectState *) G_ACTIVE_SIMULATION_SOURCE);
    }
  return 1;
}
