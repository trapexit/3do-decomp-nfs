#include "vehicle_simulation_state.h"

#include "control_input.h"
#include "control_input_poll_state.h"
#include "display_coordinate_state.h"
#include "driving_warning_display.h"
#include "frontend_transition.h"
#include "game_bootstrap.h"
#include "game_frame_count.h"
#include "game_resource_cleanup.h"
#include "memory.h"
#include "random_number.h"
#include "render_thread_signal.h"
#include "road_data.h"
#include "road_processing.h"
#include "signal_status.h"
#include "simulation_resource_cleanup.h"
#include "simulation_thread_signal.h"
#include "tire_smoke.h"
#include "track_configuration_slots.h"
#include "track_position.h"
#include "vbl_delay.h"
#include "vehicle_control_input.h"
#include "vehicle_drive_initialization.h"
#include "vehicle_drive_statistics.h"
#include "vehicle_dynamics_state.h"
#include "vehicle_index_slots.h"
#include "vehicle_pair_collision.h"
#include "vehicle_parameter_loader.h"
#include "vehicle_runtime_pool.h"
#include "vehicle_traffic_lane.h"
#include "vehicle_traffic_randomization.h"

#include "mem.h"
#include "operamath.h"

#include "stddef.h"

#define COMPILE_TIME_SIZE_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]
#define VEHICLE_MODE_SIX_TARGET_ACTIVE_FLAG_MASK 0x00042400
#define VEHICLE_SIMULATION_OPTIONAL_CONTACT_FLAG 0x00080000
#define VEHICLE_SIMULATION_CONTACT_FLAG_MASK 0x00002400
#define VEHICLE_ACTIVE_SIMULATION_CONTACT_FLAG_MASK 0x00012400
#define VEHICLE_SIMULATION_CONTACT_RESET_FLAG 0x00000004
#define VEHICLE_SIMULATION_TRANSFORM_STALE_FLAG 0x00000200
#define VEHICLE_SIMULATION_KEEP_OPTIONAL_FLAG 0
#define VEHICLE_SIMULATION_TRANSFORM_SOURCE_OFFSET 0x0C
#define VEHICLE_SIMULATION_COLLISION_TRANSFORM_CONTROL_VALUE 60
#define VEHICLE_SIMULATION_PLAYER_TRANSFORM_CONTROL_VALUE 400
#define VEHICLE_SIMULATION_TRANSFORM_SOUND_ID 0x5C
#define VEHICLE_SIMULATION_GRAVITY_FIXED 0x9CF5C
#define VEHICLE_SIMULATION_ANGULAR_VELOCITY_SCALE 0x648
#define VEHICLE_SIMULATION_ANGULAR_VELOCITY_RECIPROCAL 0x28BE63
#define VEHICLE_SIMULATION_FIXED8_SHIFT 8
#define VEHICLE_SIMULATION_ANGULAR_SMOOTHING_SHIFT 1
#define VEHICLE_SIMULATION_ACCELERATION_X_INDEX 0
#define VEHICLE_SIMULATION_ACCELERATION_Y_INDEX 1
#define VEHICLE_SIMULATION_ACCELERATION_Z_INDEX 2
#define VEHICLE_SIMULATION_TRANSFORM_SOURCE_BYTES 0xB8
#define VEHICLE_SIMULATION_TRANSFORM_WRITEBACK_BYTES 0x4C8
#define VEHICLE_SIMULATION_REFRESH_STATE_BYTES 0x94
#define VEHICLE_SIMULATION_STATE_BYTES 0x4E4
#define SOUND_EFFECT_BUCKET_FRAMES 60
#define SOUND_EFFECT_PRIORITY_SHIFT 8
#define SOUND_EFFECT_ID_MASK 0xFF
#define SOUND_EFFECT_PENDING_FLAG 0x80
#define SOUND_EFFECT_REQUEST_LEAD_FRAMES 120
#define SOUND_EFFECT_REQUEST_TRAIL_FRAMES 300
#define SOUND_EFFECT_HISTORY_FRAMES (SOUND_EFFECT_REQUEST_BUCKET_COUNT * SOUND_EFFECT_BUCKET_FRAMES)
#define SOUND_EFFECT_HISTORY_SKIP_BUCKETS 40
#define SOUND_EFFECT_MINIMUM_BUCKET_INDEX -2
#define CONTROL_HISTORY_VALUE_FRAME_COUNT 4
#define CONTROL_HISTORY_NIBBLE_FRAME_COUNT 8
#define CONTROL_HISTORY_NIBBLE_MASK 0x0F
#define CONTROL_HISTORY_NIBBLE_SHIFT 4
#define CONTROL_HISTORY_VALUE_SECONDARY_MASK 0x03
#define CONTROL_HISTORY_JOYSTICK_PRESET_INDEX 5
#define CONTROL_HISTORY_AXIS_OFFSET 32
#define CONTROL_HISTORY_AXIS_SCALE 4
#define CONTROL_HISTORY_SECONDARY_OFFSET 30
#define CONTROL_HISTORY_SECONDARY_MAXIMUM 60
#define CONTROL_HISTORY_BUTTON_CODE_FIRST 61
#define CONTROL_HISTORY_BUTTON_CODE_SECOND 62
#define CONTROL_HISTORY_BUTTON_CODE_THIRD 63
#define CONTROL_HISTORY_ANY_BUTTON_MASK 0xA0700000
#define CONTROL_HISTORY_THIRD_BUTTON_MASK 0x80200000
#define CONTROL_HISTORY_SECOND_BUTTON_MASK 0x20100000
#define CONTROL_HISTORY_PRIMARY_SHIFT 24
#define CONTROL_HISTORY_SECONDARY_SHIFT 20
#define VEHICLE_SNAPSHOT_SLOT_INDEX_SHIFT 24
#define VEHICLE_SNAPSHOT_RUNTIME_FLAGS_SHIFT 8
#define VEHICLE_SNAPSHOT_RUNTIME_FLAGS_MASK 0xFFFF
#define VEHICLE_SNAPSHOT_CONFIGURATION_MODE_MASK 0xFF
#define VEHICLE_SNAPSHOT_FRAME_INTERVAL 1200
#define VEHICLE_SNAPSHOT_CAPTURE_COUNTDOWN 300
#define REPLAY_FRAME_WRAP_INTERVAL 28800
#define REPLAY_WARNING_TIMER_INITIAL_VALUE -1000
#define REPLAY_VIEW_CONTEXT_ALTERNATE 3
#define REPLAY_MODE_SELECTION_FRAME_MASK 0xFF
#define REPLAY_MODE_SELECTION_FRAME_VALUE 0xFC
#define REPLAY_SECONDARY_ROAD_RENDER_PENDING ((void *) 1)
#define REPLAY_RECENT_FRAME_OFFSET 8
#define REPLAY_FRAME_ALIGNMENT 4
#define REPLAY_SEQUENCE_INDEX_DISABLED -2
#define REPLAY_DEFAULT_VIEW_CONTEXT 4
#define REPLAY_DEFAULT_CONTROL_MODE 6
#define REPLAY_TRACK_BASIS_RENDER_MODE 3
#define REPLAY_TRACK_BASIS_MINIMUM_FIXED 0x1E0000
#define REPLAY_TRACK_BASIS_MAXIMUM_FIXED 0xDC0000
#define REPLAY_SEQUENCE_FIRST_BUTTONS 0x10600000U
#define REPLAY_SEQUENCE_FOURTH_BUTTONS 0x80600000U
#define REPLAY_SHORT_HISTORY_FRAMES 600
#define REPLAY_EXTENDED_HISTORY_FRAMES 27600
#define REPLAY_SEQUENCE_INPUT_MODE 4
#define REPLAY_SEQUENCE_INDEX_PENDING -1
#define REPLAY_VIEW_CONTEXT_ZERO_BUTTONS 0x04000000U
#define REPLAY_VIEW_CONTEXT_THREE_BUTTONS 0x06000000U
#define REPLAY_VIEW_CONTEXT_FOUR_BUTTONS 0x08000000U
#define REPLAY_VIEW_CONTEXT_ONE_BUTTONS 0x0A000000U
#define REPLAY_VIEW_CONTEXT_TWO_BUTTONS 0x0C000000U
#define REPLAY_VIEW_CONTEXT_ZERO_CONTROL_MODE 5
#define REPLAY_VIEW_CONTEXT_THREE_CONTROL_MODE 8
#define REPLAY_VIEW_CONTEXT_ONE_CONTROL_MODE 10
#define REPLAY_VIEW_CONTEXT_TWO_CONTROL_MODE 9
#define REPLAY_VIEW_CONTEXT_ZERO_PAUSE_MODE 2
#define REPLAY_VIEW_CONTEXT_ZERO 0
#define REPLAY_VIEW_CONTEXT_ONE 1
#define REPLAY_VIEW_CONTEXT_TWO 2
#define REPLAY_VIEW_CONTEXT_THREE 3
#define REPLAY_VIEW_CONTEXT_THREE_VARIANT 2
#define REPLAY_VIEW_CONTEXT_ONE_VARIANT 8
#define REPLAY_VIEW_CONTEXT_TWO_VARIANT 4
#define REPLAY_HISTORY_RECORDING_MODE 1
#define REPLAY_HISTORY_OFFSET_MODE 3
#define REPLAY_HISTORY_SEEK_MODE 4
#define REPLAY_HISTORY_PLAYBACK_MODE 2
#define REPLAY_SEEK_INPUT_MASK 0xF0000000U
#define REPLAY_SEEK_EXTENDED_START_BUTTONS 0x80000000U
#define REPLAY_SEEK_RECENT_START_BUTTONS 0x40000000U
#define REPLAY_SEEK_BACKWARD_BUTTONS 0x10000000U
#define REPLAY_SEEK_FORWARD_BUTTONS 0x20000000U
#define REPLAY_SEEK_FRAME_MARGIN 300
#define REPLAY_SEEK_STEP_SHIFT 4
#define REPLAY_SEEK_STEP_SCALE 4
#define REPLAY_SEEK_CONTROL_MODE_ABSOLUTE 4
#define REPLAY_SEEK_CONTROL_MODE_FORWARD 3
#define REPLAY_INPUT_REPEAT_SLOW_LIMIT 5
#define REPLAY_INPUT_REPEAT_MAXIMUM 1024
#define REPLAY_INPUT_REPEAT_SLOW_STEP 1
#define REPLAY_INPUT_REPEAT_FAST_STEP 4
#define REPLAY_SEQUENCE_UPDATE_COUNT -10
#define REPLAY_SEQUENCE_SECOND_BUTTONS 0x20600000U
#define REPLAY_RECENT_REQUEST_BUTTONS 0x01000000U
#define REPLAY_VIEW_TOGGLE_BUTTONS 0x02000000U
#define REPLAY_VIEW_SELECTION_MASK 0x0C000000U
#define REPLAY_ROTATE_POSITIVE_BUTTONS 0x00400000U
#define REPLAY_ROTATE_NEGATIVE_BUTTONS 0x00200000U
#define REPLAY_ROTATION_STEP 0x1999
#define REPLAY_ROTATION_WRAP 0x1000000
#define REPLAY_ROTATION_MASK 0xFFFFFF
#define REPLAY_ROTATION_MIDDLE 0x800000
#define REPLAY_ROTATION_LOW_LIMIT 0x100000
#define REPLAY_ROTATION_HIGH_LIMIT 0xF00000
#define REPLAY_RUNTIME_UPDATE_ENABLED_FLAG 4
#define REPLAY_SEQUENCE_RANDOM_RANGE 10
#define REPLAY_SEQUENCE_RANDOM_THRESHOLD 4
#define REPLAY_SEQUENCE_NEAR_TRACK_GAP 3
#define REPLAY_SEQUENCE_DISTANCE_REQUEST_CODE 228
#define REPLAY_SEQUENCE_VIEW_REQUEST_MINIMUM 218
#define REPLAY_SEQUENCE_VIEW_REQUEST_MAXIMUM 248
#define VEHICLE_SNAPSHOT_TRAFFIC_COUNT 6
#define VEHICLE_SNAPSHOT_AUDIO_SPEED_SHIFT 16
#define VEHICLE_SNAPSHOT_AUDIO_SPEED_MASK 0xFFFF
#define VEHICLE_SNAPSHOT_RESERVED_3F8_BIAS 100
#define VEHICLE_SNAPSHOT_SURFACE_INDEX_BIAS 2
#define VEHICLE_SNAPSHOT_HIGH_DETAIL_FLAG_MASK 0x20
#define VEHICLE_SNAPSHOT_HIGH_DETAIL_SHAPE_FLAG 2
#define VEHICLE_SNAPSHOT_LANE_CHANGE_FLAG_SHIFT 4
#define VEHICLE_SNAPSHOT_SURFACE_CONTACT_FLAG_SHIFT 3
#define VEHICLE_SNAPSHOT_WARNING_STATE_FLAG_SHIFT 2
#define VEHICLE_SNAPSHOT_WARNING_INDEX_FLAG_SHIFT 1
#define VEHICLE_SNAPSHOT_STATE_FLAG_MASK 1
#define VEHICLE_SNAPSHOT_RESTORE_SURFACE_FLAG 1
#define VEHICLE_SNAPSHOT_RESISTANCE_REDUCTION_FLAG 2
#define VEHICLE_SNAPSHOT_RESTORE_FALLBACK_MODE 4
#define VEHICLE_SNAPSHOT_MODE_FIVE_TARGET 5
#define VEHICLE_SNAPSHOT_MODE_SIX_TARGET 6
#define VEHICLE_SNAPSHOT_TARGET_TRACK_GAP_MAXIMUM 5
#define VEHICLE_SNAPSHOT_SOUND_REQUESTS_ENABLED 1
#define VEHICLE_SNAPSHOT_SOUND_REQUESTS_REPLAY_DISABLED 2
#define VEHICLE_SNAPSHOT_SOUND_REQUESTS_REPLAY_OFFSET 3
#define VEHICLE_CONTROL_SEQUENCE_COMPLETE_STAGE 4
#define VEHICLE_CONTROL_SEQUENCE_IMPULSE_START_FRAME 301
#define VEHICLE_CONTROL_SEQUENCE_FIRST_TARGET_INDEX 1
#define VEHICLE_CONTROL_SEQUENCE_IMPULSE 0xA0000
#define VEHICLE_COLLISION_RESET_FIRST_INDEX 1
#define VEHICLE_COLLISION_RESET_FLAG_MASK 0x00082400
#define VEHICLE_COLLISION_PLAYER_FLAG 0x00000004
#define VEHICLE_COLLISION_TRAFFIC_FLAG 0x00000008
#define VEHICLE_COLLISION_REVERSE_FLAG 0x00001000
#define VEHICLE_COLLISION_TRACK_WIDTH_SHIFT 13

extern void
update_player_vehicle_state(void *state_);

typedef struct VehicleSimulationContactState
{
  uint8  reserved_000[0x188];
  uint32 status_flags;
} VehicleSimulationContactState;

typedef struct VehicleSimulationImpulseState
{
  uint8      reserved_000[0x3C];
  Vector3    accumulated_impulse;
  uint8      reserved_048[0x78];
  Matrix3x3 *transform;
  uint8      reserved_0_c4[0xC4];
  uint32     status_flags;
} VehicleSimulationImpulseState;

typedef struct VehicleSimulationTransformState
{
  uint8                    reserved_000[0xC0];
  MatrixControllerState   *transform;
  uint8                    reserved_0_c4[0xC4];
  uint32                   status_flags;
  uint8                    reserved_18_c[0x338];
  SimulationUpdateCallback update_callback;
} VehicleSimulationTransformState;

typedef struct VehicleSimulationTransformSource
{
  Vector3                position;
  Vector3                rotation;
  uint8                  reserved_018[4];
  int32                  transform_override_enabled;
  Matrix3x3              transform_override;
  uint8                  reserved_044[4];
  Vector3                velocity;
  uint8                  reserved_054[0x14];
  int32                  angular_velocity;
  uint8                  reserved_06_c[0x44];
  int32                  position_offset;
  MatrixControllerState *transform;
} VehicleSimulationTransformSource;

typedef struct VehicleSimulationTransformWritebackState
{
  uint8                            reserved_000[0x0C];
  VehicleSimulationTransformSource transform_source;
  uint8                            reserved_0_c4[0x38C];
  int32                            primary_sound_level;
  int32                            secondary_sound_level;
  uint8                            reserved_458[0x6C];
  SimulationUpdateCallback         update_callback;
} VehicleSimulationTransformWritebackState;

typedef struct VehicleCollisionResetState
{
  uint8                    reserved_000[0x1C];
  int32                    transform_active;
  uint8                    reserved_020[4];
  int32                    heading_rotation;
  uint8                    reserved_028[4];
  Matrix3x3                track_transform;
  int32                    track_segment_index;
  Vector3                  motion;
  uint8                    reserved_060[0x14];
  int32                    heading_velocity;
  uint8                    reserved_078[8];
  int32                    lateral_position;
  uint8                    reserved_084[8];
  Matrix3x3                track_basis;
  uint8                    reserved_0_b0[0xA8];
  int32                    minimum_track_speed;
  uint8                    reserved_15_c[0x2C];
  uint32                   status_flags;
  uint8                    reserved_18_c[0x338];
  SimulationUpdateCallback update_callback;
} VehicleCollisionResetState;

int32                    g_OPPONENT_VEHICLE_INDEX = VEHICLE_SIMULATION_INITIAL_MODE;
int32                    g_VEHICLE_COLLISION_UPDATE_PENDING;
int32                    g_VEHICLE_SNAPSHOT_EVENT_STATE;
int32                    g_ACTIVE_SIMULATION_SOUND_INDEX   = -1;
int32                    g_ACTIVE_SIMULATION_SOUND_VARIANT = -1;
uint8                    g_SECONDARY_VEHICLE_SOUND_SUPPRESSED;
uint8                    g_SECONDARY_VEHICLE_SOUND_PADDING[3];
int32                    g_SOUND_EFFECT_SELECTED_BUCKETS[SOUND_EFFECT_REQUEST_RANGE_COUNT];
SoundEffectRequestRange  g_SOUND_EFFECT_REQUEST_RANGES[SOUND_EFFECT_REQUEST_RANGE_COUNT];
int32                    g_SOUND_EFFECT_REQUEST_BUCKETS[SOUND_EFFECT_REQUEST_BUCKET_COUNT];
ReplaySimulationSnapshot g_REPLAY_SIMULATION_SNAPSHOTS[REPLAY_SIMULATION_SNAPSHOT_COUNT];
VehicleSimulationState  *g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_COUNT] = {
  NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static int32
_is_vehicle_track_gap_in_range(int32 player_track_segment_,
                               int32 target_track_segment_,
                               int32 minimum_gap_,
                               int32 maximum_gap_);

static int32
_is_vehicle_within_reset_boundaries(const VehicleCollisionResetState *state_);

static void
_reset_vehicle_collision_state(VehicleCollisionResetState *state_);

void
request_sound_effect(int32 sound_id_)
{
  int32 bucket_index;
  int32 frame_priority;
  int32 request;

  if(g_GAME_RESOURCE_STATE.sound_requests_enabled != 1)
    return;

  g_GAME_RESOURCE_STATE.last_sound_request_id    = sound_id_;
  g_GAME_RESOURCE_STATE.last_sound_request_frame = G_GAME_FRAME_COUNTER;
  bucket_index = (g_GAME_RESOURCE_STATE.sound_bucket_frame / SOUND_EFFECT_BUCKET_FRAMES);
  g_GAME_RESOURCE_STATE.sound_bucket_index = bucket_index;
  frame_priority                           = (G_GAME_FRAME_COUNTER / SOUND_EFFECT_BUCKET_FRAMES);
  request                                  = g_SOUND_EFFECT_REQUEST_BUCKETS[bucket_index];
  if(frame_priority > (request >> SOUND_EFFECT_PRIORITY_SHIFT))
    {
      g_SOUND_EFFECT_REQUEST_BUCKETS[bucket_index] = 0;
      request                                      = 0;
    }
  if((request & SOUND_EFFECT_ID_MASK) < sound_id_)
    {
      request = (int32) ((uint32) sound_id_ + SOUND_EFFECT_PENDING_FLAG);
      request |= (int32) ((uint32) frame_priority << SOUND_EFFECT_PRIORITY_SHIFT);
      g_SOUND_EFFECT_REQUEST_BUCKETS[bucket_index] = request;
    }
}

void
prepare_sound_effect_request_ranges(void)
{
  SoundEffectRequestRange *current_range;
  SoundEffectRequestRange *previous_range;
  int32                    bucket_count;
  int32                    minimum_bucket;
  int32                    selected_count;
  int32                    selection_pass;
  int32                    bucket_index;
  int32                    selected_bucket;
  int32                    selected_priority;
  int32                    request;
  int32                    priority;
  int32                    sort_pass;
  int32                    sort_index;
  int32                    swap_value;
  int32                    swapped;
  int32                    range_index;

  bucket_count = (g_DRIVING_WARNING_METER_STATE.input_value / SOUND_EFFECT_BUCKET_FRAMES);
  if(bucket_count > SOUND_EFFECT_REQUEST_BUCKET_COUNT)
    bucket_count = SOUND_EFFECT_REQUEST_BUCKET_COUNT;

  minimum_bucket = ((g_DRIVING_WARNING_METER_STATE.input_value - SOUND_EFFECT_HISTORY_FRAMES) /
                    SOUND_EFFECT_BUCKET_FRAMES);
  if(minimum_bucket >= SOUND_EFFECT_MINIMUM_BUCKET_INDEX)
    minimum_bucket += SOUND_EFFECT_HISTORY_SKIP_BUCKETS;
  else
    minimum_bucket = SOUND_EFFECT_MINIMUM_BUCKET_INDEX;

  selected_count = 0;
  for(selection_pass = 0; selection_pass < SOUND_EFFECT_REQUEST_RANGE_COUNT; selection_pass += 1)
    {
      selected_priority = SOUND_EFFECT_PENDING_FLAG;
      selected_bucket   = 0;
      for(bucket_index = 0; bucket_index < bucket_count; bucket_index += 1)
        {
          request  = g_SOUND_EFFECT_REQUEST_BUCKETS[bucket_index];
          priority = request & SOUND_EFFECT_ID_MASK;
          if((priority >= selected_priority) && (bucket_index >= minimum_bucket))
            {
              selected_priority                               = priority;
              selected_bucket                                 = bucket_index;
              g_SOUND_EFFECT_SELECTED_BUCKETS[selected_count] = bucket_index;
            }
        }

      if(selected_priority > SOUND_EFFECT_PENDING_FLAG)
        {
          g_SOUND_EFFECT_REQUEST_BUCKETS[selected_bucket] -= SOUND_EFFECT_PENDING_FLAG;
          selected_count += 1;
        }
    }

  for(bucket_index = 0; bucket_index < bucket_count; bucket_index += 1)
    {
      request  = g_SOUND_EFFECT_REQUEST_BUCKETS[bucket_index];
      priority = request & SOUND_EFFECT_ID_MASK;
      if((priority > 0) && (priority < SOUND_EFFECT_PENDING_FLAG))
        g_SOUND_EFFECT_REQUEST_BUCKETS[bucket_index] += SOUND_EFFECT_PENDING_FLAG;
    }

  for(sort_pass = 0; sort_pass < selected_count; sort_pass += 1)
    {
      swapped = 0;
      for(sort_index = 1; sort_index < selected_count; sort_index += 1)
        {
          if(g_SOUND_EFFECT_SELECTED_BUCKETS[sort_index] <
             g_SOUND_EFFECT_SELECTED_BUCKETS[sort_index - 1])
            {
              swap_value = g_SOUND_EFFECT_SELECTED_BUCKETS[sort_index];
              g_SOUND_EFFECT_SELECTED_BUCKETS[sort_index] =
                g_SOUND_EFFECT_SELECTED_BUCKETS[sort_index - 1];
              g_SOUND_EFFECT_SELECTED_BUCKETS[sort_index - 1] = swap_value;
              swapped                                         = 1;
            }
        }

      if(swapped == 0)
        break;
    }

  g_GAME_RESOURCE_STATE.sound_effect_request_range_count = 0;
  for(range_index = 0; range_index < selected_count; range_index += 1)
    {
      selected_bucket = g_SOUND_EFFECT_SELECTED_BUCKETS[range_index];
      if((selected_bucket - (SOUND_EFFECT_REQUEST_LEAD_FRAMES / SOUND_EFFECT_BUCKET_FRAMES)) <
         minimum_bucket)
        continue;

      current_range =
        &g_SOUND_EFFECT_REQUEST_RANGES[g_GAME_RESOURCE_STATE.sound_effect_request_range_count];
      current_range->first_frame =
        (selected_bucket * SOUND_EFFECT_BUCKET_FRAMES) - SOUND_EFFECT_REQUEST_LEAD_FRAMES;
      current_range->last_frame =
        (selected_bucket * SOUND_EFFECT_BUCKET_FRAMES) + SOUND_EFFECT_REQUEST_TRAIL_FRAMES;
      current_range->request_code =
        g_SOUND_EFFECT_REQUEST_BUCKETS[selected_bucket] & SOUND_EFFECT_ID_MASK;
      if(current_range->first_frame < 0)
        current_range->first_frame = 0;
      if(current_range->last_frame > g_DRIVING_WARNING_METER_STATE.input_value)
        current_range->last_frame = g_DRIVING_WARNING_METER_STATE.input_value;

      g_GAME_RESOURCE_STATE.sound_effect_request_range_count += 1;
      if(g_GAME_RESOURCE_STATE.sound_effect_request_range_count <= 1)
        continue;

      previous_range = current_range - 1;
      if(current_range->first_frame < previous_range->last_frame)
        {
          previous_range->last_frame = current_range->last_frame;
          if(current_range->request_code > previous_range->request_code)
            previous_range->request_code = current_range->request_code;
          g_GAME_RESOURCE_STATE.sound_effect_request_range_count -= 1;
        }
    }
}

void
record_control_history_frame(void)
{
  uint32 buttons;
  int32  frame;
  int32  value_index;
  int32  nibble_index;
  int32  value;
  int32  secondary;

  if(g_GAME_RESOURCE_STATE.control_history_initialized == 0)
    {
      g_GAME_RESOURCE_STATE.control_history_initialized = 1;
      g_GAME_RESOURCE_STATE.traction_control_enabled    = g_TRACTION_CONTROL_ENABLED;
    }

  if(g_STARTUP_CONFIGURATION.control_preset_index != CONTROL_HISTORY_JOYSTICK_PRESET_INDEX)
    {
      buttons                                               = g_CONTROL_INPUT_POLL_STATE.status_00;
      g_CONTROL_INPUT_POLL_STATE.simulation_control_buttons = buttons;
      g_CONTROL_INPUT_POLL_STATE.simulation_start_scale =
        ((int32) buttons >> CONTROL_HISTORY_PRIMARY_SHIFT);
      secondary =
        ((int32) buttons >> CONTROL_HISTORY_SECONDARY_SHIFT) & CONTROL_HISTORY_NIBBLE_MASK;
      g_CONTROL_INPUT_POLL_STATE.simulation_start_delay = secondary;
      value = g_CONTROL_INPUT_POLL_STATE.simulation_start_scale;
    }
  else
    {
      g_CONTROL_INPUT_POLL_STATE.simulation_start_scale =
        ((g_JOYSTICK_AXIS_VALUES[0] + CONTROL_HISTORY_AXIS_OFFSET) * CONTROL_HISTORY_AXIS_SCALE);
      buttons = g_JOYSTICK_CALIBRATION_STATE.buttons;
      if((buttons & CONTROL_HISTORY_ANY_BUTTON_MASK) == 0)
        {
          secondary = g_JOYSTICK_AXIS_VALUES[1] + CONTROL_HISTORY_SECONDARY_OFFSET;
          if(secondary > CONTROL_HISTORY_SECONDARY_MAXIMUM)
            secondary = CONTROL_HISTORY_SECONDARY_MAXIMUM;
        }
      else if((buttons & CONTROL_HISTORY_THIRD_BUTTON_MASK) != 0)
        {
          secondary = CONTROL_HISTORY_BUTTON_CODE_THIRD;
        }
      else if((buttons & CONTROL_HISTORY_SECOND_BUTTON_MASK) == 0)
        {
          secondary = CONTROL_HISTORY_BUTTON_CODE_FIRST;
        }
      else
        {
          secondary = CONTROL_HISTORY_BUTTON_CODE_SECOND;
        }

      g_CONTROL_INPUT_POLL_STATE.simulation_start_delay = secondary;
      value = g_CONTROL_INPUT_POLL_STATE.simulation_start_scale |
              (g_CONTROL_INPUT_POLL_STATE.simulation_start_delay >> CONTROL_HISTORY_NIBBLE_SHIFT);
    }

  frame       = g_GAME_RESOURCE_STATE.sound_bucket_frame;
  value_index = (frame / CONTROL_HISTORY_VALUE_FRAME_COUNT);
  g_GAME_RESOURCE_STATE.control_value_history[value_index] = (uint8) value;
  nibble_index = (frame / CONTROL_HISTORY_NIBBLE_FRAME_COUNT);
  if((value_index & 1) == 0)
    {
      g_GAME_RESOURCE_STATE.control_nibble_history[nibble_index] =
        (uint8) (g_CONTROL_INPUT_POLL_STATE.simulation_start_delay <<
                 CONTROL_HISTORY_NIBBLE_SHIFT);
    }
  else
    {
      g_GAME_RESOURCE_STATE.control_nibble_history[nibble_index] |=
        (uint8) (g_CONTROL_INPUT_POLL_STATE.simulation_start_delay & CONTROL_HISTORY_NIBBLE_MASK);
    }
}

void
load_control_history_frame(void)
{
  uint32 packed_value;
  uint32 packed_nibbles;
  uint32 secondary;
  int32  frame;
  int32  value_index;
  int32  nibble_index;

  frame          = g_GAME_RESOURCE_STATE.sound_bucket_frame;
  value_index    = (frame / CONTROL_HISTORY_VALUE_FRAME_COUNT);
  nibble_index   = (frame / CONTROL_HISTORY_NIBBLE_FRAME_COUNT);
  packed_value   = g_GAME_RESOURCE_STATE.control_value_history[value_index];
  packed_nibbles = g_GAME_RESOURCE_STATE.control_nibble_history[nibble_index];
  g_CONTROL_INPUT_POLL_STATE.simulation_start_scale = packed_value;
  if(g_STARTUP_CONFIGURATION.control_preset_index != CONTROL_HISTORY_JOYSTICK_PRESET_INDEX)
    {
      if((value_index & 1) == 0)
        secondary = (packed_nibbles >> CONTROL_HISTORY_NIBBLE_SHIFT);
      else
        secondary = (packed_nibbles & CONTROL_HISTORY_NIBBLE_MASK);

      g_CONTROL_INPUT_POLL_STATE.simulation_start_delay = secondary;
      g_CONTROL_INPUT_POLL_STATE.simulation_control_buttons =
        ((packed_value << CONTROL_HISTORY_PRIMARY_SHIFT) |
         ((g_CONTROL_INPUT_POLL_STATE.simulation_start_delay & CONTROL_HISTORY_NIBBLE_MASK)
          << CONTROL_HISTORY_SECONDARY_SHIFT));
      return;
    }

  secondary =
    ((packed_value & CONTROL_HISTORY_VALUE_SECONDARY_MASK) << CONTROL_HISTORY_NIBBLE_SHIFT);
  g_CONTROL_INPUT_POLL_STATE.simulation_start_delay = secondary;
  if((value_index & 1) == 0)
    secondary |= (packed_nibbles >> CONTROL_HISTORY_NIBBLE_SHIFT);
  else
    secondary |= (packed_nibbles & CONTROL_HISTORY_NIBBLE_MASK);
  g_CONTROL_INPUT_POLL_STATE.simulation_start_delay = secondary;
}

void
replay_control_history_frame(void)
{
  load_control_history_frame();
}

void
capture_vehicle_simulation_snapshot(const VehicleSimulationState *state_,
                                    VehicleSimulationSnapshot    *snapshot_)
{
  const VehicleRuntimeObjectState *runtime_state;
  const SimulationSource          *source;

  runtime_state                     = (const VehicleRuntimeObjectState *) state_;
  source                            = &state_->simulation_source;
  snapshot_->node                   = state_->node;
  snapshot_->reserved_008           = state_->reserved_008;
  snapshot_->state_update_callback  = runtime_state->state_update_callback;
  snapshot_->render_update_callback = runtime_state->render_update_callback;
  snapshot_->runtime_configuration =
    (((uint32) runtime_state->slot_index << VEHICLE_SNAPSHOT_SLOT_INDEX_SHIFT) |
     (runtime_state->runtime_flags << VEHICLE_SNAPSHOT_RUNTIME_FLAGS_SHIFT) |
     (uint32) runtime_state->configuration_mode);
  snapshot_->render_resource_state      = state_->render_resource_state;
  snapshot_->position                   = source->position;
  snapshot_->direction                  = source->direction;
  snapshot_->heading_rotation           = source->heading_rotation;
  snapshot_->transform_active           = source->transform_active;
  snapshot_->track_transform            = source->track_transform;
  snapshot_->track_segment_index        = source->track_segment_index;
  snapshot_->motion                     = source->motion;
  snapshot_->longitudinal_acceleration  = source->longitudinal_acceleration;
  snapshot_->reserved_074               = source->reserved_058;
  snapshot_->pitch_motion_scale         = source->pitch_motion_scale;
  snapshot_->speed_magnitude            = source->speed_magnitude;
  snapshot_->heading_velocity           = source->heading_velocity;
  snapshot_->longitudinal_axle_span     = source->longitudinal_axle_span;
  snapshot_->lateral_axle_span          = source->lateral_axle_span;
  snapshot_->lateral_position           = source->lateral_position;
  snapshot_->render_height_offset       = source->render_height_offset;
  snapshot_->track_basis                = source->track_basis;
  snapshot_->track_reference_position   = source->track_reference_position;
  snapshot_->status_flags               = source->status_flags;
  snapshot_->controller_position_offset = source->controller_position_offset;
  move_bytes((const uint8 *) source->refresh_state,
             (uint8 *) &snapshot_->refresh_state,
             VEHICLE_SIMULATION_REFRESH_STATE_BYTES);
  snapshot_->minimum_track_speed = source->minimum_track_speed;
}

void
restore_vehicle_simulation_snapshot(VehicleSimulationState          *state_,
                                    const VehicleSimulationSnapshot *snapshot_)
{
  VehicleRuntimeObjectState *runtime_state;
  SimulationSource          *source;

  runtime_state                         = (VehicleRuntimeObjectState *) state_;
  source                                = &state_->simulation_source;
  state_->node                          = snapshot_->node;
  state_->reserved_008                  = snapshot_->reserved_008;
  runtime_state->state_update_callback  = snapshot_->state_update_callback;
  runtime_state->render_update_callback = snapshot_->render_update_callback;
  state_->render_resource_state         = snapshot_->render_resource_state;
  runtime_state->slot_index =
    (int32) snapshot_->runtime_configuration >> VEHICLE_SNAPSHOT_SLOT_INDEX_SHIFT;
  runtime_state->runtime_flags =
    (snapshot_->runtime_configuration >> VEHICLE_SNAPSHOT_RUNTIME_FLAGS_SHIFT) &
    VEHICLE_SNAPSHOT_RUNTIME_FLAGS_MASK;
  runtime_state->configuration_mode =
    snapshot_->runtime_configuration & VEHICLE_SNAPSHOT_CONFIGURATION_MODE_MASK;
  source->position                   = snapshot_->position;
  source->direction                  = snapshot_->direction;
  source->heading_rotation           = snapshot_->heading_rotation;
  source->transform_active           = snapshot_->transform_active;
  source->track_transform            = snapshot_->track_transform;
  source->track_segment_index        = snapshot_->track_segment_index;
  source->motion                     = snapshot_->motion;
  source->longitudinal_acceleration  = snapshot_->longitudinal_acceleration;
  source->reserved_058               = snapshot_->reserved_074;
  source->pitch_motion_scale         = snapshot_->pitch_motion_scale;
  source->speed_magnitude            = snapshot_->speed_magnitude;
  source->heading_velocity           = snapshot_->heading_velocity;
  source->longitudinal_axle_span     = snapshot_->longitudinal_axle_span;
  source->lateral_axle_span          = snapshot_->lateral_axle_span;
  source->lateral_position           = snapshot_->lateral_position;
  source->render_height_offset       = snapshot_->render_height_offset;
  source->track_basis                = snapshot_->track_basis;
  source->track_reference_position   = snapshot_->track_reference_position;
  source->status_flags               = snapshot_->status_flags;
  source->controller_position_offset = snapshot_->controller_position_offset;
  move_bytes((const uint8 *) &snapshot_->refresh_state,
             (uint8 *) source->refresh_state,
             VEHICLE_SIMULATION_REFRESH_STATE_BYTES);
  source->minimum_track_speed                = snapshot_->minimum_track_speed;
  runtime_state->spatial_sound_gain          = 0;
  runtime_state->spatial_sound_configuration = 0;
}

void
restore_vehicle_simulation_snapshot_set(int32 restore_player_,
                                        int32 snapshot_index_,
                                        int32 traffic_index_)
{
  PlayerVehicleSimulationSnapshot  *player_snapshot;
  TrafficVehicleSimulationSnapshot *traffic_snapshot;
  VehicleSimulationState           *state;
  SimulationSource                 *source;
  uint8                             state_flags;
  int32                             slot_index;

  if(restore_player_ != 0)
    {
      player_snapshot = &((PlayerVehicleSimulationSnapshot *)
                            g_GAME_RESOURCE_STATE.allocation_third)[snapshot_index_];
      state           = G_ACTIVE_SIMULATION_SOURCE;
      restore_vehicle_simulation_snapshot(state, &player_snapshot->simulation);
      source                          = &state->simulation_source;
      source->audio_pan_target        = player_snapshot->audio_pan_target;
      source->reserved_3_f0           = player_snapshot->reserved_165;
      source->traffic_brake_indicator = player_snapshot->traffic_brake_indicator;
      source->surface_height_scale    = player_snapshot->surface_height_scale;
      source->dashboard_speed_value   = player_snapshot->dashboard_speed_value;
      source->reserved_40_c           = player_snapshot->reserved_174;
      source->track_gravity           = player_snapshot->track_gravity;
      source->reserved_420            = player_snapshot->reserved_420;
      source->reserved_410            = player_snapshot->reserved_410;
      source->reserved_458            = player_snapshot->reserved_458;
      source->reserved_060            = player_snapshot->reserved_060;
      source->reserved_424            = player_snapshot->reserved_424;
      source->reserved_3_f8 =
        (int32) player_snapshot->reserved_167 - VEHICLE_SNAPSHOT_RESERVED_3F8_BIAS;
      source->surface_index =
        (int32) player_snapshot->encoded_surface_index - VEHICLE_SNAPSHOT_SURFACE_INDEX_BIAS;
      source->audio_engine_speed_target =
        (int32) player_snapshot->packed_audio_engine_speeds >> VEHICLE_SNAPSHOT_AUDIO_SPEED_SHIFT;
      source->audio_engine_speed =
        player_snapshot->packed_audio_engine_speeds & VEHICLE_SNAPSHOT_AUDIO_SPEED_MASK;
      source->recovery_sound_counter = player_snapshot->recovery_sound_counter;
      state_flags                    = player_snapshot->state_flags;
      if((state_flags & VEHICLE_SNAPSHOT_HIGH_DETAIL_FLAG_MASK) == 0)
        source->high_detail_shape_flags[1] = 0;
      else
        source->high_detail_shape_flags[1] = VEHICLE_SNAPSHOT_HIGH_DETAIL_SHAPE_FLAG;
      source->lane_change_active =
        (state_flags >> VEHICLE_SNAPSHOT_LANE_CHANGE_FLAG_SHIFT) & VEHICLE_SNAPSHOT_STATE_FLAG_MASK;
      source->surface_contact_active =
        (state_flags >> VEHICLE_SNAPSHOT_SURFACE_CONTACT_FLAG_SHIFT) &
        VEHICLE_SNAPSHOT_STATE_FLAG_MASK;
      source->warning_display_state   = (state_flags >> VEHICLE_SNAPSHOT_WARNING_STATE_FLAG_SHIFT) &
                                        VEHICLE_SNAPSHOT_STATE_FLAG_MASK;
      source->warning_display_index   = (state_flags >> VEHICLE_SNAPSHOT_WARNING_INDEX_FLAG_SHIFT) &
                                        VEHICLE_SNAPSHOT_STATE_FLAG_MASK;
      source->warning_display_pending = state_flags & VEHICLE_SNAPSHOT_STATE_FLAG_MASK;
      source->high_detail_shape_flags[0] = 0;
      g_ACTIVE_SIMULATION_SOUND_INDEX    = -1;
      g_ACTIVE_SIMULATION_SOUND_VARIANT  = -1;
      return;
    }

  traffic_snapshot =
    &((TrafficVehicleSimulationSnapshot *) g_GAME_RESOURCE_STATE
        .allocation_fourth)[snapshot_index_ * VEHICLE_SNAPSHOT_TRAFFIC_COUNT + traffic_index_];
  slot_index =
    (int32) traffic_snapshot->simulation.runtime_configuration >> VEHICLE_SNAPSHOT_SLOT_INDEX_SHIFT;
  state = g_VEHICLE_SIMULATION_STATES[slot_index];
  restore_vehicle_simulation_snapshot(state, &traffic_snapshot->simulation);
  source                        = &state->simulation_source;
  source->target_track_speed    = traffic_snapshot->target_track_speed;
  source->track_position_offset = traffic_snapshot->track_position_offset;
  source->art_variant           = traffic_snapshot->art_variant;
}

void
capture_vehicle_simulation_snapshot_set(const VehicleSimulationState *state_, int32 traffic_index_)
{
  PlayerVehicleSimulationSnapshot  *player_snapshot;
  TrafficVehicleSimulationSnapshot *traffic_snapshot;
  const SimulationSource           *source;
  int32                             snapshot_index;

  source = &state_->simulation_source;
  if(state_ == G_ACTIVE_SIMULATION_SOURCE)
    {
      snapshot_index = (g_GAME_RESOURCE_STATE.sound_bucket_frame / VEHICLE_SNAPSHOT_FRAME_INTERVAL);
      player_snapshot = &(
        (PlayerVehicleSimulationSnapshot *) g_GAME_RESOURCE_STATE.allocation_third)[snapshot_index];
      capture_vehicle_simulation_snapshot(state_, &player_snapshot->simulation);
      player_snapshot->surface_height_scale    = source->surface_height_scale;
      player_snapshot->dashboard_speed_value   = source->dashboard_speed_value;
      player_snapshot->reserved_174            = source->reserved_40_c;
      player_snapshot->track_gravity           = source->track_gravity;
      player_snapshot->reserved_420            = source->reserved_420;
      player_snapshot->reserved_410            = source->reserved_410;
      player_snapshot->reserved_458            = source->reserved_458;
      player_snapshot->reserved_060            = source->reserved_060;
      player_snapshot->reserved_424            = (uint8) source->reserved_424;
      player_snapshot->audio_pan_target        = (uint8) source->audio_pan_target;
      player_snapshot->reserved_165            = (uint8) source->reserved_3_f0;
      player_snapshot->traffic_brake_indicator = (uint8) source->traffic_brake_indicator;
      player_snapshot->recovery_sound_counter  = source->recovery_sound_counter;
      player_snapshot->packed_audio_engine_speeds =
        ((uint32) source->audio_engine_speed |
         ((uint32) source->audio_engine_speed_target << VEHICLE_SNAPSHOT_AUDIO_SPEED_SHIFT));
      player_snapshot->state_flags =
        (uint8) ((source->lane_change_active << VEHICLE_SNAPSHOT_LANE_CHANGE_FLAG_SHIFT) |
                 (source->surface_contact_active << VEHICLE_SNAPSHOT_SURFACE_CONTACT_FLAG_SHIFT) |
                 (source->warning_display_state << VEHICLE_SNAPSHOT_WARNING_STATE_FLAG_SHIFT) |
                 (source->warning_display_index << VEHICLE_SNAPSHOT_WARNING_INDEX_FLAG_SHIFT) |
                 source->warning_display_pending);
      if((source->high_detail_shape_flags[1] & VEHICLE_SNAPSHOT_HIGH_DETAIL_SHAPE_FLAG) != 0)
        player_snapshot->state_flags |= VEHICLE_SNAPSHOT_HIGH_DETAIL_FLAG_MASK;
      player_snapshot->encoded_surface_index =
        (uint8) (source->surface_index + VEHICLE_SNAPSHOT_SURFACE_INDEX_BIAS);
      player_snapshot->reserved_167 =
        (uint8) (source->reserved_3_f8 + VEHICLE_SNAPSHOT_RESERVED_3F8_BIAS);
      return;
    }

  snapshot_index = (g_GAME_RESOURCE_STATE.sound_bucket_frame / VEHICLE_SNAPSHOT_FRAME_INTERVAL);
  traffic_snapshot =
    &((TrafficVehicleSimulationSnapshot *) g_GAME_RESOURCE_STATE
        .allocation_fourth)[snapshot_index * VEHICLE_SNAPSHOT_TRAFFIC_COUNT + traffic_index_];
  capture_vehicle_simulation_snapshot(state_, &traffic_snapshot->simulation);
  traffic_snapshot->target_track_speed    = source->target_track_speed;
  traffic_snapshot->track_position_offset = source->track_position_offset;
  traffic_snapshot->art_variant           = source->art_variant;
}

void
restore_replay_simulation_snapshot(int32 frame_, int32 snapshot_slot_)
{
  ReplaySimulationSnapshot *snapshot;
  SimulationSource         *source;
  int32                     snapshot_index;
  int32                     traffic_vehicle_count;
  int32                     traffic_index;
  int32                     requested_mode;

  if(snapshot_slot_ < 0)
    snapshot_index = frame_ / VEHICLE_SNAPSHOT_FRAME_INTERVAL;
  else
    {
      snapshot_index       = snapshot_slot_ + REPLAY_SIMULATION_AUTOMATIC_SNAPSHOT_COUNT;
      G_GAME_FRAME_COUNTER = (int32) g_TRACK_CONFIGURATION_SLOTS[snapshot_slot_];
      update_driving_warning_timer_flags();
    }

  restore_road_object_orientation_states();
  clear_road_object_orientation_states_at_limit(G_GAME_FRAME_COUNTER /
                                                VEHICLE_SNAPSHOT_FRAME_INTERVAL);
  restore_vehicle_drive_statistics();

  snapshot                                         = &g_REPLAY_SIMULATION_SNAPSHOTS[snapshot_index];
  *g_VEHICLE_RUNTIME_POOL_STATE.active_record_list = snapshot->active_vehicle_records;
  traffic_vehicle_count                            = snapshot->vehicle_count - 1;
  g_RUNTIME_STATE.traffic_coordination_countdown   = snapshot->traffic_coordination_countdown;
  g_RUNTIME_STATE.collision_simulation_enabled     = snapshot->collision_simulation_enabled;
  g_VEHICLE_COLLISION_UPDATE_PENDING               = snapshot->collision_update_pending;
  g_VEHICLE_SURFACE_INDEX                          = snapshot->vehicle_surface_index;
  g_VEHICLE_TRAFFIC_COORDINATION_COUNTER           = snapshot->traffic_state_secondary;
  g_VEHICLE_SNAPSHOT_EVENT_STATE                   = (int32) snapshot->vehicle_event_state;
  g_SECONDARY_VEHICLE_SOUND_SUPPRESSED             = snapshot->secondary_vehicle_sound_suppressed;
  g_VEHICLE_SPAWN_RANDOM_STATE.low_word            = snapshot->vehicle_spawn_random_low_word;
  g_VEHICLE_LAST_TRACK_FEATURE_SEGMENT              = snapshot->traffic_state_primary;
  g_VEHICLE_TRAFFIC_DIRECTION_COUNTS.reverse_count = snapshot->reverse_traffic_count;
  g_VEHICLE_TRAFFIC_DIRECTION_COUNTS.forward_count = snapshot->forward_traffic_count;
  g_VEHICLE_SPAWN_POSITION_STATE.active_vehicle_count = snapshot->active_vehicle_count;
  g_VEHICLE_RUNTIME_POOL_STATE.snapshot_preserved_state = snapshot->runtime_pool_preserved_state;
  g_VEHICLE_COLLISION_SCORE                             = snapshot->vehicle_collision_score;
  g_VEHICLE_RESTORE_SURFACE_STATE_PENDING =
    snapshot->vehicle_state_flags & VEHICLE_SNAPSHOT_RESTORE_SURFACE_FLAG;
  g_VEHICLE_RESISTANCE_REDUCTION_ENABLED =
    (snapshot->vehicle_state_flags & VEHICLE_SNAPSHOT_RESISTANCE_REDUCTION_FLAG) != 0;
  g_RUNTIME_STATE.road_object_seek_record_index       = 0;
  g_RUNTIME_STATE.road_object_seek_record_id          = 0;
  g_RUNTIME_STATE.road_object_activation_record_index = 0;
  g_RUNTIME_STATE.road_object_activation_record_id    = 0;

  if(snapshot_slot_ < 0)
    restore_vehicle_simulation_snapshot_set(1, snapshot_index, 0);
  else
    {
      g_GAME_RESOURCE_STATE.player_snapshot_restore_active = 1;
      restore_vehicle_simulation_snapshot_set(1, snapshot_index, 0);
    }

  for(traffic_index = 0; traffic_index < traffic_vehicle_count; ++traffic_index)
    {
      restore_vehicle_simulation_snapshot_set(0, snapshot_index, traffic_index);
    }

  if(g_GAME_RESOURCE_STATE.player_snapshot_restore_active != 0)
    g_GAME_RESOURCE_STATE.player_snapshot_restore_active = 0;

  if(snapshot_slot_ < 0)
    {
      if(g_GAME_RESOURCE_STATE.sound_requests_enabled == VEHICLE_SNAPSHOT_SOUND_REQUESTS_ENABLED)
        return;
    }
  else
    {
      if(g_GAME_RESOURCE_STATE.sound_requests_enabled == VEHICLE_SNAPSHOT_SOUND_REQUESTS_ENABLED)
        {
          save_vehicle_drive_statistics();
          save_road_object_orientation_states();
        }
      g_GAME_RESOURCE_STATE.sound_requests_enabled =
        VEHICLE_SNAPSHOT_SOUND_REQUESTS_REPLAY_DISABLED;
      g_GAME_RESOURCE_STATE.replay_restore_pending = 1;
    }

  requested_mode = (int32) g_SIMULATION_RENDER_STATE.requested_mode;
  source         = &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_PLAYER]->simulation_source;
  if(requested_mode == VEHICLE_SNAPSHOT_MODE_SIX_TARGET)
    {
      if(is_mode_six_vehicle_track_gap_in_range(0, VEHICLE_SNAPSHOT_TARGET_TRACK_GAP_MAXIMUM) == 0)
        {
          requested_mode                           = VEHICLE_SNAPSHOT_RESTORE_FALLBACK_MODE;
          g_SIMULATION_RENDER_STATE.requested_mode = (uint32) requested_mode;
        }
      else
        source =
          &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_SIX_TARGET]->simulation_source;
    }
  else if(requested_mode == VEHICLE_SNAPSHOT_MODE_FIVE_TARGET)
    {
      if(is_mode_five_vehicle_track_gap_in_range(0, VEHICLE_SNAPSHOT_TARGET_TRACK_GAP_MAXIMUM) == 0)
        {
          requested_mode                           = VEHICLE_SNAPSHOT_RESTORE_FALLBACK_MODE;
          g_SIMULATION_RENDER_STATE.requested_mode = (uint32) requested_mode;
        }
      else
        source = &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_FIVE_TARGET]
                    ->simulation_source;
    }

  initialize_simulation_state_from_source(&g_SIMULATION_RENDER_STATE, source, requested_mode);
}

void
capture_replay_simulation_snapshot(void)
{
  ReplaySimulationSnapshot *snapshot;
  DoublyLinkedNode         *node;
  int32                     snapshot_index;
  int32                     traffic_index;

  traffic_index  = 0;
  snapshot_index = g_GAME_RESOURCE_STATE.sound_bucket_frame / VEHICLE_SNAPSHOT_FRAME_INTERVAL;

  if(g_SIMULATION_PENDING_VALUE == 0)
    {
      save_vehicle_drive_statistics();
      save_road_object_orientation_states();
      g_SIMULATION_PENDING_VALUE = VEHICLE_SNAPSHOT_CAPTURE_COUNTDOWN;

      snapshot                                 = &g_REPLAY_SIMULATION_SNAPSHOTS[snapshot_index];
      snapshot->active_vehicle_records         = *g_VEHICLE_RUNTIME_POOL_STATE.active_record_list;
      snapshot->traffic_coordination_countdown = g_RUNTIME_STATE.traffic_coordination_countdown;
      snapshot->collision_simulation_enabled   = g_RUNTIME_STATE.collision_simulation_enabled;
      snapshot->vehicle_surface_index          = g_VEHICLE_SURFACE_INDEX;
      snapshot->vehicle_spawn_random_low_word  = g_VEHICLE_SPAWN_RANDOM_STATE.low_word;
      snapshot->traffic_state_primary          = g_VEHICLE_LAST_TRACK_FEATURE_SEGMENT;
      snapshot->reverse_traffic_count          = g_VEHICLE_TRAFFIC_DIRECTION_COUNTS.reverse_count;
      snapshot->forward_traffic_count          = g_VEHICLE_TRAFFIC_DIRECTION_COUNTS.forward_count;
      snapshot->active_vehicle_count =
        g_VEHICLE_SPAWN_POSITION_STATE.active_vehicle_count;
      snapshot->runtime_pool_preserved_state =
        g_VEHICLE_RUNTIME_POOL_STATE.snapshot_preserved_state;
      snapshot->vehicle_collision_score  = g_VEHICLE_COLLISION_SCORE;
      snapshot->vehicle_state_flags      = (uint8) (g_VEHICLE_RESTORE_SURFACE_STATE_PENDING |
                                                    (g_VEHICLE_RESISTANCE_REDUCTION_ENABLED << 1));
      snapshot->collision_update_pending = g_VEHICLE_COLLISION_UPDATE_PENDING;
      snapshot->traffic_state_secondary  = g_VEHICLE_TRAFFIC_COORDINATION_COUNTER;
      snapshot->vehicle_event_state      = (uint8) g_VEHICLE_SNAPSHOT_EVENT_STATE;
      snapshot->secondary_vehicle_sound_suppressed = g_SECONDARY_VEHICLE_SOUND_SUPPRESSED;

      record_control_history_frame();
      node = g_VEHICLE_RUNTIME_POOL_STATE.active_record_list->list.head_sentinel->next;
      while(node->next != NULL)
        {
          capture_vehicle_simulation_snapshot_set((const VehicleSimulationState *) node,
                                                  traffic_index);
          if((const void *) node != (const void *) G_ACTIVE_SIMULATION_SOURCE)
            ++traffic_index;
          node = node->next;
        }
      snapshot->vehicle_count = traffic_index + 1;
      restore_replay_simulation_snapshot(g_GAME_RESOURCE_STATE.sound_bucket_frame, -1);
    }
  else
    record_control_history_frame();

  --g_SIMULATION_PENDING_VALUE;
}

int32
prepare_replay_simulation_frame(int32 replay_frame_, int32 frame_limit_)
{
  int32 frame_remainder;

  g_ANTI_LOCK_BRAKES_ENABLED =
    ((g_STARTUP_CONFIGURATION.anti_lock_brakes_enabled != 0) &&
     (G_ACTIVE_SIMULATION_SOURCE->simulation_source.vehicle_parameters->anti_lock_brakes_enabled !=
      0));
  g_TRACTION_CONTROL_ENABLED =
    ((g_STARTUP_CONFIGURATION.traction_control_enabled != 0) &&
     (G_ACTIVE_SIMULATION_SOURCE->simulation_source.vehicle_parameters->traction_control_enabled !=
      0));

  if(frame_limit_ > g_DRIVING_WARNING_METER_STATE.input_value)
    frame_limit_ = g_DRIVING_WARNING_METER_STATE.input_value;

  G_GAME_FRAME_COUNTER                     = replay_frame_;
  g_GAME_RESOURCE_STATE.sound_bucket_frame = replay_frame_ % REPLAY_FRAME_WRAP_INTERVAL;
  g_GAME_RESOURCE_STATE.replay_frame_limit = frame_limit_;

  frame_remainder = replay_frame_ % VEHICLE_SNAPSHOT_FRAME_INTERVAL;
  if(frame_remainder == 0)
    {
      if(g_GAME_RESOURCE_STATE.sound_requests_enabled == VEHICLE_SNAPSHOT_SOUND_REQUESTS_ENABLED)
        {
          save_vehicle_drive_statistics();
          save_road_object_orientation_states();
        }
      g_GAME_RESOURCE_STATE.sound_requests_enabled =
        VEHICLE_SNAPSHOT_SOUND_REQUESTS_REPLAY_DISABLED;
      g_GAME_RESOURCE_STATE.replay_restore_pending = 1;
    }
  else
    {
      g_GAME_RESOURCE_STATE.replay_requested_frame = replay_frame_;
      G_GAME_FRAME_COUNTER =
        (replay_frame_ / VEHICLE_SNAPSHOT_FRAME_INTERVAL) * VEHICLE_SNAPSHOT_FRAME_INTERVAL;
      if(g_GAME_RESOURCE_STATE.sound_requests_enabled == VEHICLE_SNAPSHOT_SOUND_REQUESTS_ENABLED)
        {
          save_vehicle_drive_statistics();
          save_road_object_orientation_states();
        }
      g_GAME_RESOURCE_STATE.sound_requests_enabled = VEHICLE_SNAPSHOT_SOUND_REQUESTS_REPLAY_OFFSET;
      g_GAME_RESOURCE_STATE.replay_restore_pending = 0;
    }

  g_GAME_RESOURCE_STATE.sound_bucket_frame = G_GAME_FRAME_COUNTER % REPLAY_FRAME_WRAP_INTERVAL;
  update_driving_warning_timer_flags();
  g_GAME_RESOURCE_STATE.player_snapshot_restore_active = 1;
  clear_vehicle_index_slots((VehicleIndexSlots *) &g_TIRE_SMOKE_PARTICLES);
  g_TRACTION_CONTROL_ENABLED = g_GAME_RESOURCE_STATE.traction_control_enabled;
  clear_driving_warning_display();
  g_DRIVING_WARNING_DISPLAY_STATE.control_input_display_timer           = 0;
  g_VEHICLE_STATISTICS_NOTIFICATION_TIMER                               = 0;
  g_DRIVING_WARNING_DISPLAY_STATE.warning_timer         = REPLAY_WARNING_TIMER_INITIAL_VALUE;
  g_GAME_RESOURCE_STATE.driving_warning_status_interval = replay_frame_;
  g_GAME_RESOURCE_STATE.replay_render_sync_pending      = 1;

  while(g_GAME_RESOURCE_STATE.replay_render_sync_pending == 1)
    {
      if(g_RENDER_THREAD_STATE.frame_complete != 0)
        ++g_RENDER_THREAD_STATE.frame_complete;
      signal_render_thread();
      (void) delay_game_vbls(1);
    }

  return delay_game_vbls(1);
}

void
apply_replay_simulation_frame(void)
{
  int32 request_code;

  if((g_GAME_RESOURCE_STATE.sound_bucket_frame % VEHICLE_SNAPSHOT_FRAME_INTERVAL) == 0)
    {
      restore_replay_simulation_snapshot(g_GAME_RESOURCE_STATE.sound_bucket_frame, -1);
    }
  replay_control_history_frame();

  if(g_GAME_RESOURCE_STATE.replay_restore_pending != 0)
    {
      g_GAME_RESOURCE_STATE.sound_request_restore_pending = 0;
      initialize_simulation_state_from_source(
        &g_SIMULATION_RENDER_STATE,
        &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_PLAYER]->simulation_source,
        (int32) g_SIMULATION_RENDER_STATE.requested_mode);

      if(g_GAME_RESOURCE_STATE.replay_sequence_index >= -1)
        {
          if(g_GAME_RESOURCE_STATE.replay_sequence_index == -1)
            {
              request_code = g_SOUND_EFFECT_SELECTED_BUCKETS[SOUND_EFFECT_REQUEST_RANGE_COUNT - 1];
            }
          else
            {
              request_code =
                g_SOUND_EFFECT_REQUEST_RANGES[g_GAME_RESOURCE_STATE.replay_sequence_index]
                  .request_code;
            }
          initialize_replay_simulation_view(g_GAME_RESOURCE_STATE.replay_view_context ==
                                              REPLAY_VIEW_CONTEXT_ALTERNATE,
                                            request_code);
        }

      g_SECONDARY_ROAD_RENDER_CCB                      = REPLAY_SECONDARY_ROAD_RENDER_PENDING;
      g_GAME_RESOURCE_STATE.replay_render_sync_pending = 0;
      g_GAME_RESOURCE_STATE.replay_restore_pending     = 0;
      if(g_GAME_RESOURCE_STATE.replay_track_slot_pending != 0)
        {
          g_GAME_RESOURCE_STATE.replay_track_slot_pending = 0;
          set_track_configuration_slot(0, (void *) G_GAME_FRAME_COUNTER);
        }
    }

  if((g_GAME_RESOURCE_STATE.driving_warning_mode != 0) &&
     (((uint32) G_GAME_FRAME_COUNTER & REPLAY_MODE_SELECTION_FRAME_MASK) ==
      REPLAY_MODE_SELECTION_FRAME_VALUE))
    {
      update_simulation_mode_selection(&g_SIMULATION_RENDER_STATE);
    }
}

void
begin_recent_replay(void)
{
  int32 replay_frame;

  replay_frame = g_DRIVING_WARNING_METER_STATE.input_value - REPLAY_RECENT_FRAME_OFFSET;
  if(replay_frame < 0)
    replay_frame = 0;
  replay_frame -= replay_frame % REPLAY_FRAME_ALIGNMENT;

  (void) prepare_replay_simulation_frame(replay_frame, g_DRIVING_WARNING_METER_STATE.input_value);
  g_GAME_RESOURCE_STATE.replay_input_mode              = 1;
  g_GAME_RESOURCE_STATE.replay_sequence_index          = REPLAY_SEQUENCE_INDEX_DISABLED;
  g_GAME_RESOURCE_STATE.replay_view_context            = REPLAY_DEFAULT_VIEW_CONTEXT;
  g_GAME_RESOURCE_STATE.replay_vehicle_sprite_rotation = 0;
  g_GAME_RESOURCE_STATE.replay_control_mode            = REPLAY_DEFAULT_CONTROL_MODE;
  g_GAME_RESOURCE_STATE.replay_view_variant            = 0;

  g_SIMULATION_RENDER_STATE.requested_mode = (uint32) g_GAME_RESOURCE_STATE.replay_saved_view_mode;
  if((g_GAME_RESOURCE_STATE.replay_saved_view_mode == 0) &&
     (G_ACTIVE_SIMULATION_SOURCE->simulation_source.track_transform.m00 >
      REPLAY_TRACK_BASIS_MINIMUM_FIXED) &&
     (G_ACTIVE_SIMULATION_SOURCE->simulation_source.track_transform.m00 <
      REPLAY_TRACK_BASIS_MAXIMUM_FIXED))
    {
      g_SIMULATION_RENDER_STATE.mode = REPLAY_TRACK_BASIS_RENDER_MODE;
    }

  configure_frontend_display(0, 0);
  g_GAME_RESOURCE_STATE.runtime_initialized = 0;
}

void
seek_replay_from_input(uint32 buttons_, int32 repeat_count_)
{
  int32 replay_frame;
  int32 seek_step;
  int32 extended_start_frame;
  int32 recent_start_frame;

  seek_step = repeat_count_;
  if(seek_step < 0)
    seek_step += (1 << REPLAY_SEEK_STEP_SHIFT) - 1;
  seek_step = (seek_step >> REPLAY_SEEK_STEP_SHIFT) * REPLAY_SEEK_STEP_SCALE;
  if(repeat_count_ == 0)
    seek_step = REPLAY_SEEK_STEP_SCALE;

  recent_start_frame   = g_DRIVING_WARNING_METER_STATE.input_value - REPLAY_SEEK_FRAME_MARGIN;
  extended_start_frame = g_DRIVING_WARNING_METER_STATE.input_value - REPLAY_EXTENDED_HISTORY_FRAMES;
  if(recent_start_frame < G_GAME_FRAME_COUNTER)
    G_GAME_FRAME_COUNTER = recent_start_frame;

  if((buttons_ & REPLAY_SEEK_EXTENDED_START_BUTTONS) != 0)
    {
      replay_frame = extended_start_frame;
      if(replay_frame < 0)
        replay_frame = 0;
      if(replay_frame < g_GAME_RUNTIME_STATE.race_start_frame)
        replay_frame = g_GAME_RUNTIME_STATE.race_start_frame;
      g_GAME_RESOURCE_STATE.driving_warning_mode = 0;
      g_GAME_RESOURCE_STATE.replay_control_mode  = REPLAY_SEEK_CONTROL_MODE_ABSOLUTE;
    }
  else if((buttons_ & REPLAY_SEEK_RECENT_START_BUTTONS) != 0)
    {
      replay_frame = recent_start_frame;
      if(extended_start_frame > replay_frame)
        replay_frame = extended_start_frame;
      if(replay_frame < 0)
        replay_frame = 0;
      g_GAME_RESOURCE_STATE.driving_warning_mode = 0;
      g_GAME_RESOURCE_STATE.replay_control_mode  = REPLAY_SEEK_CONTROL_MODE_FORWARD;
    }
  else if((buttons_ & REPLAY_SEEK_BACKWARD_BUTTONS) != 0)
    {
      replay_frame = G_GAME_FRAME_COUNTER;
      if(replay_frame > seek_step)
        replay_frame -= seek_step;
      if(extended_start_frame > replay_frame)
        replay_frame = extended_start_frame;
      g_GAME_RESOURCE_STATE.driving_warning_mode = 0;
      g_GAME_RESOURCE_STATE.replay_control_mode  = REPLAY_SEEK_CONTROL_MODE_ABSOLUTE;
    }
  else if((buttons_ & REPLAY_SEEK_FORWARD_BUTTONS) != 0)
    {
      replay_frame = G_GAME_FRAME_COUNTER;
      if((recent_start_frame - seek_step) > replay_frame)
        replay_frame += seek_step;
      if(replay_frame < 0)
        replay_frame = 0;
      g_GAME_RESOURCE_STATE.driving_warning_mode = 0;
      g_GAME_RESOURCE_STATE.replay_control_mode  = REPLAY_SEEK_CONTROL_MODE_FORWARD;
    }
  else
    replay_frame = G_GAME_FRAME_COUNTER;

  replay_frame -= replay_frame % REPLAY_FRAME_ALIGNMENT;
  G_GAME_FRAME_COUNTER = replay_frame;
  if(g_GAME_RESOURCE_STATE.sound_requests_enabled == REPLAY_HISTORY_RECORDING_MODE)
    {
      save_vehicle_drive_statistics();
      save_road_object_orientation_states();
    }

  g_GAME_RESOURCE_STATE.sound_requests_enabled         = REPLAY_HISTORY_SEEK_MODE;
  g_VEHICLE_SURFACE_SOUND_MODE                         = 0;
  g_GAME_RESOURCE_STATE.replay_sequence_index          = REPLAY_SEQUENCE_INDEX_DISABLED;
  g_GAME_RESOURCE_STATE.replay_vehicle_sprite_rotation = 0;
  g_GAME_RESOURCE_STATE.replay_view_context            = REPLAY_DEFAULT_VIEW_CONTEXT;
  g_GAME_RESOURCE_STATE.replay_view_variant            = 0;
}

void
handle_replay_sequence_request(uint32 buttons_)
{
  int32 replay_frame;

  if(buttons_ == REPLAY_SEQUENCE_FIRST_BUTTONS)
    {
      if(g_GAME_RESOURCE_STATE.sound_requests_enabled == 1)
        {
          g_GAME_RESOURCE_STATE.replay_saved_view_mode =
            (int32) g_SIMULATION_RENDER_STATE.requested_mode;
        }

      prepare_sound_effect_request_ranges();
      if(g_GAME_RESOURCE_STATE.sound_effect_request_range_count <= 0)
        return;

      g_GAME_RESOURCE_STATE.replay_sequence_index = 0;
      (void) prepare_replay_simulation_frame(g_GAME_RUNTIME_STATE.track_segment_count,
                                             g_GAME_RUNTIME_STATE.result);
      g_GAME_RESOURCE_STATE.replay_view_context = REPLAY_DEFAULT_VIEW_CONTEXT;
      g_GAME_RESOURCE_STATE.replay_view_variant = 0;
      g_GAME_RESOURCE_STATE.replay_control_mode = REPLAY_DEFAULT_CONTROL_MODE;
      return;
    }

  g_GAME_RESOURCE_STATE.replay_sequence_index          = REPLAY_SEQUENCE_INDEX_DISABLED;
  g_GAME_RESOURCE_STATE.replay_vehicle_sprite_rotation = 0;
  g_GAME_RESOURCE_STATE.replay_view_context            = REPLAY_DEFAULT_VIEW_CONTEXT;
  g_GAME_RESOURCE_STATE.replay_view_variant            = 0;
  g_GAME_RESOURCE_STATE.replay_control_mode            = REPLAY_DEFAULT_CONTROL_MODE;

  if(buttons_ == REPLAY_SEQUENCE_FOURTH_BUTTONS)
    {
      G_GAME_FRAME_COUNTER =
        g_DRIVING_WARNING_METER_STATE.input_value - REPLAY_EXTENDED_HISTORY_FRAMES;
      if(G_GAME_FRAME_COUNTER < 0)
        G_GAME_FRAME_COUNTER = 0;
      if(G_GAME_FRAME_COUNTER < g_GAME_RUNTIME_STATE.race_start_frame)
        G_GAME_FRAME_COUNTER = g_GAME_RUNTIME_STATE.race_start_frame;
    }
  else
    {
      G_GAME_FRAME_COUNTER =
        g_DRIVING_WARNING_METER_STATE.input_value - REPLAY_SHORT_HISTORY_FRAMES;
      if(G_GAME_FRAME_COUNTER < 0)
        G_GAME_FRAME_COUNTER = 0;
    }

  g_GAME_RESOURCE_STATE.replay_input_mode         = REPLAY_SEQUENCE_INPUT_MODE;
  g_GAME_RESOURCE_STATE.replay_track_slot_pending = 1;
  replay_frame = G_GAME_FRAME_COUNTER;
  replay_frame -= replay_frame % REPLAY_FRAME_ALIGNMENT;
  (void) prepare_replay_simulation_frame(replay_frame, g_DRIVING_WARNING_METER_STATE.input_value);
  if(buttons_ == REPLAY_SEQUENCE_FOURTH_BUTTONS)
    g_GAME_RESOURCE_STATE.driving_warning_mode = 1;
  g_GAME_RESOURCE_STATE.replay_sequence_index = REPLAY_SEQUENCE_INDEX_PENDING;
}

void
select_replay_view_from_input(uint32 buttons_, int32 repeat_count_)
{
  int32 replay_frame;
  int32 history_mode;

  history_mode = g_GAME_RESOURCE_STATE.sound_requests_enabled;
  if(history_mode == REPLAY_HISTORY_RECORDING_MODE)
    return;

  if(buttons_ == REPLAY_VIEW_CONTEXT_FOUR_BUTTONS)
    {
      g_GAME_RESOURCE_STATE.replay_control_mode = REPLAY_DEFAULT_CONTROL_MODE;
      g_GAME_RESOURCE_STATE.replay_view_variant = 0;
      g_GAME_RESOURCE_STATE.replay_view_context = REPLAY_DEFAULT_VIEW_CONTEXT;
    }
  else if(buttons_ == REPLAY_VIEW_CONTEXT_THREE_BUTTONS)
    {
      g_GAME_RESOURCE_STATE.replay_view_context = REPLAY_VIEW_CONTEXT_THREE;
      g_GAME_RESOURCE_STATE.replay_control_mode = REPLAY_VIEW_CONTEXT_THREE_CONTROL_MODE;
      g_GAME_RESOURCE_STATE.replay_view_variant = REPLAY_VIEW_CONTEXT_THREE_VARIANT;
    }
  else if(buttons_ == REPLAY_VIEW_CONTEXT_ONE_BUTTONS)
    {
      g_GAME_RESOURCE_STATE.replay_control_mode = REPLAY_VIEW_CONTEXT_ONE_CONTROL_MODE;
      g_GAME_RESOURCE_STATE.replay_view_context = REPLAY_VIEW_CONTEXT_ONE;
      g_GAME_RESOURCE_STATE.replay_view_variant = REPLAY_VIEW_CONTEXT_ONE_VARIANT;
    }
  else if(buttons_ == REPLAY_VIEW_CONTEXT_TWO_BUTTONS)
    {
      g_GAME_RESOURCE_STATE.replay_control_mode = REPLAY_VIEW_CONTEXT_TWO_CONTROL_MODE;
      g_GAME_RESOURCE_STATE.replay_view_variant = REPLAY_VIEW_CONTEXT_TWO_VARIANT;
      g_GAME_RESOURCE_STATE.replay_view_context = REPLAY_VIEW_CONTEXT_TWO;
    }
  else if((buttons_ == REPLAY_VIEW_CONTEXT_ZERO_BUTTONS) && (repeat_count_ == 0))
    {
      if(history_mode == REPLAY_HISTORY_OFFSET_MODE)
        return;
      if(g_GAME_RESOURCE_STATE.replay_pause_mode == 0)
        {
          g_GAME_RESOURCE_STATE.replay_view_context = REPLAY_VIEW_CONTEXT_ZERO;
          g_GAME_RESOURCE_STATE.replay_pause_mode   = REPLAY_VIEW_CONTEXT_ZERO_PAUSE_MODE;
          g_GAME_RESOURCE_STATE.replay_control_mode = REPLAY_VIEW_CONTEXT_ZERO_CONTROL_MODE;
        }
    }
  else
    return;

  if(history_mode != REPLAY_SEQUENCE_INPUT_MODE)
    return;

  g_GAME_RESOURCE_STATE.replay_track_slot_pending = 1;
  g_GAME_RESOURCE_STATE.replay_input_mode         = REPLAY_SEQUENCE_INPUT_MODE;
  replay_frame                                    = G_GAME_FRAME_COUNTER;
  replay_frame -= replay_frame % REPLAY_FRAME_ALIGNMENT;
  (void) prepare_replay_simulation_frame(replay_frame, g_DRIVING_WARNING_METER_STATE.input_value);
  g_GAME_RESOURCE_STATE.driving_warning_mode = 0;
}

void
process_replay_control_input(void)
{
  uint32 buttons;
  uint32 raw_buttons;
  int32  rotation;

  raw_buttons = (uint32) g_CONTROL_INPUT_POLL_STATE.frontend_display_enabled;
  if((g_GAME_RESOURCE_STATE.replay_raw_buttons == 0) && ((raw_buttons & 1) != 0))
    {
      g_GAME_RESOURCE_STATE.replay_input_buttons |= REPLAY_SEEK_EXTENDED_START_BUTTONS;
    }
  g_GAME_RESOURCE_STATE.replay_raw_buttons = raw_buttons;

  if(g_GAME_RESOURCE_STATE.replay_previous_buttons == g_GAME_RESOURCE_STATE.replay_input_buttons)
    {
      if(g_GAME_RESOURCE_STATE.replay_input_repeat_count < REPLAY_INPUT_REPEAT_MAXIMUM)
        {
          if(g_GAME_RESOURCE_STATE.replay_input_repeat_count >= REPLAY_INPUT_REPEAT_SLOW_LIMIT)
            {
              g_GAME_RESOURCE_STATE.replay_input_repeat_count += REPLAY_INPUT_REPEAT_FAST_STEP;
            }
          else
            {
              g_GAME_RESOURCE_STATE.replay_input_repeat_count += REPLAY_INPUT_REPEAT_SLOW_STEP;
            }
        }
    }
  else
    g_GAME_RESOURCE_STATE.replay_input_repeat_count = 0;

  if(g_GAME_RESOURCE_STATE.replay_sequence_first_pending != 0)
    {
      g_GAME_RESOURCE_STATE.replay_sequence_first_pending  = 0;
      g_GAME_RESOURCE_STATE.replay_sequence_second_pending = 0;
      g_GAME_RESOURCE_STATE.replay_update_count            = REPLAY_SEQUENCE_UPDATE_COUNT;
      g_GAME_RESOURCE_STATE.replay_sequence_fourth_pending = 0;
      g_GAME_RESOURCE_STATE.replay_input_buttons           = REPLAY_SEQUENCE_FIRST_BUTTONS;
      (void) ScavengeMem();
    }
  else if(g_GAME_RESOURCE_STATE.replay_sequence_second_pending != 0)
    {
      g_GAME_RESOURCE_STATE.replay_sequence_second_pending = 0;
      g_GAME_RESOURCE_STATE.replay_sequence_fourth_pending = 0;
      g_GAME_RESOURCE_STATE.replay_input_buttons           = REPLAY_SEQUENCE_SECOND_BUTTONS;
      (void) ScavengeMem();
    }
  else if(g_GAME_RESOURCE_STATE.replay_sequence_fourth_pending != 0)
    {
      g_GAME_RESOURCE_STATE.replay_update_count            = REPLAY_SEQUENCE_UPDATE_COUNT;
      g_GAME_RESOURCE_STATE.replay_sequence_fourth_pending = 0;
      g_GAME_RESOURCE_STATE.replay_input_buttons           = REPLAY_SEQUENCE_FOURTH_BUTTONS;
      (void) ScavengeMem();
    }

  buttons = g_GAME_RESOURCE_STATE.replay_input_buttons;
  if(buttons == 0)
    goto update_previous_buttons;

  if((buttons == REPLAY_SEQUENCE_FIRST_BUTTONS) || (buttons == REPLAY_SEQUENCE_SECOND_BUTTONS) ||
     (buttons == REPLAY_SEQUENCE_FOURTH_BUTTONS))
    {
      g_GAME_RESOURCE_STATE.driving_warning_mode = 0;
      handle_replay_sequence_request(buttons);
      g_GAME_RESOURCE_STATE.runtime_initialized = 0;
    }
  else if(((buttons & REPLAY_SEEK_INPUT_MASK) != 0) &&
          (g_GAME_RESOURCE_STATE.replay_update_count > 0))
    {
      seek_replay_from_input(buttons, g_GAME_RESOURCE_STATE.replay_input_repeat_count);
      g_GAME_RESOURCE_STATE.runtime_initialized  = 1;
      g_GAME_RESOURCE_STATE.driving_warning_mode = 0;
    }
  else if(buttons == REPLAY_RECENT_REQUEST_BUTTONS)
    {
      if(g_GAME_RUNTIME_STATE.flags_14 == 0)
        {
          begin_recent_replay();
          g_GAME_RESOURCE_STATE.driving_warning_mode = 0;
        }
    }
  else if(buttons == REPLAY_VIEW_TOGGLE_BUTTONS)
    {
      if((g_GAME_RESOURCE_STATE.replay_previous_buttons != REPLAY_VIEW_TOGGLE_BUTTONS) &&
         (g_GAME_RESOURCE_STATE.sound_requests_enabled != REPLAY_HISTORY_SEEK_MODE))
        {
          update_simulation_mode_selection(&g_SIMULATION_RENDER_STATE);
          g_GAME_RESOURCE_STATE.replay_vehicle_sprite_rotation = 0;
          if((g_GAME_RESOURCE_STATE.sound_requests_enabled == REPLAY_HISTORY_PLAYBACK_MODE) &&
             (g_GAME_RESOURCE_STATE.replay_view_context == 0) &&
             (g_GAME_RESOURCE_STATE.replay_pause_mode <= 0))
            {
              g_GAME_RESOURCE_STATE.replay_pause_mode   = REPLAY_VIEW_CONTEXT_ZERO_PAUSE_MODE;
              g_GAME_RESOURCE_STATE.replay_control_mode = REPLAY_VIEW_CONTEXT_ZERO_CONTROL_MODE;
            }
          g_GAME_RESOURCE_STATE.runtime_initialized  = 0;
          g_GAME_RESOURCE_STATE.driving_warning_mode = 0;
        }
    }
  else if((buttons & REPLAY_VIEW_SELECTION_MASK) != 0)
    {
      select_replay_view_from_input(buttons, g_GAME_RESOURCE_STATE.replay_input_repeat_count);
      g_GAME_RESOURCE_STATE.runtime_initialized = 0;
    }

  if((g_GAME_RESOURCE_STATE.sound_requests_enabled == REPLAY_HISTORY_PLAYBACK_MODE) &&
     (g_GAME_RESOURCE_STATE.replay_view_context > 0))
    {
      if(buttons == REPLAY_ROTATE_POSITIVE_BUTTONS)
        {
          g_GAME_RESOURCE_STATE.replay_vehicle_sprite_rotation += REPLAY_ROTATION_STEP;
        }
      else if(buttons == REPLAY_ROTATE_NEGATIVE_BUTTONS)
        {
          g_GAME_RESOURCE_STATE.replay_vehicle_sprite_rotation -= REPLAY_ROTATION_STEP;
        }

      rotation = g_GAME_RESOURCE_STATE.replay_vehicle_sprite_rotation;
      if(rotation < 0)
        rotation += REPLAY_ROTATION_WRAP;
      else
        rotation &= REPLAY_ROTATION_MASK;
      if(rotation < REPLAY_ROTATION_MIDDLE)
        {
          if(rotation > REPLAY_ROTATION_LOW_LIMIT)
            rotation = REPLAY_ROTATION_LOW_LIMIT;
        }
      else if(rotation < REPLAY_ROTATION_HIGH_LIMIT)
        rotation = REPLAY_ROTATION_HIGH_LIMIT;
      if(g_SIMULATION_RENDER_STATE.mode == 0)
        rotation = 0;
      g_GAME_RESOURCE_STATE.replay_vehicle_sprite_rotation = rotation;
    }

update_previous_buttons:
  g_GAME_RESOURCE_STATE.replay_previous_buttons = g_GAME_RESOURCE_STATE.replay_input_buttons;
}

void
resume_replay_from_track_slot(void)
{
  int32 replay_frame;

  replay_frame         = (int32) g_TRACK_CONFIGURATION_SLOTS[0];
  G_GAME_FRAME_COUNTER = replay_frame;
  update_driving_warning_timer_flags();
  replay_frame = G_GAME_FRAME_COUNTER;
  g_GAME_RESOURCE_STATE.sound_bucket_frame = replay_frame % REPLAY_FRAME_WRAP_INTERVAL;
  (void) prepare_replay_simulation_frame(replay_frame, g_DRIVING_WARNING_METER_STATE.input_value);
  apply_replay_simulation_frame();
}

void
advance_replay_sequence(void)
{
  SoundEffectRequestRange *request_range;
  int32                    random_value;
  int32                    track_gap;
  int32                    view_allowed;

  g_GAME_RESOURCE_STATE.runtime_initialized            = 0;
  g_GAME_RESOURCE_STATE.replay_view_context            = REPLAY_DEFAULT_VIEW_CONTEXT;
  g_GAME_RESOURCE_STATE.replay_vehicle_sprite_rotation = 0;
  g_GAME_RESOURCE_STATE.replay_control_mode            = REPLAY_DEFAULT_CONTROL_MODE;
  g_GAME_RESOURCE_STATE.replay_view_variant            = 0;

  if(g_GAME_RESOURCE_STATE.replay_sequence_advance_pending != 0)
    g_GAME_RESOURCE_STATE.replay_sequence_advance_pending = 0;
  else
    ++g_GAME_RESOURCE_STATE.replay_sequence_index;

  if(g_GAME_RESOURCE_STATE.replay_sequence_index >=
     g_GAME_RESOURCE_STATE.sound_effect_request_range_count)
    {
      g_GAME_RESOURCE_STATE.replay_sequence_index = 0;
      request_range                               = &g_SOUND_EFFECT_REQUEST_RANGES[0];
      (void) prepare_replay_simulation_frame(request_range->first_frame, request_range->last_frame);
      apply_replay_simulation_frame();
      g_GAME_RESOURCE_STATE.replay_sequence_active = 0;
      return;
    }

  random_value  = scale_by_random_fraction(REPLAY_SEQUENCE_RANDOM_RANGE);
  request_range = &g_SOUND_EFFECT_REQUEST_RANGES[g_GAME_RESOURCE_STATE.replay_sequence_index];
  view_allowed  = 1;
  if(request_range->request_code == REPLAY_SEQUENCE_DISTANCE_REQUEST_CODE)
    {
      track_gap = g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_PLAYER]
                    ->simulation_source.track_segment_index -
                  g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_FIVE_TARGET]
                    ->simulation_source.track_segment_index;
      if(track_gap < 0)
        track_gap = -track_gap;
      if(track_gap >= REPLAY_SEQUENCE_NEAR_TRACK_GAP)
        view_allowed = 0;
    }

  if((request_range->request_code > REPLAY_SEQUENCE_VIEW_REQUEST_MINIMUM) &&
     (request_range->request_code < REPLAY_SEQUENCE_VIEW_REQUEST_MAXIMUM) &&
     (random_value > REPLAY_SEQUENCE_RANDOM_THRESHOLD) && (view_allowed != 0))
    {
      g_GAME_RESOURCE_STATE.replay_view_context = REPLAY_VIEW_CONTEXT_THREE;
      g_GAME_RESOURCE_STATE.replay_view_variant = REPLAY_VIEW_CONTEXT_THREE_VARIANT;
      g_GAME_RESOURCE_STATE.replay_control_mode = REPLAY_VIEW_CONTEXT_THREE_CONTROL_MODE;
    }

  (void) prepare_replay_simulation_frame(request_range->first_frame, request_range->last_frame);
  apply_replay_simulation_frame();
}

void
update_replay_playback(void)
{
  if((g_GAME_RESOURCE_STATE.replay_view_context == 0) &&
     (g_GAME_RESOURCE_STATE.replay_pause_mode <= 0))
    {
      g_GAME_RESOURCE_STATE.replay_control_mode = REPLAY_VIEW_CONTEXT_ZERO_CONTROL_MODE;
      update_driving_warning_timer_flags();
      return;
    }

  if((G_GAME_FRAME_COUNTER < g_GAME_RESOURCE_STATE.replay_frame_limit) &&
     ((g_GAME_RESOURCE_STATE.sound_request_restore_pending == 0) ||
      (g_GAME_RESOURCE_STATE.replay_sequence_index < 0)))
    {
      g_GAME_RESOURCE_STATE.runtime_initialized = 0;
      apply_replay_simulation_frame();
      return;
    }

  if((g_GAME_RESOURCE_STATE.replay_frame_limit == g_DRIVING_WARNING_METER_STATE.input_value) &&
     (g_GAME_RESOURCE_STATE.replay_sequence_index < 0))
    {
      g_GAME_RESOURCE_STATE.sound_requests_enabled = g_GAME_RESOURCE_STATE.replay_input_mode;
      if(g_GAME_RESOURCE_STATE.sound_requests_enabled == REPLAY_HISTORY_RECORDING_MODE)
        {
          g_GAME_RESOURCE_STATE.replay_completion_pending = 1;
          return;
        }

      g_GAME_RESOURCE_STATE.runtime_initialized = 0;
      resume_replay_from_track_slot();
      return;
    }

  if(g_GAME_RESOURCE_STATE.replay_sequence_index >= 0)
    advance_replay_sequence();
}

void
update_replay_runtime(void)
{
  uint32 buttons;
  int32  replay_frame;

  buttons = g_CONTROL_INPUT_POLL_STATE.status_00;
  if((buttons == REPLAY_RECENT_REQUEST_BUTTONS) && (g_GAME_RUNTIME_STATE.flags_14 != 0) &&
     (g_GAME_RESOURCE_STATE.sound_requests_enabled != REPLAY_HISTORY_OFFSET_MODE) &&
     (g_GAME_RESOURCE_STATE.replay_update_count > 0))
    {
      g_GAME_RUNTIME_STATE.flags_18                        = g_GAME_RUNTIME_STATE.flags_14;
      g_GAME_RESOURCE_STATE.replay_vehicle_sprite_rotation = 0;
      g_GAME_RESOURCE_STATE.runtime_initialized            = 0;
      return;
    }

  if(g_CONTROL_INPUT_POLL_STATE.frontend_display_enabled != 0)
    {
      g_GAME_RESOURCE_STATE.replay_input_buttons = buttons;
      process_replay_control_input();
    }

  if(((uint32) G_DRIVING_WARNING_TIMER_FLAGS & REPLAY_RUNTIME_UPDATE_ENABLED_FLAG) == 0)
    return;

  replay_frame                             = G_GAME_FRAME_COUNTER;
  g_GAME_RESOURCE_STATE.sound_bucket_frame = replay_frame % REPLAY_FRAME_WRAP_INTERVAL;
  if(g_GAME_RESOURCE_STATE.sound_requests_enabled == REPLAY_HISTORY_RECORDING_MODE)
    {
      capture_replay_simulation_snapshot();
      g_DRIVING_WARNING_METER_STATE.input_value = G_GAME_FRAME_COUNTER;
    }
  else if(g_GAME_RESOURCE_STATE.sound_requests_enabled == REPLAY_HISTORY_PLAYBACK_MODE)
    {
      update_replay_playback();
      ++g_GAME_RESOURCE_STATE.replay_update_count;
    }
  else if(g_GAME_RESOURCE_STATE.sound_requests_enabled == REPLAY_HISTORY_OFFSET_MODE)
    {
      if(replay_frame >= g_GAME_RESOURCE_STATE.replay_requested_frame)
        {
          g_GAME_RESOURCE_STATE.sound_requests_enabled = REPLAY_HISTORY_PLAYBACK_MODE;
          g_GAME_RESOURCE_STATE.replay_restore_pending = 1;
          ++g_GAME_RESOURCE_STATE.replay_update_count;
        }
      if(G_GAME_FRAME_COUNTER < g_GAME_RESOURCE_STATE.replay_frame_limit)
        apply_replay_simulation_frame();
    }
  else if(g_GAME_RESOURCE_STATE.sound_requests_enabled == REPLAY_HISTORY_SEEK_MODE)
    update_driving_warning_timer_flags();
}

COMPILE_TIME_SIZE_ASSERT(VehicleSimulationRefreshStateSize,
                         sizeof(VehicleSimulationRefreshState) ==
                           VEHICLE_SIMULATION_REFRESH_STATE_BYTES);
COMPILE_TIME_SIZE_ASSERT(SoundEffectRequestRangeSize, sizeof(SoundEffectRequestRange) == 0x0C);
COMPILE_TIME_SIZE_ASSERT(SoundEffectRequestRangeCountOffset,
                         offsetof(GameResourceState, sound_effect_request_range_count) == 0x48);
COMPILE_TIME_SIZE_ASSERT(ControlHistoryInitializedOffset,
                         offsetof(GameResourceState, control_history_initialized) == 0x5C);
COMPILE_TIME_SIZE_ASSERT(TractionControlEnabledOffset,
                         offsetof(GameResourceState, traction_control_enabled) == 0x60);
COMPILE_TIME_SIZE_ASSERT(ControlValueHistoryOffset,
                         offsetof(GameResourceState, control_value_history) == 0x64);
COMPILE_TIME_SIZE_ASSERT(ControlNibbleHistoryOffset,
                         offsetof(GameResourceState, control_nibble_history) == 0x68);
COMPILE_TIME_SIZE_ASSERT(SoundRequestRestorePendingOffset,
                         offsetof(GameResourceState, sound_request_restore_pending) == 0x14);
COMPILE_TIME_SIZE_ASSERT(ReplayViewContextOffset,
                         offsetof(GameResourceState, replay_view_context) == 0x20);
COMPILE_TIME_SIZE_ASSERT(ReplayViewVariantOffset,
                         offsetof(GameResourceState, replay_view_variant) == 0x28);
COMPILE_TIME_SIZE_ASSERT(ReplayCompletionPendingOffset,
                         offsetof(GameResourceState, replay_completion_pending) == 0x34);
COMPILE_TIME_SIZE_ASSERT(ReplayUpdateCountOffset,
                         offsetof(GameResourceState, replay_update_count) == 0x0C);
COMPILE_TIME_SIZE_ASSERT(ReplayInputButtonsOffset,
                         offsetof(GameResourceState, replay_input_buttons) == 0xF4);
COMPILE_TIME_SIZE_ASSERT(ReplayPreviousButtonsOffset,
                         offsetof(GameResourceState, replay_previous_buttons) == 0x100);
COMPILE_TIME_SIZE_ASSERT(ReplayInputRepeatCountOffset,
                         offsetof(GameResourceState, replay_input_repeat_count) == 0x104);
COMPILE_TIME_SIZE_ASSERT(ReplayRawButtonsOffset,
                         offsetof(GameResourceState, replay_raw_buttons) == 0x108);
COMPILE_TIME_SIZE_ASSERT(ReplaySequenceAdvancePendingOffset,
                         offsetof(GameResourceState, replay_sequence_advance_pending) == 0x04);
COMPILE_TIME_SIZE_ASSERT(ReplaySequenceIndexOffset,
                         offsetof(GameResourceState, replay_sequence_index) == 0x4C);
COMPILE_TIME_SIZE_ASSERT(ReplayTrackSlotPendingOffset,
                         offsetof(GameResourceState, replay_track_slot_pending) == 0x50);
COMPILE_TIME_SIZE_ASSERT(ReplayFrameLimitOffset,
                         offsetof(GameResourceState, replay_frame_limit) == 0x6C);
COMPILE_TIME_SIZE_ASSERT(ReplayRequestedFrameOffset,
                         offsetof(GameResourceState, replay_requested_frame) == 0x70);
COMPILE_TIME_SIZE_ASSERT(ReplaySequenceActiveOffset,
                         offsetof(GameResourceState, replay_sequence_active) == 0x10C);
COMPILE_TIME_SIZE_ASSERT(GameResourceStateSize, sizeof(GameResourceState) == 0x110);
COMPILE_TIME_SIZE_ASSERT(ReplaySimulationSnapshotSize, sizeof(ReplaySimulationSnapshot) == 0x68);
COMPILE_TIME_SIZE_ASSERT(ReplaySimulationSnapshotVehicleCountOffset,
                         offsetof(ReplaySimulationSnapshot, vehicle_count) == 0x2C);
COMPILE_TIME_SIZE_ASSERT(ReplaySimulationSnapshotEventStateOffset,
                         offsetof(ReplaySimulationSnapshot, vehicle_event_state) == 0x65);
COMPILE_TIME_SIZE_ASSERT(ReplaySimulationSnapshotsSize,
                         sizeof(g_REPLAY_SIMULATION_SNAPSHOTS) == 0xB60);
COMPILE_TIME_SIZE_ASSERT(PlayerSnapshotRestoreActiveOffset,
                         offsetof(GameResourceState, player_snapshot_restore_active) == 0x7C);
COMPILE_TIME_SIZE_ASSERT(ReplayRenderSyncPendingOffset,
                         offsetof(GameResourceState, replay_render_sync_pending) == 0x90);
COMPILE_TIME_SIZE_ASSERT(RuntimePoolSnapshotPreservedStateOffset,
                         offsetof(VehicleRuntimePoolState, snapshot_preserved_state) == 0x20);
COMPILE_TIME_SIZE_ASSERT(SimulationSourceVehicleParametersOffset,
                         offsetof(SimulationSource, vehicle_parameters) == 0x4BC);
COMPILE_TIME_SIZE_ASSERT(VehicleParametersMaximumEngineSpeedOffset,
                         offsetof(VehicleParameters, maximum_engine_speed) == 0xA8);
COMPILE_TIME_SIZE_ASSERT(VehicleParametersAntiLockBrakesOffset,
                         offsetof(VehicleParameters, anti_lock_brakes_enabled) == 0x310);
COMPILE_TIME_SIZE_ASSERT(VehicleParametersTractionControlOffset,
                         offsetof(VehicleParameters, traction_control_enabled) == 0x314);
COMPILE_TIME_SIZE_ASSERT(SimulationStartScaleOffset,
                         offsetof(ControlInputPollState, simulation_start_scale) == 0x14);
COMPILE_TIME_SIZE_ASSERT(SimulationStartDelayOffset,
                         offsetof(ControlInputPollState, simulation_start_delay) == 0x18);
COMPILE_TIME_SIZE_ASSERT(SimulationControlButtonsOffset,
                         offsetof(ControlInputPollState, simulation_control_buttons) == 0x1C);
COMPILE_TIME_SIZE_ASSERT(VehicleSimulationSnapshotSize, sizeof(VehicleSimulationSnapshot) == 0x164);
COMPILE_TIME_SIZE_ASSERT(VehicleSimulationSourceOffset,
                         offsetof(VehicleSimulationState, simulation_source) == 0x0C);
COMPILE_TIME_SIZE_ASSERT(VehicleSimulationSnapshotConfigurationOffset,
                         offsetof(VehicleSimulationSnapshot, runtime_configuration) == 0x14);
COMPILE_TIME_SIZE_ASSERT(VehicleSimulationSnapshotTransformOffset,
                         offsetof(VehicleSimulationSnapshot, track_transform) == 0x3C);
COMPILE_TIME_SIZE_ASSERT(VehicleSimulationSnapshotStatusOffset,
                         offsetof(VehicleSimulationSnapshot, status_flags) == 0xC4);
COMPILE_TIME_SIZE_ASSERT(VehicleSimulationSnapshotRefreshOffset,
                         offsetof(VehicleSimulationSnapshot, refresh_state) == 0xCC);
COMPILE_TIME_SIZE_ASSERT(VehicleSimulationSnapshotMinimumSpeedOffset,
                         offsetof(VehicleSimulationSnapshot, minimum_track_speed) == 0x160);
COMPILE_TIME_SIZE_ASSERT(PlayerVehicleSimulationSnapshotSize,
                         sizeof(PlayerVehicleSimulationSnapshot) == 0x19C);
COMPILE_TIME_SIZE_ASSERT(PlayerVehicleSnapshotTrackGravityOffset,
                         offsetof(PlayerVehicleSimulationSnapshot, track_gravity) == 0x18C);
COMPILE_TIME_SIZE_ASSERT(TrafficVehicleSimulationSnapshotSize,
                         sizeof(TrafficVehicleSimulationSnapshot) == 0x170);
COMPILE_TIME_SIZE_ASSERT(TrafficVehicleSnapshotArtVariantOffset,
                         offsetof(TrafficVehicleSimulationSnapshot, art_variant) == 0x16C);
COMPILE_TIME_SIZE_ASSERT(VehicleRuntimeStateCallbackOffset,
                         offsetof(VehicleRuntimeObjectState, state_update_callback) == 0x4C0);
COMPILE_TIME_SIZE_ASSERT(VehicleRuntimeConfigurationOffset,
                         offsetof(VehicleRuntimeObjectState, configuration_mode) == 0x4D0);
COMPILE_TIME_SIZE_ASSERT(VehicleRuntimeRenderResourceOffset,
                         offsetof(VehicleRuntimeObjectState, render_resource_state) == 0x4D4);
COMPILE_TIME_SIZE_ASSERT(VehicleRuntimeSpatialSoundGainOffset,
                         offsetof(VehicleRuntimeObjectState, spatial_sound_gain) == 0x450);
COMPILE_TIME_SIZE_ASSERT(VehicleRuntimeSpatialSoundConfigurationOffset,
                         offsetof(VehicleRuntimeObjectState, spatial_sound_configuration) == 0x454);
COMPILE_TIME_SIZE_ASSERT(VehicleSimulationTransformSourceSize,
                         sizeof(VehicleSimulationTransformSource) ==
                           VEHICLE_SIMULATION_TRANSFORM_SOURCE_BYTES);
COMPILE_TIME_SIZE_ASSERT(VehicleSimulationTransformWritebackSize,
                         sizeof(VehicleSimulationTransformWritebackState) ==
                           VEHICLE_SIMULATION_TRANSFORM_WRITEBACK_BYTES);
COMPILE_TIME_SIZE_ASSERT(VehicleSimulationStateSize,
                         sizeof(VehicleSimulationState) == VEHICLE_SIMULATION_STATE_BYTES);

int32
is_mode_five_vehicle_track_gap_in_range(int32 minimum_gap_, int32 maximum_gap_)
{
  VehicleSimulationState *player_state;
  VehicleSimulationState *target_state;

  if(g_OPPONENT_VEHICLE_INDEX == VEHICLE_SIMULATION_DISABLED_MODE)
    return 0;

  player_state = g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_PLAYER];
  target_state = g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_FIVE_TARGET];
  return _is_vehicle_track_gap_in_range(player_state->simulation_source.track_segment_index,
                                        target_state->simulation_source.track_segment_index,
                                        minimum_gap_,
                                        maximum_gap_);
}

int32
is_mode_six_vehicle_track_gap_in_range(int32 minimum_gap_, int32 maximum_gap_)
{
  VehicleSimulationState *player_state;
  VehicleSimulationState *target_state;

  target_state = g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_SIX_TARGET];
  if((target_state->simulation_source.status_flags & VEHICLE_MODE_SIX_TARGET_ACTIVE_FLAG_MASK) == 0)
    return 0;

  player_state = g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_PLAYER];
  return _is_vehicle_track_gap_in_range(player_state->simulation_source.track_segment_index,
                                        target_state->simulation_source.track_segment_index,
                                        minimum_gap_,
                                        maximum_gap_);
}

void
request_vehicle_render_sort_refresh(void)
{
  int32 state_index;

  for(state_index = 0; state_index < VEHICLE_SIMULATION_STATE_COUNT; ++state_index)
    g_VEHICLE_SIMULATION_STATES[state_index]->render_resource_state->sort_refresh_countdown = 1;
}

void
clear_vehicle_simulation_contact_flags(int32 clear_optional_flag_)
{
  VehicleSimulationContactState *active_state;
  VehicleSimulationContactState *target_state;

  target_state = (VehicleSimulationContactState *)
    g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_SIX_TARGET];
  if(clear_optional_flag_ != 0)
    {
      target_state->status_flags &= ~VEHICLE_SIMULATION_OPTIONAL_CONTACT_FLAG;
    }
  target_state->status_flags &= ~VEHICLE_SIMULATION_CONTACT_FLAG_MASK;
  active_state = (VehicleSimulationContactState *) G_ACTIVE_SIMULATION_SOURCE;
  active_state->status_flags &= ~VEHICLE_ACTIVE_SIMULATION_CONTACT_FLAG_MASK;
  g_VEHICLE_COLLISION_UPDATE_PENDING = 0;
}

void
reset_vehicle_collision_simulation(void)
{
  VehicleCollisionResetState *active_state;
  VehicleCollisionResetState *state;
  SimulationUpdateCallback    update_callback;
  int32                       state_index;

  clear_vehicle_simulation_contact_flags(1);
  active_state = (VehicleCollisionResetState *) G_ACTIVE_SIMULATION_SOURCE;
  for(state_index = VEHICLE_COLLISION_RESET_FIRST_INDEX;
      state_index < VEHICLE_SIMULATION_STATE_COUNT;
      ++state_index)
    {
      state = (VehicleCollisionResetState *) g_VEHICLE_SIMULATION_STATES[state_index];
      state->status_flags &= ~VEHICLE_COLLISION_RESET_FLAG_MASK;
      g_VEHICLE_COLLISION_UPDATE_PENDING = 0;
      update_callback                    = state->update_callback;

      if((update_callback != (SimulationUpdateCallback) update_recovering_vehicle_simulation) &&
         (update_callback != (SimulationUpdateCallback) update_player_vehicle_state) &&
         ((state->status_flags & VEHICLE_COLLISION_TRAFFIC_FLAG) == 0) &&
         ((state->status_flags & VEHICLE_COLLISION_REVERSE_FLAG) != 0) &&
         (state->track_segment_index < active_state->track_segment_index))
        continue;

      if((state->status_flags & VEHICLE_COLLISION_PLAYER_FLAG) == 0)
        {
          if(state == active_state)
            continue;
          if(update_callback == (SimulationUpdateCallback) update_traffic_vehicle_simulation)
            g_VEHICLE_SPAWN_POSITION_STATE.active_vehicle_count--;
          state->update_callback = (SimulationUpdateCallback) update_vehicle_simulation_transform;
          reposition_vehicle_simulation(
            (SimulationSource *) ((uint8 *) state + VEHICLE_SIMULATION_TRANSFORM_SOURCE_OFFSET),
            1);
          continue;
        }

      if(update_callback == (SimulationUpdateCallback) update_player_vehicle_state)
        {
          if(_is_vehicle_within_reset_boundaries(state) == 0)
            continue;
          update_simulation_from_transform(
            (SimulationSource *) state,
            (SimulationUpdateCallback) update_vehicle_simulation_transform);
          _reset_vehicle_collision_state(state);
        }
      else if(update_callback == (SimulationUpdateCallback) update_recovering_vehicle_simulation)
        {
          if(_is_vehicle_within_reset_boundaries(state) == 0)
            continue;
          state->update_callback = (SimulationUpdateCallback) update_vehicle_simulation_transform;
          _reset_vehicle_collision_state(state);
        }
    }
}

void
update_simulation_from_transform(SimulationSource        *source_,
                                 SimulationUpdateCallback update_callback_)
{
  VehicleSimulationTransformWritebackState *state;
  VehicleSimulationTransformSource         *source;
  MatrixControllerState                    *transform;
  int32                                     angular_velocity;
  int32                                     position_offset;

  state                  = (VehicleSimulationTransformWritebackState *) source_;
  source                 = &state->transform_source;
  transform              = source->transform;
  state->update_callback = update_callback_;
  source->position.x     = transform->position.x;
  source->position.y     = transform->position.y;
  source->position.z     = transform->position.z;
  position_offset        = (source->position_offset >> VEHICLE_SIMULATION_FIXED8_SHIFT);
  source->position.x -=
    (position_offset * (transform->matrix.m10 >> VEHICLE_SIMULATION_FIXED8_SHIFT));
  source->position.y -=
    (position_offset * (transform->matrix.m11 >> VEHICLE_SIMULATION_FIXED8_SHIFT));
  source->position.z -=
    (position_offset * (transform->matrix.m12 >> VEHICLE_SIMULATION_FIXED8_SHIFT));
  source->position.z = -source->position.z;
  source->velocity.x = -transform->velocity.x;
  source->velocity.y = transform->velocity.y;
  source->velocity.z = -transform->velocity.z;
  angular_velocity =
    -MulSF16(transform->angular_velocity.y, VEHICLE_SIMULATION_ANGULAR_VELOCITY_RECIPROCAL);
  source->angular_velocity +=
    ((angular_velocity - source->angular_velocity) >> VEHICLE_SIMULATION_ANGULAR_SMOOTHING_SHIFT);
  transform->contact_countdown = 0;
  state->primary_sound_level   = 0;
  state->secondary_sound_level = 0;

  if(source_ != (SimulationSource *) G_ACTIVE_SIMULATION_SOURCE)
    return;

  g_ACTIVE_SIMULATION_SOUND_INDEX   = -1;
  g_ACTIVE_SIMULATION_SOUND_VARIANT = -1;
}

void
rebuild_simulation_transform(void *transform_source_)
{
  VehicleSimulationTransformSource *source;
  MatrixControllerState            *transform;
  int32                             position_offset;

  source                = (VehicleSimulationTransformSource *) transform_source_;
  transform             = source->transform;
  transform->position.x = source->position.x;
  transform->position.y = source->position.y;
  transform->position.z = -source->position.z;
  transform->velocity.x = source->velocity.x;
  transform->velocity.y = source->velocity.y;
  transform->velocity.z = source->velocity.z;
  transform->fields_48[VEHICLE_SIMULATION_ACCELERATION_X_INDEX] = 0;
  transform->fields_48[VEHICLE_SIMULATION_ACCELERATION_Y_INDEX] = -VEHICLE_SIMULATION_GRAVITY_FIXED;
  transform->fields_48[VEHICLE_SIMULATION_ACCELERATION_Z_INDEX] = 0;
  transform->angular_velocity.x                                 = 0;
  transform->angular_velocity.y =
    -MulSF16(source->angular_velocity, VEHICLE_SIMULATION_ANGULAR_VELOCITY_SCALE);
  transform->angular_velocity.z = 0;
  transform->velocity.z         = -transform->velocity.z;
  transform->velocity.x         = -transform->velocity.x;

  if(source->transform_override_enabled == 0)
    {
      initialize_matrix3x3_rotation_xzy(&transform->matrix,
                                        -source->rotation.x,
                                        source->rotation.z,
                                        -source->rotation.y);
    }
  else
    {
      transform->matrix.m00 = source->transform_override.m00;
      transform->matrix.m01 = source->transform_override.m01;
      transform->matrix.m02 = -source->transform_override.m02;
      transform->matrix.m10 = source->transform_override.m10;
      transform->matrix.m11 = source->transform_override.m11;
      transform->matrix.m12 = -source->transform_override.m12;
      transform->matrix.m20 = -source->transform_override.m20;
      transform->matrix.m21 = -source->transform_override.m21;
      transform->matrix.m22 = source->transform_override.m22;
    }

  position_offset = (source->position_offset >> VEHICLE_SIMULATION_FIXED8_SHIFT);
  transform->position.x +=
    (position_offset * (transform->matrix.m10 >> VEHICLE_SIMULATION_FIXED8_SHIFT));
  transform->position.y +=
    (position_offset * (transform->matrix.m11 >> VEHICLE_SIMULATION_FIXED8_SHIFT));
  transform->position.z +=
    (position_offset * (transform->matrix.m12 >> VEHICLE_SIMULATION_FIXED8_SHIFT));
}

void
prepare_simulation_collision_transform(SimulationSource *source_)
{
  VehicleSimulationTransformState *state;

  state = (VehicleSimulationTransformState *) source_;
  rebuild_simulation_transform((uint8 *) source_ + VEHICLE_SIMULATION_TRANSFORM_SOURCE_OFFSET);
  state->update_callback              = update_player_vehicle_state;
  state->transform->contact_countdown = VEHICLE_SIMULATION_COLLISION_TRANSFORM_CONTROL_VALUE;
}

void
prepare_simulation_transform(SimulationSource *source_)
{
  VehicleSimulationTransformState *state;

  state = (VehicleSimulationTransformState *) source_;
  rebuild_simulation_transform((uint8 *) source_ + VEHICLE_SIMULATION_TRANSFORM_SOURCE_OFFSET);
  g_RUNTIME_STATE.collision_simulation_enabled = 1;
  state->update_callback                       = update_player_vehicle_state;
  state->status_flags &= ~VEHICLE_SIMULATION_TRANSFORM_STALE_FLAG;
  state->transform->contact_countdown = VEHICLE_SIMULATION_PLAYER_TRANSFORM_CONTROL_VALUE;
  request_sound_effect(VEHICLE_SIMULATION_TRANSFORM_SOUND_ID);
}

void
apply_simulation_local_impulse(SimulationSource *source_,
                               int32             longitudinal_impulse_,
                               int32             lateral_impulse_,
                               int32             vertical_impulse_)
{
  VehicleSimulationImpulseState *state;
  int32                          longitudinal_x;
  int32                          longitudinal_y;
  int32                          longitudinal_z;
  int32                          lateral_x;
  int32                          lateral_y;
  int32                          lateral_z;
  int32                          vertical_x;
  int32                          vertical_y;
  int32                          vertical_z;

  state = (VehicleSimulationImpulseState *) source_;
  prepare_simulation_transform(source_);
  vertical_x     = MulSF16(vertical_impulse_, state->transform->m00);
  vertical_y     = MulSF16(vertical_impulse_, state->transform->m01);
  vertical_z     = MulSF16(vertical_impulse_, state->transform->m02);
  longitudinal_x = MulSF16(longitudinal_impulse_, state->transform->m10);
  longitudinal_y = MulSF16(longitudinal_impulse_, state->transform->m11);
  longitudinal_z = MulSF16(longitudinal_impulse_, state->transform->m12);
  lateral_x      = MulSF16(lateral_impulse_, state->transform->m20);
  lateral_y      = MulSF16(lateral_impulse_, state->transform->m21);
  lateral_z      = MulSF16(lateral_impulse_, state->transform->m22);

  state->accumulated_impulse.x += vertical_x;
  state->accumulated_impulse.y += vertical_y;
  state->accumulated_impulse.z += vertical_z;
  state->accumulated_impulse.x += lateral_x;
  state->accumulated_impulse.y += lateral_y;
  state->accumulated_impulse.z += lateral_z;
  state->accumulated_impulse.x -= longitudinal_x;
  state->accumulated_impulse.y -= longitudinal_y;
  state->accumulated_impulse.z -= longitudinal_z;
  if((state->status_flags & VEHICLE_SIMULATION_CONTACT_RESET_FLAG) != 0)
    clear_vehicle_simulation_contact_flags(VEHICLE_SIMULATION_KEEP_OPTIONAL_FLAG);
}

void
apply_control_sequence_vehicle_impulses(void)
{
  int32 index;

  if(g_CONTROL_INPUT_SEQUENCE_STAGE != VEHICLE_CONTROL_SEQUENCE_COMPLETE_STAGE)
    {
      return;
    }
  if(G_GAME_FRAME_COUNTER < VEHICLE_CONTROL_SEQUENCE_IMPULSE_START_FRAME)
    {
      return;
    }

  for(index = VEHICLE_CONTROL_SEQUENCE_FIRST_TARGET_INDEX; index < VEHICLE_SIMULATION_STATE_COUNT;
      index++)
    {
      apply_simulation_local_impulse((SimulationSource *) g_VEHICLE_SIMULATION_STATES[index],
                                     VEHICLE_CONTROL_SEQUENCE_IMPULSE,
                                     VEHICLE_CONTROL_SEQUENCE_IMPULSE,
                                     VEHICLE_CONTROL_SEQUENCE_IMPULSE);
    }
}

static int32
_is_vehicle_track_gap_in_range(int32 player_track_segment_,
                               int32 target_track_segment_,
                               int32 minimum_gap_,
                               int32 maximum_gap_)
{
  int32 track_gap;

  track_gap = (player_track_segment_ - target_track_segment_);
  if(track_gap <= minimum_gap_)
    return 0;
  if(track_gap <= maximum_gap_)
    return 1;

  return 0;
}

static int32
_is_vehicle_within_reset_boundaries(const VehicleCollisionResetState *state_)
{
  const TrackSegment *segment;
  int32               negative_boundary;
  int32               positive_boundary;

  segment = &g_TRACK_SEGMENTS[state_->track_segment_index];
  negative_boundary =
    -((int32) segment->negative_boundary_width << VEHICLE_COLLISION_TRACK_WIDTH_SHIFT);
  positive_boundary =
    ((int32) segment->positive_boundary_width << VEHICLE_COLLISION_TRACK_WIDTH_SHIFT);
  return ((state_->lateral_position >= negative_boundary) &&
          (state_->lateral_position <= positive_boundary));
}

static void
_reset_vehicle_collision_state(VehicleCollisionResetState *state_)
{
  state_->heading_velocity    = 0;
  state_->heading_rotation    = 0;
  state_->transform_active    = 0;
  state_->track_transform     = state_->track_basis;
  state_->motion.x            = 0;
  state_->motion.y            = 0;
  state_->motion.z            = 0;
  state_->minimum_track_speed = 0;
}
