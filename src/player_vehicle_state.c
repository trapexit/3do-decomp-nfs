#include "player_vehicle_state.h"

#include "driving_warning_display.h"
#include "frontend_transition.h"
#include "game_frame_count.h"
#include "geometry.h"
#include "matrix_collision.h"
#include "signal_status.h"
#include "simulation_thread_signal.h"
#include "sound_spatial.h"
#include "three_d_object.h"
#include "tire_smoke.h"
#include "track_position.h"
#include "vehicle_callbacks.h"
#include "vehicle_drive_statistics.h"
#include "vehicle_parameter_loader.h"
#include "vehicle_simulation_state.h"

#include "audio.h"
#include "operamath.h"

#define PLAYER_STATE_ACTIVE_SOURCE_OFFSET 0x0C
#define PLAYER_STATE_TRACK_GAP_BEHIND 40
#define PLAYER_STATE_TRACK_GAP_AHEAD 39
#define PLAYER_STATE_ENGINE_IDLE_TARGET 1500
#define PLAYER_STATE_ENGINE_IDLE_ROUNDING 7
#define PLAYER_STATE_ENGINE_IDLE_SHIFT 3
#define PLAYER_STATE_FRAME_TIME 0x888
#define PLAYER_STATE_FIXED8_SHIFT 8
#define PLAYER_STATE_ANGLE_MASK 0xFFFFFF
#define PLAYER_STATE_SIGNED_ANGLE_LIMIT 0xBFFFFF
#define PLAYER_STATE_ANGLE_FULL_TURN 0x1000000
#define PLAYER_STATE_ENGINE_SETTLE_LIMIT 201
#define PLAYER_STATE_ENGINE_SETTLE_DECREMENT 200
#define PLAYER_STATE_INITIAL_ENGINE_SPEED 700
#define PLAYER_STATE_TRACK_BOUNDARY_SCALE 0x2000
#define PLAYER_STATE_SPECIAL_SURFACE_TYPE 4
#define PLAYER_STATE_TRACK_BOUNDARY_DISABLED_TYPE 5
#define PLAYER_STATE_BOUNDARY_HIGH_MASK 0xF0
#define PLAYER_STATE_BOUNDARY_LOW_MASK 0x0F
#define PLAYER_STATE_SPECIAL_SURFACE_OFFSET 7
#define PLAYER_STATE_LINEAR_SLEEP_SCALE_SHIFT 7
#define PLAYER_STATE_SLEEP_TEST_LIMIT 0x3333
#define PLAYER_STATE_SLEEP_DOT_ONE 0x10000
#define PLAYER_STATE_TIRE_SMOKE_FLAG 0x200
#define PLAYER_STATE_TIRE_SMOKE_START_LIMIT 0x40000
#define PLAYER_STATE_TIRE_SMOKE_START_TIME 0x5555
#define PLAYER_STATE_RECOVERY_COOLDOWN 60
#define PLAYER_STATE_RECOVERY_DISTANCE 0x28000
#define PLAYER_STATE_RECOVERY_RELATIVE_SCALE 0x60000
#define PLAYER_STATE_RECOVERY_SPEED_LIMIT 0x90001
#define PLAYER_STATE_RECOVERY_RATIO_LIMIT 0x10000
#define PLAYER_STATE_STABILITY_LIMIT 0x18000
#define PLAYER_STATE_STABILITY_DOT_LIMIT 0x0F700
#define PLAYER_STATE_STABILITY_DOT_TOLERANCE 10
#define PLAYER_STATE_STABILITY_ALT_LOWER 0x8F4
#define PLAYER_STATE_CONTACT_REFRESH_MASK 3
#define PLAYER_STATE_CONTACT_REFRESH_PHASE 2
#define PLAYER_STATE_ORIENTATION_SPEED_LIMIT 0x30000
#define PLAYER_STATE_COLLISION_SOUND_LOW_NORMAL 0x0CC00
#define PLAYER_STATE_COLLISION_SOUND_HIGH_NORMAL 0x0CCCC
#define PLAYER_STATE_COLLISION_SOUND_CLASS_DEFAULT 2
#define PLAYER_STATE_COLLISION_SOUND_CLASS_SIDE 4
#define PLAYER_STATE_COLLISION_SOUND_VARIANT 1
#define PLAYER_STATE_COLLISION_SOUND_POSITION_LIMIT 0x80000
#define PLAYER_STATE_COLLISION_SOUND_HYSTERESIS 0x8000
#define PLAYER_STATE_COLLISION_SOUND_COOLDOWN 10
#define PLAYER_STATE_COLLISION_GAIN_SCALE 30
#define PLAYER_STATE_COLLISION_GAIN_MIN 0x230000
#define PLAYER_STATE_COLLISION_GAIN_MAX 0x2BC0000
#define PLAYER_STATE_WARNING_DISPLAY_TIME 30
#define PLAYER_STATE_RECOVERY_RUNTIME_FLAG 1
#define PLAYER_STATE_RECOVERY_GEAR_STAGE 2
#define PLAYER_STATE_CONTACT_FAST_DECREMENT 2
#define PLAYER_STATE_RECOVERY_GAME_RESULT_A 2
#define PLAYER_STATE_RECOVERY_GAME_RESULT_B 3
#define PLAYER_STATE_RECOVERY_GAME_FLAG 8
#define PLAYER_STATE_ACTIVE_CONTACT_FLAG 8
#define VEHICLE_MESH_RESOURCE_OBJECT_OFFSET 0x14
#define VEHICLE_MESH_DEFORMATION_Z_THRESHOLD 58
#define VEHICLE_MESH_DEFORMATION_Y_THRESHOLD 32
#define VEHICLE_MESH_DEFORMATION_RANDOM_MULTIPLIER 0x2001
#define VEHICLE_MESH_DEFORMATION_RANDOM_INCREMENT 17
#define VEHICLE_MESH_DEFORMATION_Z_SHIFT 27
#define VEHICLE_MESH_DEFORMATION_Y_SHIFT 28

typedef struct VehicleMeshResourceSlot
{
  uint8         reserved_00[VEHICLE_MESH_RESOURCE_OBJECT_OFFSET];
  ThreeDObject *model;
} VehicleMeshResourceSlot;

extern void
update_traffic_vehicle_simulation(void *state_);
extern void
update_recovering_vehicle_simulation(void *state_);
extern void
rebuild_vehicle_track_frame(SimulationSource *source_);
extern void
clear_vehicle_index_slots(TireSmokeParticleState *particles_);

int32        g_VEHICLE_COLLISION_RESPONSE_MAGNITUDE;
Vector3      g_VEHICLE_COLLISION_CONTACT_VELOCITY;
int32        g_VEHICLE_ORIENTATION_STABILITY_VALUE;
int32        g_VEHICLE_COLLISION_POSITION_CORRECTION_ENABLED = 1;
int32        g_VEHICLE_SURFACE_COLLISION_NORMAL_SCALE        = 0x7333;
int32        g_VEHICLE_SURFACE_COLLISION_FRICTION_SCALE      = 0xCCCC;
int32        g_VEHICLE_SURFACE_COLLISION_UNUSED_LIMITS[2] = { -0x1999, 0x1999 };
typedef char VehicleSurfaceCollisionUnusedLimitsSize
  [(sizeof(g_VEHICLE_SURFACE_COLLISION_UNUSED_LIMITS) == 8) ? 1 : -1];
int32  g_PLAYER_VEHICLE_RECOVERY_PENDING;
uint32 g_VEHICLE_MESH_DEFORMATION_RANDOM_STATE;
int32  g_VEHICLE_MESH_DEFORMATION_APPLIED;

static int32
_absolute_player_state_value(int32 value_)
{
  if(value_ < 1)
    return -value_;
  return value_;
}

void
deform_vehicle_model_vertices_once(VehicleRuntimeObjectState *runtime_object_)
{
  VehicleMeshResourceSlot *resource_slot;
  ThreeDObject            *model;
  Vector3                 *vertices;
  int32                    vertex_index;

  if(g_VEHICLE_MESH_DEFORMATION_APPLIED != 0)
    return;

  g_VEHICLE_MESH_DEFORMATION_APPLIED      = 1;
  g_VEHICLE_MESH_DEFORMATION_RANDOM_STATE = GetAudioTime();
  resource_slot = (VehicleMeshResourceSlot *) runtime_object_->render_resource_state;
  model         = resource_slot->model;
  vertices      = (Vector3 *) model->material_table;
  for(vertex_index = 0; vertex_index < model->material_count; vertex_index++)
    {
      if(vertices[vertex_index].z > VEHICLE_MESH_DEFORMATION_Z_THRESHOLD)
        {
          g_VEHICLE_MESH_DEFORMATION_RANDOM_STATE =
            (g_VEHICLE_MESH_DEFORMATION_RANDOM_STATE * VEHICLE_MESH_DEFORMATION_RANDOM_MULTIPLIER) +
            VEHICLE_MESH_DEFORMATION_RANDOM_INCREMENT;
          vertices[vertex_index].z -=
            (int32) (g_VEHICLE_MESH_DEFORMATION_RANDOM_STATE >> VEHICLE_MESH_DEFORMATION_Z_SHIFT);
          if(vertices[vertex_index].y > VEHICLE_MESH_DEFORMATION_Y_THRESHOLD)
            {
              g_VEHICLE_MESH_DEFORMATION_RANDOM_STATE =
                (g_VEHICLE_MESH_DEFORMATION_RANDOM_STATE *
                 VEHICLE_MESH_DEFORMATION_RANDOM_MULTIPLIER) +
                VEHICLE_MESH_DEFORMATION_RANDOM_INCREMENT;
              vertices[vertex_index].y += (int32) (g_VEHICLE_MESH_DEFORMATION_RANDOM_STATE >>
                                                   VEHICLE_MESH_DEFORMATION_Y_SHIFT);
            }
        }
    }
}

static int32
_calculate_player_state_basis_dot(const Vector3 *left_, const Vector3 *right_)
{
  return (left_->x >> PLAYER_STATE_FIXED8_SHIFT) * (right_->x >> PLAYER_STATE_FIXED8_SHIFT) +
         (left_->y >> PLAYER_STATE_FIXED8_SHIFT) * (right_->y >> PLAYER_STATE_FIXED8_SHIFT) +
         (left_->z >> PLAYER_STATE_FIXED8_SHIFT) * (right_->z >> PLAYER_STATE_FIXED8_SHIFT);
}

static void
_extract_player_state_angles(SimulationSource *source_, MatrixControllerState *controller_)
{
  uint32 x_angle;
  uint32 z_angle;

  extract_matrix3x3_rotation_yzx_angles(&controller_->matrix,
                                        &source_->direction.x,
                                        &source_->direction.z,
                                        &source_->direction.y);
  x_angle              = (uint32) -source_->direction.x & PLAYER_STATE_ANGLE_MASK;
  z_angle              = (uint32) source_->direction.z & PLAYER_STATE_ANGLE_MASK;
  source_->direction.x = (int32) x_angle;
  source_->direction.z = (int32) z_angle;
  source_->direction.y = (int32) ((uint32) -source_->direction.y & PLAYER_STATE_ANGLE_MASK);
  if(x_angle > PLAYER_STATE_SIGNED_ANGLE_LIMIT)
    source_->direction.x = (int32) x_angle - PLAYER_STATE_ANGLE_FULL_TURN;
  if(z_angle > PLAYER_STATE_SIGNED_ANGLE_LIMIT)
    source_->direction.z = (int32) z_angle - PLAYER_STATE_ANGLE_FULL_TURN;
}

static void
_clear_player_state_motion(VehicleParameterState *vehicle_)
{
  vehicle_->primary_control_value   = 0;
  vehicle_->secondary_control_value = 0;
  if(vehicle_->engine_speed < PLAYER_STATE_ENGINE_SETTLE_LIMIT)
    vehicle_->engine_speed = 0;
  else
    vehicle_->engine_speed -= PLAYER_STATE_ENGINE_SETTLE_DECREMENT;
  vehicle_->track_lateral_velocity     = 0;
  vehicle_->track_vertical_velocity    = 0;
  vehicle_->vehicle_speed              = 0;
  vehicle_->yaw_velocity               = 0;
  vehicle_->driven_wheel_speed         = 0;
  vehicle_->driven_wheel_state_flags   = 0;
  vehicle_->undriven_wheel_state_flags = 0;
  vehicle_->gear_control_index         = -1;
  vehicle_->gear_control_ready         = 0;
  if(vehicle_->gear_control_stage != 0)
    vehicle_->gear_control_stage = PLAYER_STATE_RECOVERY_GEAR_STAGE;
  vehicle_->gear_shift_delay               = -1;
  vehicle_->deferred_primary_control_value = 0;
  vehicle_->gear_control_pending           = 0;
  vehicle_->control_event_value            = 0;
}

void
update_player_vehicle_state(void *state_)
{
  SimulationSource           *active_source;
  SimulationSource           *source;
  VehicleParameterState      *vehicle;
  MatrixControllerState      *controller;
  TrackSegment               *segment;
  VehicleGearShiftStatistics *gear_statistics;
  Vector3                     first_basis;
  Vector3                     second_basis;
  Vector3                     third_basis;
  Vector3                     reference_position;
  Vector3                     relative_position;
  Vector3                     collision_normal;
  Vector3                     collision_point;
  int32                       active_particles;
  int32                       collision_class;
  int32                       collision_gain;
  int32                       collision_impulse;
  int32                       distance;
  int32                       gap;
  int32                       margin;
  int32                       pan;
  int32                       recovery_started;
  int32                       scale;
  int32                       stability_dot;
  int32                       track_segment_index;
  uint8                       boundary_code;
  uint8                       boundary_enabled;
  uint32                      contact_countdown;

  source     = (SimulationSource *) state_;
  vehicle    = (VehicleParameterState *) state_;
  controller = (MatrixControllerState *) source->refresh_state;
  active_source =
    (SimulationSource *) ((uint8 *) G_ACTIVE_SIMULATION_SOURCE + PLAYER_STATE_ACTIVE_SOURCE_OFFSET);

  track_segment_index = source->track_segment_index;
  gap                 = track_segment_index - active_source->track_segment_index;
  if((source->update_callback == update_traffic_vehicle_simulation) ||
     (gap < -PLAYER_STATE_TRACK_GAP_BEHIND) || (gap > PLAYER_STATE_TRACK_GAP_AHEAD))
    {
      update_simulation_from_transform(
        (SimulationSource *) source->runtime_record,
        (SimulationUpdateCallback) update_vehicle_simulation_transform);
      return;
    }

  source->transform_active = 1;
  if(source->runtime_record == G_ACTIVE_VEHICLE_RUNTIME_RECORD)
    {
      scale = PLAYER_STATE_ENGINE_IDLE_TARGET - vehicle->engine_speed;
      if(scale < 0)
        scale += PLAYER_STATE_ENGINE_IDLE_ROUNDING;
      vehicle->engine_speed += scale >> PLAYER_STATE_ENGINE_IDLE_SHIFT;
    }

  if(adjust_track_segment_index(&source->position, &track_segment_index) != 0)
    {
      source->track_segment_index = track_segment_index;
      rebuild_vehicle_track_frame(source);
    }

  first_basis.x        = source->track_basis.m00;
  first_basis.y        = source->track_basis.m01;
  first_basis.z        = -source->track_basis.m02;
  second_basis.x       = source->track_basis.m10;
  second_basis.y       = source->track_basis.m11;
  second_basis.z       = -source->track_basis.m12;
  third_basis.x        = source->track_basis.m20;
  third_basis.y        = source->track_basis.m21;
  third_basis.z        = -source->track_basis.m22;
  reference_position.x = source->track_reference_position.x;
  reference_position.y = source->track_reference_position.y;
  reference_position.z = -source->track_reference_position.z;

  advance_matrix_controller_state(controller);
  relative_position.x = controller->position.x - reference_position.x;
  relative_position.y = controller->position.y - reference_position.y;
  relative_position.z = controller->position.z - reference_position.z;
  collision_impulse   = 0;
  segment             = &g_TRACK_SEGMENTS[source->track_segment_index];

  if(_calculate_player_state_basis_dot(&first_basis, &relative_position) < 1)
    {
      boundary_code    = segment->boundary_behavior_codes;
      boundary_enabled = (uint8) (boundary_code & PLAYER_STATE_BOUNDARY_HIGH_MASK);
      if(boundary_enabled != 0)
        boundary_code = segment->road_geometry_type;
      if((boundary_enabled != 0) && (boundary_code != PLAYER_STATE_TRACK_BOUNDARY_DISABLED_TYPE))
        margin = g_RUNTIME_STATE.positive_road_boundary_margin;
      else
        margin = g_RUNTIME_STATE.negative_road_boundary_margin;
      scale =
        ((int32) segment->negative_boundary_width * -PLAYER_STATE_TRACK_BOUNDARY_SCALE - margin) >>
        PLAYER_STATE_FIXED8_SHIFT;
      collision_normal = first_basis;
    }
  else
    {
      boundary_code    = segment->boundary_behavior_codes;
      boundary_enabled = (uint8) (boundary_code & PLAYER_STATE_BOUNDARY_LOW_MASK);
      if(boundary_enabled != 0)
        boundary_code = segment->road_geometry_type;
      if((boundary_enabled != 0) && (boundary_code != PLAYER_STATE_TRACK_BOUNDARY_DISABLED_TYPE))
        margin = g_RUNTIME_STATE.positive_road_boundary_margin;
      else
        margin = g_RUNTIME_STATE.negative_road_boundary_margin;
      scale =
        (margin + (int32) segment->positive_boundary_width * PLAYER_STATE_TRACK_BOUNDARY_SCALE) >>
        PLAYER_STATE_FIXED8_SHIFT;
      collision_normal.x = -first_basis.x;
      collision_normal.y = -first_basis.y;
      collision_normal.z = -first_basis.z;
    }
  collision_point.x = (first_basis.x >> PLAYER_STATE_FIXED8_SHIFT) * scale + reference_position.x;
  collision_point.y = (first_basis.y >> PLAYER_STATE_FIXED8_SHIFT) * scale + reference_position.y;
  collision_point.z = (first_basis.z >> PLAYER_STATE_FIXED8_SHIFT) * scale + reference_position.z;
  resolve_matrix_controller_surface_collision(controller, &collision_normal, &collision_point);
  if(g_VEHICLE_COLLISION_RESPONSE_MAGNITUDE > 0)
    collision_impulse = g_VEHICLE_COLLISION_RESPONSE_MAGNITUDE;

  if(segment->road_geometry_type == PLAYER_STATE_SPECIAL_SURFACE_TYPE)
    {
      collision_point.x =
        second_basis.x * PLAYER_STATE_SPECIAL_SURFACE_OFFSET + reference_position.x;
      collision_point.y =
        second_basis.y * PLAYER_STATE_SPECIAL_SURFACE_OFFSET + reference_position.y;
      collision_point.z =
        second_basis.z * PLAYER_STATE_SPECIAL_SURFACE_OFFSET + reference_position.z;
      collision_normal.x = -second_basis.x;
      collision_normal.y = -second_basis.y;
      collision_normal.z = -second_basis.z;
      resolve_matrix_controller_surface_collision(controller, &collision_normal, &collision_point);
      if(collision_impulse < g_VEHICLE_COLLISION_RESPONSE_MAGNITUDE)
        collision_impulse = g_VEHICLE_COLLISION_RESPONSE_MAGNITUDE;
    }
  resolve_matrix_controller_surface_collision(controller, &second_basis, &reference_position);
  if(collision_impulse < g_VEHICLE_COLLISION_RESPONSE_MAGNITUDE)
    collision_impulse = g_VEHICLE_COLLISION_RESPONSE_MAGNITUDE;

  if(active_source == source)
    {
      _extract_player_state_angles(source, controller);
      _clear_player_state_motion(vehicle);
    }

  if((_absolute_player_state_value(controller->velocity.x) <
      g_RUNTIME_STATE.linear_motion_sleep_threshold) &&
     (_absolute_player_state_value(controller->velocity.y) <
      g_RUNTIME_STATE.linear_motion_sleep_threshold) &&
     (_absolute_player_state_value(controller->velocity.z) <
      g_RUNTIME_STATE.linear_motion_sleep_threshold) &&
     (_absolute_player_state_value(controller->angular_velocity.x) <
      g_RUNTIME_STATE.angular_motion_sleep_threshold) &&
     (_absolute_player_state_value(controller->angular_velocity.y) <
      g_RUNTIME_STATE.angular_motion_sleep_threshold) &&
     (_absolute_player_state_value(controller->angular_velocity.z) <
      g_RUNTIME_STATE.angular_motion_sleep_threshold) &&
     (g_VEHICLE_ORIENTATION_STABILITY_VALUE < PLAYER_STATE_SLEEP_TEST_LIMIT))
    {
      stability_dot =
        _calculate_player_state_basis_dot(&second_basis, (const Vector3 *) &controller->matrix.m10);
      if((stability_dot >
          PLAYER_STATE_SLEEP_DOT_ONE - g_RUNTIME_STATE.orientation_sleep_threshold) ||
         (stability_dot <
          g_RUNTIME_STATE.orientation_sleep_threshold - PLAYER_STATE_SLEEP_DOT_ONE) ||
         ((stability_dot < g_RUNTIME_STATE.orientation_sleep_threshold) &&
          (stability_dot > -g_RUNTIME_STATE.orientation_sleep_threshold)))
        {
          controller->velocity.x = (controller->velocity.x >> PLAYER_STATE_FIXED8_SHIFT)
                                << PLAYER_STATE_LINEAR_SLEEP_SCALE_SHIFT;
          controller->velocity.y = (controller->velocity.y >> PLAYER_STATE_FIXED8_SHIFT)
                                << PLAYER_STATE_LINEAR_SLEEP_SCALE_SHIFT;
          controller->velocity.z = (controller->velocity.z >> PLAYER_STATE_FIXED8_SHIFT)
                                << PLAYER_STATE_LINEAR_SLEEP_SCALE_SHIFT;
          controller->angular_velocity.x =
            (controller->angular_velocity.x >> PLAYER_STATE_FIXED8_SHIFT)
            << PLAYER_STATE_LINEAR_SLEEP_SCALE_SHIFT;
          controller->angular_velocity.y =
            (controller->angular_velocity.y >> PLAYER_STATE_FIXED8_SHIFT)
            << PLAYER_STATE_LINEAR_SLEEP_SCALE_SHIFT;
          controller->angular_velocity.z =
            (controller->angular_velocity.z >> PLAYER_STATE_FIXED8_SHIFT)
            << PLAYER_STATE_LINEAR_SLEEP_SCALE_SHIFT;
        }
    }

  if((active_source == source) && (g_RUNTIME_STATE.collision_simulation_enabled != 0))
    {
      if((source->status_flags & PLAYER_STATE_TIRE_SMOKE_FLAG) != 0)
        {
          active_particles =
            update_tire_smoke_particles(&g_TIRE_SMOKE_PARTICLES, PLAYER_STATE_FRAME_TIME);
          if(active_particles == 0)
            source->status_flags &= ~PLAYER_STATE_TIRE_SMOKE_FLAG;
        }
      if((source->status_flags & PLAYER_STATE_TIRE_SMOKE_FLAG) == 0)
        {
          if((g_RUNTIME_STATE.collision_simulation_enabled != 0) &&
             ((_absolute_player_state_value(controller->velocity.x) +
               _absolute_player_state_value(controller->velocity.y) +
               _absolute_player_state_value(controller->velocity.z)) <
              PLAYER_STATE_TIRE_SMOKE_START_LIMIT))
            {
              source->status_flags |= PLAYER_STATE_TIRE_SMOKE_FLAG;
              emit_tire_smoke_particles(&g_TIRE_SMOKE_PARTICLES,
                                        active_source,
                                        PLAYER_STATE_TIRE_SMOKE_START_TIME);
            }
        }
      else
        {
          emit_tire_smoke_particles(&g_TIRE_SMOKE_PARTICLES,
                                    active_source,
                                    PLAYER_STATE_FRAME_TIME);
        }

      g_SIMULATION_MODE_TRANSITION_COOLDOWN = PLAYER_STATE_RECOVERY_COOLDOWN;
      if((g_RUNTIME_STATE.collision_simulation_enabled != 0) &&
         (controller->contact_countdown == 0) &&
         (G_ACTIVE_VEHICLE_RUNTIME_RECORD != NULL) &&
         (((SimulationSource *) G_ACTIVE_VEHICLE_RUNTIME_RECORD)->update_callback ==
          (SimulationUpdateCallback) update_vehicle_simulation_transform) &&
         (active_source->track_segment_index <= source->track_segment_index) &&
         (source->target_track_speed >= PLAYER_STATE_RECOVERY_SPEED_LIMIT))
        {
          scale = DivSF16((active_source->track_segment_index - source->track_segment_index) *
                            PLAYER_STATE_RECOVERY_RELATIVE_SCALE,
                          source->target_track_speed);
          if(scale <= PLAYER_STATE_RECOVERY_RATIO_LIMIT)
            controller->contact_countdown = 1;
        }

      recovery_started = 0;
      if(controller->contact_countdown == 0)
        {
          if(g_RUNTIME_STATE.collision_simulation_enabled != 0)
            {
              recovery_started                             = 1;
              g_RUNTIME_STATE.collision_simulation_enabled = 0;
              reset_vehicle_collision_simulation();
              controller->matrix     = source->track_basis;
              controller->matrix.m02 = -controller->matrix.m02;
              controller->matrix.m12 = -controller->matrix.m12;
              controller->matrix.m20 = -controller->matrix.m20;
              controller->matrix.m21 = -controller->matrix.m21;
              relative_position.x    = controller->position.x - reference_position.x;
              relative_position.y    = controller->position.y - reference_position.y;
              relative_position.z    = controller->position.z - reference_position.z;
              scale = (PLAYER_STATE_RECOVERY_DISTANCE -
                       _calculate_player_state_basis_dot(&second_basis, &relative_position)) >>
                      PLAYER_STATE_FIXED8_SHIFT;
              controller->position.x += scale * (second_basis.x >> PLAYER_STATE_FIXED8_SHIFT);
              controller->position.y += scale * (second_basis.y >> PLAYER_STATE_FIXED8_SHIFT);
              controller->position.z += scale * (second_basis.z >> PLAYER_STATE_FIXED8_SHIFT);
              _extract_player_state_angles(source, controller);
              g_SIMULATION_MODE_TRANSITION_COOLDOWN = 0;
              clear_vehicle_index_slots(&g_TIRE_SMOKE_PARTICLES);
              source->status_flags &= ~PLAYER_STATE_TIRE_SMOKE_FLAG;
              _clear_player_state_motion(vehicle);
              vehicle->engine_speed                = PLAYER_STATE_INITIAL_ENGINE_SPEED;
              vehicle->target_engine_speed         = 0;
              vehicle->world_lateral_velocity      = 0;
              vehicle->world_vertical_velocity     = 0;
              vehicle->world_longitudinal_velocity = 0;
              if(G_FRONTEND_EXIT_REQUESTED == PLAYER_STATE_RECOVERY_RUNTIME_FLAG)
                {
                  g_DRIVING_WARNING_DISPLAY_STATE.control_input_display_timer =
                    PLAYER_STATE_WARNING_DISPLAY_TIME;
                }
            }

          update_simulation_from_transform(
            (SimulationSource *) G_ACTIVE_SIMULATION_SOURCE,
            (SimulationUpdateCallback) update_driving_warning_vehicle_state);
          source->transform_active = 0;
          if(recovery_started != 0)
            {
              update_vehicle_track_motion(source, 0);
              source->heading_velocity            = 0;
              source->speed_acceleration_table[0] = 0;
              source->motion.x                    = 0;
              source->motion.y                    = 0;
              source->motion.z                    = 0;
            }

          gear_statistics = g_VEHICLE_GEAR_SHIFT_STATISTICS;
          if((G_FRONTEND_EXIT_REQUESTED == PLAYER_STATE_RECOVERY_RUNTIME_FLAG) &&
             (g_GAME_RUNTIME_STATE.result != PLAYER_STATE_RECOVERY_GAME_RESULT_A) &&
             (g_GAME_RUNTIME_STATE.result != PLAYER_STATE_RECOVERY_GAME_RESULT_B))
            {
              gear_statistics->recovery_count++;
              g_VEHICLE_DRIVE_STATISTICS.summary.recovery_count++;
            }
          g_PLAYER_VEHICLE_RECOVERY_PENDING = 1;
          if((G_FRONTEND_EXIT_REQUESTED == PLAYER_STATE_RECOVERY_RUNTIME_FLAG) &&
             ((g_VEHICLE_DRIVE_STATISTICS.summary.lane_search_limit -
               (int32) g_VEHICLE_DRIVE_STATISTICS.summary.recovery_count) +
                g_VEHICLE_DRIVE_STATISTICS.summary.recovery_adjustment <
              1))
            {
              g_GAME_RUNTIME_STATE.flags_14 = PLAYER_STATE_RECOVERY_GAME_FLAG;
              g_GAME_RUNTIME_STATE.flags_18 = PLAYER_STATE_RECOVERY_GAME_FLAG;
              gear_statistics->recovery_frame_duration =
                G_GAME_FRAME_COUNTER - g_GAME_RUNTIME_STATE.race_start_frame;
              gear_statistics->recovery_track_position =
                active_source->track_segment_index * PLAYER_STATE_RECOVERY_RELATIVE_SCALE;
              if(gear_statistics->first_recovery_track_position == 0)
                {
                  gear_statistics->latest_recovery_track_position =
                    active_source->track_segment_index * PLAYER_STATE_RECOVERY_RELATIVE_SCALE;
                }
              update_vehicle_drive_statistics_derived_values(
                (VehicleDriveStatisticsRecord *) gear_statistics);
            }
          return;
        }

      vehicle->speed_magnitude      = _absolute_player_state_value(source->target_track_speed);
      contact_countdown             = (uint32) controller->contact_countdown;
      controller->contact_countdown = (int32) contact_countdown - 1;
      if(controller->contact_countdown != 0)
        {
          if((_absolute_player_state_value(controller->velocity.x) <
              PLAYER_STATE_STABILITY_LIMIT) &&
             (_absolute_player_state_value(controller->velocity.y) <
              PLAYER_STATE_STABILITY_LIMIT) &&
             (_absolute_player_state_value(controller->velocity.z) <
              PLAYER_STATE_STABILITY_LIMIT) &&
             (_absolute_player_state_value(controller->angular_velocity.x) <
              PLAYER_STATE_STABILITY_LIMIT) &&
             (_absolute_player_state_value(controller->angular_velocity.y) <
              PLAYER_STATE_STABILITY_LIMIT) &&
             (_absolute_player_state_value(controller->angular_velocity.z) <
              PLAYER_STATE_STABILITY_LIMIT))
            {
              controller->contact_countdown =
                (int32) contact_countdown - PLAYER_STATE_CONTACT_FAST_DECREMENT;
              stability_dot =
                _calculate_player_state_basis_dot(&second_basis,
                                                  (const Vector3 *) &controller->matrix.m10);
              if((stability_dot <
                  PLAYER_STATE_STABILITY_DOT_LIMIT - PLAYER_STATE_STABILITY_DOT_TOLERANCE) ||
                 (stability_dot >
                  PLAYER_STATE_STABILITY_DOT_LIMIT + PLAYER_STATE_STABILITY_DOT_TOLERANCE))
                {
                  if((controller->contact_countdown != 0) &&
                     ((stability_dot <= -PLAYER_STATE_STABILITY_DOT_LIMIT) ||
                      (stability_dot > PLAYER_STATE_STABILITY_ALT_LOWER)))
                    controller->contact_countdown--;
                }
            }
        }
    }
  else
    {
      stability_dot =
        _calculate_player_state_basis_dot(&second_basis, (const Vector3 *) &controller->matrix.m10);
      if(controller->contact_countdown == 0)
        {
          if(stability_dot >
             PLAYER_STATE_STABILITY_DOT_LIMIT + PLAYER_STATE_STABILITY_DOT_TOLERANCE)
            {
              rebuild_matrix3x3_basis_from_normal(&controller->matrix, &second_basis);
              if((source->status_flags & PLAYER_STATE_ACTIVE_CONTACT_FLAG) != 0)
                clear_vehicle_simulation_contact_flags(0);
              update_simulation_from_transform(
                (SimulationSource *) source->runtime_record,
                (SimulationUpdateCallback) update_recovering_vehicle_simulation);
            }
        }
      else
        {
          contact_countdown             = (uint32) controller->contact_countdown - 1;
          controller->contact_countdown = (int32) contact_countdown;
          if((_absolute_player_state_value(controller->velocity.x) >=
              PLAYER_STATE_STABILITY_LIMIT) ||
             (_absolute_player_state_value(controller->velocity.y) >=
              PLAYER_STATE_STABILITY_LIMIT) ||
             (_absolute_player_state_value(controller->velocity.z) >=
              PLAYER_STATE_STABILITY_LIMIT) ||
             (_absolute_player_state_value(controller->angular_velocity.x) >=
              PLAYER_STATE_STABILITY_LIMIT) ||
             (_absolute_player_state_value(controller->angular_velocity.y) >=
              PLAYER_STATE_STABILITY_LIMIT) ||
             (_absolute_player_state_value(controller->angular_velocity.z) >=
              PLAYER_STATE_STABILITY_LIMIT))
            {
              controller->contact_countdown =
                (int32) ((contact_countdown & PLAYER_STATE_CONTACT_REFRESH_MASK) +
                         PLAYER_STATE_RECOVERY_COOLDOWN);
              if((source->status_flags & PLAYER_STATE_ACTIVE_CONTACT_FLAG) != 0)
                clear_vehicle_simulation_contact_flags(0);
            }
        }

      if((controller->stability_measure > PLAYER_STATE_ORIENTATION_SPEED_LIMIT) &&
         (g_GAME_RUNTIME_STATE.reserved_10[0] == 0))
        {
          rebuild_matrix3x3_basis_from_normal(&controller->matrix, &second_basis);
          scale = _calculate_player_state_basis_dot(&second_basis, &controller->angular_velocity) >>
                  PLAYER_STATE_FIXED8_SHIFT;
          controller->angular_velocity.x = (second_basis.x >> PLAYER_STATE_FIXED8_SHIFT) * scale;
          controller->angular_velocity.y = (second_basis.y >> PLAYER_STATE_FIXED8_SHIFT) * scale;
          controller->angular_velocity.z = (second_basis.z >> PLAYER_STATE_FIXED8_SHIFT) * scale;
        }
    }

  if(g_VEHICLE_ORIENTATION_STABILITY_VALUE >= PLAYER_STATE_SLEEP_TEST_LIMIT)
    {
      collision_class = PLAYER_STATE_COLLISION_SOUND_CLASS_DEFAULT;
      if(g_VEHICLE_COLLISION_RESPONSE_MAGNITUDE > 0)
        {
          collision_gain = (_absolute_player_state_value(relative_position.x) +
                            _absolute_player_state_value(relative_position.z)) *
                           PLAYER_STATE_COLLISION_GAIN_SCALE;
          if(collision_gain < PLAYER_STATE_COLLISION_GAIN_MIN)
            collision_gain = 0;
          else if(collision_gain > PLAYER_STATE_COLLISION_GAIN_MAX)
            collision_gain = PLAYER_STATE_COLLISION_GAIN_MAX;
          source->collision_impulse = collision_gain;
        }
      if(controller->matrix.m11 > PLAYER_STATE_COLLISION_SOUND_HIGH_NORMAL)
        collision_class = PLAYER_STATE_COLLISION_SOUND_CLASS_SIDE;
      if(active_source == source)
        {
          source->collision_type            = 0;
          g_ACTIVE_SIMULATION_SOUND_INDEX   = collision_class;
          g_ACTIVE_SIMULATION_SOUND_VARIANT = 1;
        }
      else
        {
          source->collision_type =
            (collision_class << PLAYER_STATE_FIXED8_SHIFT) | PLAYER_STATE_COLLISION_SOUND_VARIANT;
        }

      if((g_GAME_RUNTIME_STATE.statistics_frame_window < collision_impulse) &&
         (controller->stability_measure < PLAYER_STATE_COLLISION_SOUND_POSITION_LIMIT))
        {
          calculate_spatial_sound_parameters((const Vector3 *) source->runtime_record,
                                             &distance,
                                             &pan);
          play_spatial_sound_effect_scaled(-1,
                                           collision_class,
                                           PLAYER_STATE_COLLISION_SOUND_VARIANT,
                                           collision_impulse,
                                           distance,
                                           pan);
          g_GAME_RUNTIME_STATE.statistics_frame_window =
            collision_impulse + PLAYER_STATE_COLLISION_SOUND_HYSTERESIS;
          g_GAME_RUNTIME_STATE.statistics_track_segment_threshold =
            PLAYER_STATE_COLLISION_SOUND_COOLDOWN;
        }
    }
  else
    {
      source->collision_impulse = 0;
    }

  source->position = controller->position;
  scale            = source->controller_position_offset >> PLAYER_STATE_FIXED8_SHIFT;
  source->position.x -= scale * (controller->matrix.m10 >> PLAYER_STATE_FIXED8_SHIFT);
  source->position.y -= scale * (controller->matrix.m11 >> PLAYER_STATE_FIXED8_SHIFT);
  source->position.z =
    -(source->position.z - scale * (controller->matrix.m12 >> PLAYER_STATE_FIXED8_SHIFT));
  source->motion   = controller->velocity;
  source->motion.x = -source->motion.x;
  source->motion.z = -source->motion.z;

  if((((uint32) controller->contact_countdown & PLAYER_STATE_CONTACT_REFRESH_MASK) ==
      PLAYER_STATE_CONTACT_REFRESH_PHASE) ||
     (controller->contact_countdown == 0))
    orthonormalize_matrix3x3(&controller->matrix);

  source->track_transform     = controller->matrix;
  source->track_transform.m02 = -source->track_transform.m02;
  source->track_transform.m12 = -source->track_transform.m12;
  source->track_transform.m20 = -source->track_transform.m20;
  source->track_transform.m21 = -source->track_transform.m21;
}
