#pragma once

#include "frontend_render_state.h"
#include "geometry.h"
#include "vehicle_runtime_pool.h"

struct TrackOrientationState;
struct VehicleParameters;
struct VehicleSimulationState;

#define SIMULATION_MODE_CONFIGURATION_COUNT 11
#define SIMULATION_VIEW_CONFIGURATION_COUNT 3
#define SIMULATION_DEPTH_SCALE_TABLE_COUNT 1024

typedef void (*SimulationUpdateCallback)(void *state_);

typedef struct SimulationCameraConfiguration
{
  uint8 reserved_000[0xA8];
  int32 dashboard_motion_divisor;
  uint8 reserved_0_ac[0x29C];
  int32 transition_height;
  int32 vertical_origin;
} SimulationCameraConfiguration;

typedef struct SimulationProjectionDescriptor
{
  int32   view_angle_degrees;
  int32   viewport_size;
  int32   horizontal_origin;
  int32   vertical_origin;
  uint8   reserved_10[0x08];
  int32   projection_scale;
  uint32 *depth_scale_table;
} SimulationProjectionDescriptor;

typedef struct SimulationProjectionTransform
{
  Matrix3x3 view_matrix;
  int32     reserved_24;
  Vector3   camera_position;
  int32     x_rotation;
  int32     y_rotation;
  int32     z_rotation;
  uint8     flags;
  uint8     reserved_41[3];
} SimulationProjectionTransform;

typedef struct SimulationProjectionSource
{
  uint8                           reserved_000[4];
  int32                           dashboard_art_index;
  uint8                           reserved_008[0x30];
  int32                           track_segment_index;
  int32                           road_update_state;
  uint8                           reserved_040[0x50];
  Matrix3x3                       primary_view_matrix;
  SimulationProjectionTransform   projection_transform;
  int32                           track_section_position;
  int32                           road_geometry_refresh_pending;
  SimulationProjectionDescriptor *primary_projection;
  SimulationProjectionDescriptor *secondary_projection;
} SimulationProjectionSource;

typedef struct SimulationSource
{
  Vector3                        position;
  Vector3                        direction;
  int32                          heading_rotation;
  int32                          transform_active;
  Matrix3x3                      track_transform;
  int32                          track_segment_index;
  Vector3                        motion;
  int32                          longitudinal_acceleration;
  int32                          reserved_058;
  int32                          pitch_motion_scale;
  int32                          reserved_060;
  int32                          speed_magnitude;
  int32                          heading_velocity;
  int32                          longitudinal_axle_span;
  int32                          lateral_axle_span;
  int32                          lateral_position;
  int32                          render_height_offset;
  void                          *runtime_record;
  Matrix3x3                      track_basis;
  Vector3                        track_reference_position;
  int32                          controller_position_offset;
  void                          *refresh_state;
  uint8                          reserved_0_b8[0x94];
  int32                          minimum_track_speed;
  int32                          target_track_speed;
  int32                          track_position_offset;
  int32                          track_speed_reference[3];
  int32                          track_speed_scale[3];
  int32                          track_speed_response[3];
  uint32                         status_flags;
  int32                          speed_acceleration_table[100];
  int32                          art_acceleration_scales[6];
  int32                          art_acceleration_reciprocals[6];
  int32                          art_trailing_fixed_value;
  uint8                          art_variant;
  uint8                          reserved_345[3];
  int32                          alternate_track_segment_index;
  int32                          alternate_track_position_fraction;
  Vector3                        alternate_surface_origin;
  int32                          alternate_direction_x;
  int32                          alternate_heading;
  int32                          road_render_z_rotation_offset;
  int32                          dashboard_rotation;
  int32                          alternate_heading_refresh_pending;
  Vector3                        alternate_surface_projection_origin;
  uint8                          reserved_37_c[0x18];
  int32                          alternate_heading_reference_x;
  uint8                          reserved_398[4];
  int32                          alternate_heading_reference_z;
  int32                          mode_zero_z_rotation;
  int32                          mode_zero_x_rotation;
  Vector3                        surface_longitudinal_vector;
  Vector3                        surface_lateral_vector;
  Vector3                        surface_projection_origin;
  Vector3                        road_animation_view_position;
  int32                          track_interpolation;
  uint8                          reserved_3_dc[0x10];
  int32                          audio_pan_target;
  int32                          reserved_3_f0;
  int32                          traffic_brake_indicator;
  int32                          reserved_3_f8;
  int32                          audio_engine_speed;
  int32                          audio_engine_speed_target;
  int32                          surface_height_scale;
  int32                          dashboard_speed_value;
  int32                          reserved_40_c;
  int32                          reserved_410;
  Vector3                        track_gravity;
  int32                          reserved_420;
  int32                          reserved_424;
  int32                          surface_index;
  int32                          surface_contact_active;
  int32                          warning_display_state;
  int32                          warning_display_index;
  int32                          warning_display_pending;
  int32                          recovery_sound_counter;
  int32                          lane_change_active;
  int32                          collision_impulse;
  int32                          collision_type;
  int32                          reserved_44_c;
  int32                          primary_spatial_sound_gain;
  int32                          secondary_spatial_sound_gain;
  int32                          reserved_458;
  int32                          spatial_sound_heading;
  uint8                          reserved_460[4];
  uint8                          high_detail_shape_flags[2];
  uint8                          reserved_466[0x3A];
  int32                          art_trailing_integer_value;
  uint8                          reserved_4_a4[0x0C];
  SimulationCameraConfiguration *camera_configuration;
  uint8                          reserved_4_b4[8];
  struct VehicleParameters      *vehicle_parameters;
  uint8                          reserved_4_c0[4];
  SimulationUpdateCallback       update_callback;
} SimulationSource;

/* Retail aliases this handoff to the runtime pool's player slot. */
#define G_ACTIVE_SIMULATION_SOURCE                                                                 \
  ((struct VehicleSimulationState *) g_VEHICLE_RUNTIME_POOL_STATE.player_object)
extern int32 g_SIMULATION_VECTOR_Z_ADJUSTMENT;

void
update_simulation_from_transform(SimulationSource        *source_,
                                 SimulationUpdateCallback update_callback_);
void
rebuild_simulation_transform(void *transform_source_);
void
prepare_simulation_collision_transform(SimulationSource *source_);
void
prepare_simulation_transform(SimulationSource *source_);
void
apply_simulation_local_impulse(SimulationSource *source_,
                               int32             longitudinal_impulse_,
                               int32             lateral_impulse_,
                               int32             vertical_impulse_);
void
align_vehicle_orientation_to_surface(SimulationSource *source_, int32 longitudinal_height_offset_);
void
initialize_vehicle_surface_vectors(SimulationSource *source_);
void
initialize_alternate_vehicle_surface_vectors(SimulationSource *source_);
void
project_vehicle_surface_vectors(SimulationSource *source_, uint8 use_alternate_state_);
void
update_vehicle_track_motion(SimulationSource *source_, int32 update_enabled_);

typedef struct SimulationViewConfiguration
{
  int32   view_angle_degrees;
  int32   viewport_size;
  int32   horizontal_origin;
  int32   vertical_origin;
  int32   height;
  int32   vertical_center;
  int32   projection_scale;
  uint32 *depth_scale_table;
} SimulationViewConfiguration;

typedef struct SimulationModeConfiguration
{
  int32   view_configuration_index;
  Vector3 transition_vector;
  int32   movement_scale;
  int32   uses_track_geometry;
  int32   normal_next_mode;
  int32   exit_next_mode;
  int32   mode_parameter_c;
  int32   transition_shift;
  uint32  current_flag;
  int32   interpolation_frames;
  int32   next_mode;
  int32   vertical_threshold;
  Vector3 alternate_transition_vector;
  int32   alternate_transition_enabled;
} SimulationModeConfiguration;

typedef struct SimulationRenderState
{
  uint32                             mode;
  uint32                             requested_mode;
  uint32                             mode_elapsed;
  uint32                             interpolation_frames;
  Vector3                            current_position;
  Vector3                            base_position;
  const Vector3                     *position_source;
  Vector3                            position_offset;
  uint32                             track_segment_index;
  uint32                             position_flags;
  Vector3                            current_vector;
  Vector3                            base_vector;
  const Vector3                     *vector_source;
  Vector3                            vector_offset;
  uint8                              current_flag;
  uint8                              reserved_69[3];
  Vector3                            transition_vector;
  uint8                              reserved_78[0x0C];
  const SimulationViewConfiguration *view_configuration;
  const SimulationViewConfiguration *secondary_view_configuration;
  const SimulationSource            *source;
  uint8                              reserved_90[0x48];
  uint32                             render_mode;
  Vector3                            render_position;
  Vector3                            render_vector;
  uint8                              render_flag;
  uint8                              reserved_f5[3];
  uint32                             render_track_segment_index;
  uint32                             render_position_flags;
  const SimulationViewConfiguration *render_view_configuration;
  const SimulationViewConfiguration *render_secondary_view_configuration;
} SimulationRenderState;

extern int32   g_SIMULATION_MODE_TRANSITION_COOLDOWN;
extern int32   g_SIMULATION_MODE_CYCLE_AVAILABLE;
extern uint32 *g_SIMULATION_PRIMARY_DEPTH_SCALE_TABLE;
extern uint32  g_SIMULATION_SECONDARY_DEPTH_SCALE_TABLE[SIMULATION_DEPTH_SCALE_TABLE_COUNT];
extern SimulationRenderState g_SIMULATION_RENDER_STATE;
extern SimulationModeConfiguration
  g_SIMULATION_MODE_CONFIGURATIONS[SIMULATION_MODE_CONFIGURATION_COUNT];
extern SimulationViewConfiguration
  g_SIMULATION_VIEW_CONFIGURATIONS[SIMULATION_VIEW_CONFIGURATION_COUNT];

void
initialize_simulation_state(SimulationRenderState  *state_,
                            const Vector3          *position_,
                            const Vector3          *vector_,
                            const SimulationSource *source_,
                            int32                   mode_);
void
initialize_simulation_state_from_source(SimulationRenderState  *state_,
                                        const SimulationSource *source_,
                                        int32                   mode_);
void
set_simulation_track_segment(int32 track_segment_index_);
void
initialize_replay_simulation_view(int32 view_context_, int32 request_code_);
void
update_simulation_state(SimulationRenderState *state_);
void
initialize_simulation_projection_tables(void);
void
rebuild_simulation_view_matrices(void);
void
configure_simulation_projection(SimulationProjectionSource *source_,
                                uint8                       use_secondary_projection_);
void
update_simulation_mode_state(const struct TrackOrientationState *orientation_);
void
update_simulation_mode_selection(SimulationRenderState *state_);
void
set_simulation_mode(SimulationRenderState *state_, int32 mode_);
void
capture_simulation_render_state(SimulationRenderState *state_);
