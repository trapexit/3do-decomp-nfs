#pragma once

#include "doubly_linked_list.h"
#include "simulation_snapshot.h"

struct RenderResourceRecord;

#define VEHICLE_SIMULATION_STATE_PLAYER 0
#define VEHICLE_SIMULATION_STATE_MODE_FIVE_TARGET 1
#define VEHICLE_SIMULATION_STATE_MODE_SIX_TARGET 2
#define VEHICLE_SIMULATION_STATE_COUNT 7
#define VEHICLE_SIMULATION_DISABLED_MODE 8
#define VEHICLE_SIMULATION_INITIAL_MODE 5
#define SOUND_EFFECT_REQUEST_BUCKET_COUNT 480
#define SOUND_EFFECT_REQUEST_RANGE_COUNT 30
#define REPLAY_SIMULATION_AUTOMATIC_SNAPSHOT_COUNT 24
#define REPLAY_SIMULATION_MANUAL_SNAPSHOT_COUNT 4
#define REPLAY_SIMULATION_SNAPSHOT_COUNT                                                           \
  (REPLAY_SIMULATION_AUTOMATIC_SNAPSHOT_COUNT + REPLAY_SIMULATION_MANUAL_SNAPSHOT_COUNT)

typedef struct SoundEffectRequestRange
{
  int32 first_frame;
  int32 last_frame;
  int32 request_code;
} SoundEffectRequestRange;

typedef struct VehicleSimulationRefreshState
{
  uint8 reserved_00[0x44];
  int32 refresh_pending;
  uint8 reserved_48[0x38];
  int32 control_state;
  int32 activation_countdown;
  int32 activation_period;
  int32 pending_state;
  int32 randomized_speed;
} VehicleSimulationRefreshState;

typedef struct VehicleSimulationState
{
  DoublyLinkedNode             node;
  int32                        reserved_008;
  SimulationSource             simulation_source;
  struct RenderResourceRecord *render_resource_state;
  int32                        render_detail_level;
  void                        *render_object;
  uint8                      **render_shape_pointers;
} VehicleSimulationState;

typedef struct VehicleSimulationSnapshot
{
  DoublyLinkedNode              node;
  int32                         reserved_008;
  void                         *state_update_callback;
  void                         *render_update_callback;
  uint32                        runtime_configuration;
  struct RenderResourceRecord  *render_resource_state;
  Vector3                       position;
  Vector3                       direction;
  int32                         heading_rotation;
  int32                         transform_active;
  Matrix3x3                     track_transform;
  int32                         track_segment_index;
  Vector3                       motion;
  int32                         longitudinal_acceleration;
  int32                         reserved_074;
  int32                         pitch_motion_scale;
  int32                         speed_magnitude;
  int32                         heading_velocity;
  int32                         longitudinal_axle_span;
  int32                         lateral_axle_span;
  int32                         lateral_position;
  int32                         render_height_offset;
  Matrix3x3                     track_basis;
  Vector3                       track_reference_position;
  uint32                        status_flags;
  int32                         controller_position_offset;
  VehicleSimulationRefreshState refresh_state;
  int32                         minimum_track_speed;
} VehicleSimulationSnapshot;

typedef struct PlayerVehicleSimulationSnapshot
{
  VehicleSimulationSnapshot simulation;
  uint8                     audio_pan_target;
  uint8                     reserved_165;
  uint8                     traffic_brake_indicator;
  uint8                     reserved_167;
  uint32                    packed_audio_engine_speeds;
  int32                     surface_height_scale;
  int32                     dashboard_speed_value;
  int32                     reserved_174;
  int32                     reserved_060;
  int32                     reserved_458;
  uint8                     reserved_424;
  uint8                     encoded_surface_index;
  uint8                     state_flags;
  uint8                     reserved_183;
  int32                     recovery_sound_counter;
  int32                     reserved_410;
  Vector3                   track_gravity;
  int32                     reserved_420;
} PlayerVehicleSimulationSnapshot;

typedef struct TrafficVehicleSimulationSnapshot
{
  VehicleSimulationSnapshot simulation;
  int32                     target_track_speed;
  int32                     track_position_offset;
  uint8                     art_variant;
  uint8                     reserved_16_d[3];
} TrafficVehicleSimulationSnapshot;

typedef struct ReplaySimulationSnapshot
{
  DoublyLinkedListStorage active_vehicle_records;
  uint8                   reserved_24[8];
  int32                   vehicle_count;
  int32                   traffic_coordination_countdown;
  int32                   collision_simulation_enabled;
  int32                   collision_update_pending;
  int32                   vehicle_surface_index;
  int32                   traffic_state_secondary;
  uint8                   secondary_vehicle_sound_suppressed;
  uint8                   reserved_45[3];
  uint32                  vehicle_spawn_random_low_word;
  int32                   traffic_state_primary;
  int32                   reverse_traffic_count;
  int32                   forward_traffic_count;
  int32                   active_vehicle_count;
  int32                   runtime_pool_preserved_state;
  int32                   vehicle_collision_score;
  uint8                   vehicle_state_flags;
  uint8                   vehicle_event_state;
  uint8                   reserved_66[2];
} ReplaySimulationSnapshot;

extern int32                    g_OPPONENT_VEHICLE_INDEX;
extern int32                    g_VEHICLE_COLLISION_UPDATE_PENDING;
extern int32                    g_ACTIVE_SIMULATION_SOUND_INDEX;
extern int32                    g_ACTIVE_SIMULATION_SOUND_VARIANT;
extern uint8                    g_SECONDARY_VEHICLE_SOUND_SUPPRESSED;
extern int32                    g_PLAYER_VEHICLE_ACCELERATION_RATE;
extern int32                    g_SOUND_EFFECT_SELECTED_BUCKETS[SOUND_EFFECT_REQUEST_RANGE_COUNT];
extern SoundEffectRequestRange  g_SOUND_EFFECT_REQUEST_RANGES[SOUND_EFFECT_REQUEST_RANGE_COUNT];
extern int32                    g_SOUND_EFFECT_REQUEST_BUCKETS[SOUND_EFFECT_REQUEST_BUCKET_COUNT];
extern VehicleSimulationState  *g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_COUNT];
extern ReplaySimulationSnapshot g_REPLAY_SIMULATION_SNAPSHOTS[REPLAY_SIMULATION_SNAPSHOT_COUNT];
extern int32                    g_VEHICLE_SNAPSHOT_EVENT_STATE;

int32
is_mode_five_vehicle_track_gap_in_range(int32 minimum_gap_, int32 maximum_gap_);
int32
is_mode_six_vehicle_track_gap_in_range(int32 minimum_gap_, int32 maximum_gap_);
void
request_vehicle_render_sort_refresh(void);
void
request_sound_effect(int32 sound_id_);
void
prepare_sound_effect_request_ranges(void);
void
record_control_history_frame(void);
void
load_control_history_frame(void);
void
replay_control_history_frame(void);
void
capture_vehicle_simulation_snapshot(const VehicleSimulationState *state_,
                                    VehicleSimulationSnapshot    *snapshot_);
void
restore_vehicle_simulation_snapshot(VehicleSimulationState          *state_,
                                    const VehicleSimulationSnapshot *snapshot_);
void
restore_vehicle_simulation_snapshot_set(int32 restore_player_,
                                        int32 snapshot_index_,
                                        int32 traffic_index_);
void
capture_vehicle_simulation_snapshot_set(const VehicleSimulationState *state_, int32 traffic_index_);
void
restore_replay_simulation_snapshot(int32 frame_, int32 snapshot_slot_);
void
capture_replay_simulation_snapshot(void);
int32
prepare_replay_simulation_frame(int32 replay_frame_, int32 frame_limit_);
void
apply_replay_simulation_frame(void);
void
begin_recent_replay(void);
void
seek_replay_from_input(uint32 buttons_, int32 repeat_count_);
void
handle_replay_sequence_request(uint32 buttons_);
void
select_replay_view_from_input(uint32 buttons_, int32 repeat_count_);
void
process_replay_control_input(void);
void
resume_replay_from_track_slot(void);
void
advance_replay_sequence(void);
void
update_replay_playback(void);
void
update_replay_runtime(void);
void
apply_control_sequence_vehicle_impulses(void);
void
clear_vehicle_simulation_contact_flags(int32 clear_optional_flag_);
void
reset_vehicle_collision_simulation(void);
void
initialize_vehicle_simulation_position(SimulationSource *source_,
                                       int32             track_segment_index_,
                                       int32             placement_mode_,
                                       int32             position_offset_,
                                       int32             activate_record_);
void
update_traffic_vehicle_simulation(void *state_);
void
update_recovering_vehicle_simulation(void *state_);
void
rebuild_vehicle_track_frame(SimulationSource *source_);
void
reposition_vehicle_simulation(SimulationSource *source_, int32 activate_record_);
void
reposition_vehicle_at_track_boundary(SimulationSource *source_, int32 use_positive_boundary_);
int32
calculate_vehicle_track_speed(SimulationSource *source_);
void
update_vehicle_simulation_transform(SimulationSource *source_);
void
update_vehicle_dynamics(SimulationSource *source_, int32 control_state_);
int32
limit_vehicle_lateral_acceleration(SimulationSource *source_,
                                   int32             direction_,
                                   int32             acceleration_);
void
adjust_vehicle_lateral_velocity(SimulationSource *source_);
