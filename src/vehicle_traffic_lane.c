#include "vehicle_traffic_lane.h"

#include "doubly_linked_list.h"
#include "driving_warning_display.h"
#include "frontend_transition.h"
#include "game_frame_count.h"
#include "geometry.h"
#include "player_vehicle_state.h"
#include "road_data.h"
#include "road_object_table.h"
#include "signal_status.h"
#include "simulation_thread_signal.h"
#include "sound_spatial.h"
#include "track_disk_stream.h"
#include "track_position.h"
#include "vehicle_drive_initialization.h"
#include "vehicle_drive_statistics.h"
#include "vehicle_dynamics_state.h"
#include "vehicle_motion_projection.h"
#include "vehicle_parameter_loader.h"
#include "vehicle_pair_collision.h"
#include "vehicle_render_resource_selection.h"
#include "vehicle_traffic_randomization.h"
#include "vehicle_traffic_spacing.h"
#include "vehicle_vector_projection.h"

#include "operamath.h"

#include "stddef.h"

#define VEHICLE_TRAFFIC_LANE_REVERSE_FLAG 0x1000
#define VEHICLE_TRAFFIC_LANE_PLAYER_FLAG 4
#define VEHICLE_TRAFFIC_LANE_PLAYER_OVERRIDE_MASK 0x404
#define VEHICLE_TRAFFIC_LANE_RUNTIME_ACTIVE_FLAG 4
#define VEHICLE_TRAFFIC_LANE_PLAYER_SLOT_MINIMUM 3
#define VEHICLE_TRAFFIC_LANE_MAXIMUM_INDEX 7
#define VEHICLE_TRAFFIC_LANE_SEGMENT_GAP_ADJUSTMENT 2
#define VEHICLE_TRAFFIC_LANE_DEFAULT_SEARCH_DISTANCE 0x1B
#define VEHICLE_TRAFFIC_LANE_CLOSE_SEGMENT_DISTANCE 3
#define VEHICLE_TRAFFIC_LANE_PLAYER_MINIMUM_DISTANCE 3
#define VEHICLE_TRAFFIC_LANE_PLAYER_MAXIMUM_DISTANCE 0x0F
#define VEHICLE_TRAFFIC_LANE_PRIMARY_SEGMENT_SCALE 0x50000
#define VEHICLE_TRAFFIC_LANE_SECONDARY_SEGMENT_SCALE 0x1B6DB
#define VEHICLE_TRAFFIC_LANE_SEGMENT_DISTANCE_SCALE 0x60000
#define VEHICLE_TRAFFIC_LANE_RELATIVE_MOTION_SCALE 0x13333
#define VEHICLE_TRAFFIC_LANE_PLAYER_RELATIVE_SCALE 0x50000
#define VEHICLE_TRAFFIC_LANE_PLAYER_THRESHOLD_SCALE 0x20000
#define VEHICLE_TRAFFIC_LANE_BOUNDARY_NIBBLE_MASK 0x0F
#define VEHICLE_TRAFFIC_LANE_NEGATIVE_BOUNDARY_SHIFT 4
#define VEHICLE_TRAFFIC_LANE_POSITIVE_BOUNDARY_BASE 4
#define VEHICLE_TRAFFIC_LANE_NEGATIVE_BOUNDARY_BASE 3
#define VEHICLE_TRAFFIC_LANE_TARGET_SPEED_LIMIT 0x14CCC
#define VEHICLE_TRAFFIC_LANE_SPEED_DIFFERENCE_SHIFT 8
#define VEHICLE_TRAFFIC_LANE_PRIMARY_TARGET_SCORE_SCALE (-0xCD)
#define VEHICLE_TRAFFIC_LANE_SECONDARY_TARGET_SCORE_SCALE (-0x80)
#define VEHICLE_TRAFFIC_LANE_PRIMARY_SPEED_SCORE_SCALE (-0x100)
#define VEHICLE_TRAFFIC_LANE_SECONDARY_SPEED_SCORE_SCALE (-0x1A)
#define VEHICLE_TRAFFIC_LANE_LEFT_SPEED_INDEX 0
#define VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX 1
#define VEHICLE_TRAFFIC_LANE_RIGHT_SPEED_INDEX 2
#define VEHICLE_TRAFFIC_EVENT_SPEED_MARGIN_SHIFT 4
#define VEHICLE_TRAFFIC_EVENT_RANDOM_WORD_MASK 0xFFFF
#define VEHICLE_TRAFFIC_EVENT_RANDOM_VALUE_MASK 0x7F
#define VEHICLE_TRAFFIC_EVENT_PROBABILITY_SHIFT 16
#define VEHICLE_TRAFFIC_EVENT_IMPULSE_RANDOM_SHIFT 8
#define VEHICLE_TRAFFIC_EVENT_TABLE_INDEX_MASK 0x1C0
#define VEHICLE_TRAFFIC_EVENT_TABLE_INDEX_SHIFT 6
#define VEHICLE_TRAFFIC_EVENT_FLAG_HIGH_SHIFT 8
#define VEHICLE_TRAFFIC_EVENT_DIRECTION_FLAG 0x200
#define VEHICLE_TRAFFIC_EVENT_STATUS_TRAFFIC_FLAG 8
#define VEHICLE_TRAFFIC_EVENT_SOUND_ID 0x52
#define VEHICLE_TRAFFIC_EVENT_SOUND_MINIMUM_DISTANCE (-4)
#define VEHICLE_TRAFFIC_EVENT_SOUND_MAXIMUM_DISTANCE 15
#define VEHICLE_TRAFFIC_EVENT_IMPULSE_SCALE 3
#define VEHICLE_TRAFFIC_EVENT_PRIMARY_IMPULSE_LIMIT 100
#define VEHICLE_TRAFFIC_EVENT_ALTERNATE_IMPULSE_MINIMUM 126
#define VEHICLE_TRAFFIC_EVENT_RANDOM_RANGE_LIMIT 127
#define VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_ZERO 3
#define VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_ONE 5
#define VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_TWO 13
#define VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_THREE 19
#define VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_FOUR 28
#define VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_FIVE 36
#define VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_SIX 51
#define VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_SEVEN 64
#define VEHICLE_TRACK_FEATURE_SEGMENT_OFFSET 0x31
#define VEHICLE_TRACK_FEATURE_SEGMENTS_PER_SECTION 4
#define VEHICLE_TRACK_FEATURE_PRESENT_FLAG 1
#define VEHICLE_TRACK_FEATURE_DIRECTION_FLAG 2
#define VEHICLE_TRACK_FEATURE_DISABLE_LIMIT_FLAG 4
#define VEHICLE_TRACK_FEATURE_TABLE_INDEX_MASK 0x18
#define VEHICLE_TRACK_FEATURE_TABLE_INDEX_SHIFT 3
#define VEHICLE_TRACK_FEATURE_RANDOM_SHIFT 16
#define VEHICLE_TRACK_FEATURE_RANDOM_MASK 0x7F
#define VEHICLE_TRACK_FEATURE_ACTIVE_FLAG 4
#define VEHICLE_TRACK_FEATURE_STATUS_PENDING_FLAG 0x80000
#define VEHICLE_TRACK_FEATURE_SPEED_HOLD_FLAG 0x20000
#define VEHICLE_TRACK_FEATURE_EDGE_WIDTH_SCALE 0x2000
#define VEHICLE_TRACK_FEATURE_PLACEMENT_NEGATIVE 0
#define VEHICLE_TRACK_FEATURE_PLACEMENT_POSITIVE 1
#define VEHICLE_TRACK_FEATURE_ACTIVATE_RECORD 0
#define VEHICLE_TRACK_FEATURE_PROBABILITY_ZERO 0x20
#define VEHICLE_TRACK_FEATURE_PROBABILITY_ONE 0x40
#define VEHICLE_TRACK_FEATURE_PROBABILITY_TWO 0x60
#define VEHICLE_TRACK_FEATURE_PROBABILITY_THREE 0x80
#define VEHICLE_OVERTAKE_FIRST_RACER_SLOT 1
#define VEHICLE_OVERTAKE_LAST_RACER_SLOT 6
#define VEHICLE_OVERTAKE_ACTIVE_MARKER 1
#define VEHICLE_OVERTAKE_INACTIVE_MARKER 0
#define VEHICLE_OVERTAKE_SOUND_ID 0x50
#define VEHICLE_OVERTAKE_LANE_NIBBLE_MASK 0x0F
#define VEHICLE_OVERTAKE_LANE_NIBBLE_SHIFT 4
#define VEHICLE_OVERTAKE_NEGATIVE_LANE_BASE 3
#define VEHICLE_OVERTAKE_POSITIVE_LANE_BASE 4
#define VEHICLE_PROXIMITY_PRIMARY_EVENT_FLAG 0x400
#define VEHICLE_PROXIMITY_NEARBY_FLAG 0x800
#define VEHICLE_PROXIMITY_REVERSE_FLAG 0x1000
#define VEHICLE_PROXIMITY_SECONDARY_EVENT_FLAG 0x2000
#define VEHICLE_PROXIMITY_LANE_AVOIDANCE_FLAG 0x8000
#define VEHICLE_PROXIMITY_ACTIVE_EVENT_FLAG 0x10000
#define VEHICLE_PROXIMITY_SPEED_HOLD_FLAG 0x20000
#define VEHICLE_PROXIMITY_EVENT_RECORDED_FLAG 0x80000
#define VEHICLE_PROXIMITY_EVENT_FLAGS 0x2400
#define VEHICLE_PROXIMITY_EVENT_CLEAR_FLAGS 0x80400
#define VEHICLE_PROXIMITY_FIXED_ONE 0x10000
#define VEHICLE_PROXIMITY_SPEED_ACTIVATION_LIMIT 0x30000
#define VEHICLE_PROXIMITY_AVOIDANCE_OFFSET 0x2E0000
#define VEHICLE_PROXIMITY_NEAR_SEGMENT_LIMIT 10
#define VEHICLE_PROXIMITY_TRIGGER_SEGMENT_LIMIT 5
#define VEHICLE_PROXIMITY_ACTIVE_SEGMENT_LIMIT 3
#define VEHICLE_PROXIMITY_TRACK_GROUP_SHIFT 2
#define VEHICLE_PROXIMITY_TRACK_GROUP_STRIDE 3
#define VEHICLE_PROXIMITY_TRACK_SPEED_OFFSET 1
#define VEHICLE_PROXIMITY_LANE_SCALE 2
#define VEHICLE_PROXIMITY_NEGATIVE_LANE_MASK 0xF0
#define VEHICLE_PROXIMITY_GAME_ACTIVE 1
#define VEHICLE_PROXIMITY_GAME_EVENT_FLAG 4
#define VEHICLE_PROXIMITY_EVENT_WINDOW_SEGMENTS 4
#define VEHICLE_PROXIMITY_ATTEMPT_LIMIT_ADJUSTMENT 1
#define VEHICLE_PROXIMITY_TRACK_POSITION_SCALE 0x60000
#define VEHICLE_TRAFFIC_RESPONSE_FIXED_ONE 0x10000
#define VEHICLE_TRAFFIC_RESPONSE_FINISH_OFFSET 0x640000
#define VEHICLE_TRAFFIC_RESPONSE_AVOIDANCE_OFFSET 0x70000
#define VEHICLE_TRAFFIC_RESPONSE_TRACK_POSITION_SCALE 0x60000
#define VEHICLE_TRAFFIC_RESPONSE_PROJECTION_EXCLUSION 0x50000
#define VEHICLE_TRAFFIC_RESPONSE_MINIMUM_PROJECTION 0x4FFFF
#define VEHICLE_TRAFFIC_RESPONSE_SOUND_SPEED_LIMIT 0xF0000
#define VEHICLE_TRAFFIC_RESPONSE_DIRECTION_SCALE 0x400
#define VEHICLE_TRAFFIC_RESPONSE_DIRECTION_HALF_TURN 0x800000
#define VEHICLE_TRAFFIC_RESPONSE_DIRECTION_FULL_TURN 0x1000000
#define VEHICLE_TRAFFIC_RESPONSE_RATE_LIMIT 0x8000
#define VEHICLE_TRAFFIC_RESPONSE_FILTER_SHIFT 3
#define VEHICLE_TRAFFIC_RESPONSE_FILTER_BIAS 7
#define VEHICLE_TRAFFIC_RESPONSE_PRIMARY_TARGET_SHIFT 17
#define VEHICLE_TRAFFIC_RESPONSE_SECONDARY_TARGET_SHIFT 18
#define VEHICLE_TRAFFIC_RESPONSE_DIRECTION_HIGH_INDEX 4
#define VEHICLE_TRAFFIC_RESPONSE_DIRECTION_LOW_INDEX 5
#define VEHICLE_TRAFFIC_RESPONSE_BIG_ENDIAN_SHIFT 8
#define VEHICLE_TRAFFIC_RESPONSE_PROJECTION_SHIFT 8
#define VEHICLE_TRAFFIC_RESPONSE_MAXIMUM_MARGIN 200
#define VEHICLE_TRAFFIC_RESPONSE_PENDING_VALUE 3000
#define VEHICLE_TRAFFIC_RESPONSE_FORWARD_RANGE_ADJUSTMENT 1
#define VEHICLE_TRAFFIC_RESPONSE_MINIMUM_TRACK_SEGMENT 67
#define VEHICLE_TRAFFIC_RESPONSE_SAME_LANE_THRESHOLD 1
#define VEHICLE_TRAFFIC_RESPONSE_WIDE_SPACING_THRESHOLD 20
#define VEHICLE_TRAFFIC_RESPONSE_MEDIUM_SPACING_THRESHOLD 8
#define VEHICLE_TRAFFIC_RESPONSE_NARROW_SPACING_THRESHOLD 3
#define VEHICLE_TRAFFIC_RESPONSE_SOUND_GAP_ONE 2
#define VEHICLE_TRAFFIC_RESPONSE_SOUND_GAP_TWO 5
#define VEHICLE_TRAFFIC_RESPONSE_SOUND_VARIANT 0
#define VEHICLE_TRAFFIC_RESPONSE_RANDOM_MASK 0xFFFF
#define VEHICLE_TRAFFIC_RESPONSE_RANDOM_VALUE_MASK 0xFFFFFF
#define VEHICLE_TRAFFIC_RESPONSE_RANDOM_VALUE_SHIFT 8
#define VEHICLE_TRAFFIC_RESPONSE_RANDOM_SCALE 10
#define VEHICLE_TRAFFIC_RESPONSE_RANDOM_BIAS 0x8000
#define VEHICLE_TRAFFIC_RESPONSE_RANDOM_OUTPUT_SHIFT 16
#define VEHICLE_TRAFFIC_RESPONSE_SPEED_THRESHOLD_COUNT 5
#define VEHICLE_TRAFFIC_RESPONSE_SCALE_COUNT 6
#define VEHICLE_TRAFFIC_RESPONSE_FIRST_SCALE_INDEX 0
#define VEHICLE_TRAFFIC_RESPONSE_ACTIVE_FLAG 1
#define VEHICLE_TRAFFIC_RESPONSE_INACTIVE_FLAG 0
#define VEHICLE_TRAFFIC_RESPONSE_REFRESH_ZERO 6
#define VEHICLE_TRAFFIC_RESPONSE_REFRESH_ONE 5
#define VEHICLE_TRAFFIC_RESPONSE_REFRESH_TWO 4
#define VEHICLE_TRAFFIC_RESPONSE_REFRESH_THREE 4
#define VEHICLE_TRAFFIC_RESPONSE_REFRESH_FOUR 7
#define VEHICLE_TRAFFIC_RESPONSE_REFRESH_FIVE 5
#define VEHICLE_TRAFFIC_RESPONSE_REFRESH_SIX 6
#define VEHICLE_TRAFFIC_RESPONSE_REFRESH_SEVEN 4
#define VEHICLE_TRAFFIC_RESPONSE_RESULT_A 2
#define VEHICLE_TRAFFIC_RESPONSE_RESULT_B 3
#define VEHICLE_TRAFFIC_RESPONSE_UNSIGNED_SCALE_ONE 0x1AAAA
#define VEHICLE_TRAFFIC_RESPONSE_UNSIGNED_SCALE_TWO 0x35555
#define VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT 8
#define VEHICLE_TRAFFIC_PROJECTION_PALETTE_SLOT_MINIMUM 3
#define VEHICLE_TRAFFIC_PROJECTION_PALETTE_DISABLED_MODE 3
#define VEHICLE_TRAFFIC_PROJECTION_STATUS_PLAYER_FLAG 4
#define VEHICLE_TRAFFIC_PROJECTION_STATUS_TRAFFIC_FLAG 8
#define VEHICLE_TRAFFIC_PROJECTION_STATUS_ACTIVE_FLAG 0x400
#define VEHICLE_TRAFFIC_PROJECTION_STATUS_OVERRIDE_MASK 0x404
#define VEHICLE_TRAFFIC_PROJECTION_STATUS_REVERSE_FLAG 0x1000
#define VEHICLE_TRAFFIC_PROJECTION_STATUS_SPEED_HOLD_FLAG 0x20000
#define VEHICLE_TRAFFIC_PROJECTION_STATUS_CLEAR_MASK 0x4003
#define VEHICLE_TRAFFIC_PROJECTION_STATUS_DEFAULT_FLAGS 0x1E0
#define VEHICLE_TRAFFIC_PROJECTION_TRACK_GROUP_SHIFT 2
#define VEHICLE_TRAFFIC_PROJECTION_TRACK_GROUP_STRIDE 3
#define VEHICLE_TRAFFIC_PROJECTION_TRACK_SPEED_OFFSET 2
#define VEHICLE_TRAFFIC_PROJECTION_TRACK_SPEED_SHIFT 16
#define VEHICLE_TRAFFIC_PROJECTION_REVERSE_SPEED (-910222)
#define VEHICLE_TRAFFIC_PROJECTION_CLOSE_SEGMENT_MAXIMUM 3
#define VEHICLE_TRAFFIC_PROJECTION_CLOSE_SEGMENT_MINIMUM (-2)
#define VEHICLE_TRAFFIC_PROJECTION_EVENT_LIMIT 900
#define VEHICLE_TRAFFIC_PROJECTION_EXTENT_X_INDEX 0
#define VEHICLE_TRAFFIC_PROJECTION_EXTENT_Y_INDEX 1
#define VEHICLE_TRAFFIC_PROJECTION_EXTENT_Z_INDEX 2
#define VEHICLE_TRAFFIC_COORDINATOR_FRAME_STEP 4
#define VEHICLE_TRAFFIC_COORDINATOR_LANE_COUNT 8
#define VEHICLE_TRAFFIC_COORDINATOR_VEHICLE_COUNT 7
#define VEHICLE_TRAFFIC_COORDINATOR_SCORE_COUNT 3
#define VEHICLE_TRAFFIC_COORDINATOR_LANE_SCALE 0x33
#define VEHICLE_TRAFFIC_COORDINATOR_POSITION_SHIFT 8
#define VEHICLE_TRAFFIC_COORDINATOR_SCALE_SHIFT 16
#define VEHICLE_TRAFFIC_COORDINATOR_LANE_BIAS 4
#define VEHICLE_TRAFFIC_COORDINATOR_LANE_MINIMUM 0
#define VEHICLE_TRAFFIC_COORDINATOR_LANE_MAXIMUM 7
#define VEHICLE_TRAFFIC_COORDINATOR_EXTENT_ROUNDING 0x8000
#define VEHICLE_TRAFFIC_COORDINATOR_SINGLE_LANE_MASK 1
#define VEHICLE_TRAFFIC_COORDINATOR_DOUBLE_LANE_MASK 3
#define VEHICLE_TRAFFIC_COORDINATOR_TRIPLE_LANE_MASK 7
#define VEHICLE_TRAFFIC_COORDINATOR_LANE_DEBOUNCE_LIMIT 5
#define VEHICLE_TRAFFIC_COORDINATOR_RUNTIME_ACTIVE_FLAG 4
#define VEHICLE_TRAFFIC_COORDINATOR_NEARBY_FLAGS 0xC00
#define VEHICLE_TRAFFIC_COORDINATOR_REVERSE_FLAG 0x1000
#define VEHICLE_TRAFFIC_COORDINATOR_SPEED_HOLD_FLAG 0x20000
#define VEHICLE_TRAFFIC_COORDINATOR_EVENT_MASK 0x404
#define VEHICLE_TRAFFIC_COORDINATOR_PLAYER_FLAG 4
#define VEHICLE_TRAFFIC_COORDINATOR_TRAFFIC_FLAG 8
#define VEHICLE_TRAFFIC_COORDINATOR_TRACK_EVENT_FLAG 0x20
#define VEHICLE_TRAFFIC_COORDINATOR_EVENT_SEGMENT_LIMIT 20
#define VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_MASK 0x0F
#define VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_SHIFT 4
#define VEHICLE_TRAFFIC_COORDINATOR_NEGATIVE_LANE_BASE 3
#define VEHICLE_TRAFFIC_COORDINATOR_POSITIVE_LANE_BASE 4
#define VEHICLE_TRAFFIC_COORDINATOR_DIRECTION_DIVIDE 4
#define VEHICLE_TRAFFIC_COORDINATOR_FORWARD_SIDE_PENALTY 0x30000
#define VEHICLE_TRAFFIC_COORDINATOR_REVERSE_SIDE_PENALTY 0x50000
#define VEHICLE_TRAFFIC_COORDINATOR_BOUNDARY_BONUS 0x280000
#define VEHICLE_TRAFFIC_COORDINATOR_PLAYER_EDGE_PENALTY 0x0F0000
#define VEHICLE_TRAFFIC_COORDINATOR_TRAFFIC_EDGE_PENALTY 0x260000
#define VEHICLE_TRAFFIC_COORDINATOR_EDGE_CLEARANCE_PENALTY 0x640000
#define VEHICLE_TRAFFIC_COORDINATOR_OUTSIDE_LANE_PENALTY 0x3E80000
#define VEHICLE_TRAFFIC_COORDINATOR_BOUNDARY_WIDTH_SCALE 0x2000
#define VEHICLE_TRAFFIC_COORDINATOR_TRANSITION_SEGMENT_GAP 11
#define VEHICLE_TRAFFIC_COORDINATOR_FIXED_ONE 0x10000
#define VEHICLE_TRAFFIC_COORDINATOR_REVERSE_NEIGHBOR_SPEED 0xA0000
#define VEHICLE_TRAFFIC_COORDINATOR_REVERSE_NEIGHBOR_PENALTY 0x1E0000
#define VEHICLE_TRAFFIC_COORDINATOR_SOUND_SEGMENT_LIMIT 30
#define VEHICLE_TRAFFIC_COORDINATOR_SOUND_SPEED_MINIMUM (-0x30000)
#define VEHICLE_TRAFFIC_COORDINATOR_SOUND_RANDOM_MASK 0xFFFF
#define VEHICLE_TRAFFIC_COORDINATOR_SOUND_VALUE_MASK 0xFFFFFF
#define VEHICLE_TRAFFIC_COORDINATOR_SOUND_VALUE_SHIFT 8
#define VEHICLE_TRAFFIC_COORDINATOR_SOUND_BYTE_MASK 0xFF
#define VEHICLE_TRAFFIC_COORDINATOR_SOUND_PROBABILITY 20
#define VEHICLE_TRAFFIC_COORDINATOR_LANE_POSITION_SCALE 0x50000
#define VEHICLE_TRAFFIC_COORDINATOR_LANE_POSITION_BIAS 0x118000
#define VEHICLE_TRAFFIC_COORDINATOR_LEFT_SCORE_INDEX 0
#define VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX 1
#define VEHICLE_TRAFFIC_COORDINATOR_RIGHT_SCORE_INDEX 2
#define VEHICLE_TRAFFIC_COORDINATOR_SECOND_ADJACENT_LANE 2

#define COMPILE_TIME_SIZE_ASSERT(name_, expression_)                                               \
  typedef char compile_time_size_assert_##name_[(expression_) ? 1 : -1]

typedef struct VehicleTrafficLaneState
{
  DoublyLinkedNode         node;
  uint8                    reserved_008[0x7C];
  int32                    projected_lateral_extent;
  uint8                    reserved_088[0x424];
  int32                    spatial_sound_variant;
  uint8                    reserved_4_b0[0x14];
  SimulationUpdateCallback update_callback;
  int32                    runtime_slot_index;
  uint32                   runtime_flags;
} VehicleTrafficLaneState;

typedef struct VehicleTrafficProjectionExtent
{
  uint8 reserved_000[0x74];
  int32 axis_extents[3];
} VehicleTrafficProjectionExtent;

typedef struct VehicleTrafficCoordinatorStatistics
{
  uint8 reserved_000[0x40];
  int32 active_lane_index;
  int32 lane_change_debounce;
  int32 lane_change_count;
} VehicleTrafficCoordinatorStatistics;

typedef struct VehicleTrafficCoordinatorReference
{
  uint8 reserved_000[0x74];
  int32 lateral_reference;
  uint8 reserved_078[0x18];
  int32 lane_transition_scale;
} VehicleTrafficCoordinatorReference;

typedef struct VehicleTrackFeatureReferenceState
{
  uint8 reserved_000[0x74];
  int32 lateral_reference;
} VehicleTrackFeatureReferenceState;

int32 g_VEHICLE_TRAFFIC_COORDINATION_DISABLED;
int32 g_VEHICLE_TRAFFIC_ACTIVE_LANE_INDEX;
int32 g_VEHICLE_TRAFFIC_LANE_CONSTRAINTS_ENABLED;
int32 g_VEHICLE_TRAFFIC_COORDINATION_COUNTER;
int32 g_VEHICLE_LAST_TRACK_FEATURE_SEGMENT;

typedef struct VehicleTrafficResponseState
{
  uint8 reserved_000[0x31C];
  int32 speed_thresholds[VEHICLE_TRAFFIC_RESPONSE_SPEED_THRESHOLD_COUNT];
  uint8 reserved_330[4];
  int32 response_scales[VEHICLE_TRAFFIC_RESPONSE_SCALE_COUNT];
  int32 maximum_response;
  uint8 reserved_350[0xB8];
  int32 smoothed_response;
} VehicleTrafficResponseState;

int32 g_VEHICLE_TRAFFIC_EVENT_PROBABILITIES[VEHICLE_TRAFFIC_EVENT_PROBABILITY_COUNT] = {
  VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_ZERO, VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_ONE,
  VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_TWO,  VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_THREE,
  VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_FOUR, VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_FIVE,
  VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_SIX,  VEHICLE_TRAFFIC_EVENT_PROBABILITY_LEVEL_SEVEN
};
int32 g_VEHICLE_TRACK_FEATURE_PROBABILITIES[VEHICLE_TRACK_FEATURE_PROBABILITY_COUNT] = {
  VEHICLE_TRACK_FEATURE_PROBABILITY_ZERO,
  VEHICLE_TRACK_FEATURE_PROBABILITY_ONE,
  VEHICLE_TRACK_FEATURE_PROBABILITY_TWO,
  VEHICLE_TRACK_FEATURE_PROBABILITY_THREE
};
int32 g_VEHICLE_TRAFFIC_REFRESH_MODES[VEHICLE_TRAFFIC_REFRESH_MODE_COUNT] = {
  VEHICLE_TRAFFIC_RESPONSE_REFRESH_ZERO, VEHICLE_TRAFFIC_RESPONSE_REFRESH_ONE,
  VEHICLE_TRAFFIC_RESPONSE_REFRESH_TWO,  VEHICLE_TRAFFIC_RESPONSE_REFRESH_THREE,
  VEHICLE_TRAFFIC_RESPONSE_REFRESH_FOUR, VEHICLE_TRAFFIC_RESPONSE_REFRESH_FIVE,
  VEHICLE_TRAFFIC_RESPONSE_REFRESH_SIX,  VEHICLE_TRAFFIC_RESPONSE_REFRESH_SEVEN
};

COMPILE_TIME_SIZE_ASSERT(vehicle_traffic_lane_callback_offset,
                         offsetof(VehicleTrafficLaneState, update_callback) == 0x4C4);
COMPILE_TIME_SIZE_ASSERT(vehicle_traffic_lane_projected_extent_offset,
                         offsetof(VehicleTrafficLaneState, projected_lateral_extent) == 0x84);
COMPILE_TIME_SIZE_ASSERT(vehicle_traffic_lane_sound_variant_offset,
                         offsetof(VehicleTrafficLaneState, spatial_sound_variant) == 0x4AC);
COMPILE_TIME_SIZE_ASSERT(vehicle_traffic_lane_slot_offset,
                         offsetof(VehicleTrafficLaneState, runtime_slot_index) == 0x4C8);
COMPILE_TIME_SIZE_ASSERT(vehicle_traffic_lane_flags_offset,
                         offsetof(VehicleTrafficLaneState, runtime_flags) == 0x4CC);
COMPILE_TIME_SIZE_ASSERT(vehicle_traffic_projection_extent_offset,
                         offsetof(VehicleTrafficProjectionExtent, axis_extents) == 0x74);
COMPILE_TIME_SIZE_ASSERT(vehicle_traffic_coordinator_statistics_lane_offset,
                         offsetof(VehicleTrafficCoordinatorStatistics, active_lane_index) == 0x40);
COMPILE_TIME_SIZE_ASSERT(vehicle_traffic_coordinator_reference_transition_offset,
                         offsetof(VehicleTrafficCoordinatorReference, lane_transition_scale) ==
                           0x90);
COMPILE_TIME_SIZE_ASSERT(vehicle_traffic_response_threshold_offset,
                         offsetof(VehicleTrafficResponseState, speed_thresholds) == 0x31C);
COMPILE_TIME_SIZE_ASSERT(vehicle_traffic_response_scale_offset,
                         offsetof(VehicleTrafficResponseState, response_scales) == 0x334);
COMPILE_TIME_SIZE_ASSERT(vehicle_traffic_response_maximum_offset,
                         offsetof(VehicleTrafficResponseState, maximum_response) == 0x34C);
COMPILE_TIME_SIZE_ASSERT(vehicle_traffic_response_smoothed_offset,
                         offsetof(VehicleTrafficResponseState, smoothed_response) == 0x408);

void
advance_vehicle_track_feature_event(void)
{
  const ReferenceDistanceRecord     *feature_record;
  VehicleTrackFeatureReferenceState *reference_state;
  VehicleTrafficLaneState           *feature_runtime;
  VehicleSimulationState            *active_vehicle;
  VehicleSimulationState            *feature_vehicle;
  SimulationSource                  *active_source;
  SimulationSource                  *feature_source;
  uint32                             product;
  uint32                             probability_index;
  uint32                             track_feature_flags;
  int32                              current_segment;
  int32                              feature_segment;
  int32                              lateral_position;
  int32                              placement_mode;
  int32                              random_value;

  if(g_VEHICLE_SPAWN_CONFIGURATION.maximum_active_vehicle_count == 0)
    return;

  active_vehicle  = (VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE;
  active_source   = &active_vehicle->simulation_source;
  current_segment = active_source->track_segment_index;
  if(current_segment > g_GAME_RUNTIME_STATE.track_segment_count)
    clear_vehicle_simulation_contact_flags(1);

  if(current_segment <= g_VEHICLE_LAST_TRACK_FEATURE_SEGMENT)
    return;

  feature_vehicle = g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_SIX_TARGET];
  feature_runtime = (VehicleTrafficLaneState *) feature_vehicle;
  if((feature_runtime->runtime_flags & VEHICLE_TRACK_FEATURE_ACTIVE_FLAG) == 0)
    {
      feature_segment = g_VEHICLE_LAST_TRACK_FEATURE_SEGMENT + 1;
      while(feature_segment <= current_segment)
        {
          if((feature_segment + VEHICLE_TRACK_FEATURE_SEGMENT_OFFSET) <
             (G_TRACK_DATA->section_count * VEHICLE_TRACK_FEATURE_SEGMENTS_PER_SECTION))
            {
              feature_record = &g_REFERENCE_DISTANCE_RECORDS[feature_segment +
                                                             VEHICLE_TRACK_FEATURE_SEGMENT_OFFSET];
              track_feature_flags = ((uint32) feature_record->traffic_event_flags_high
                                     << VEHICLE_TRAFFIC_EVENT_FLAG_HIGH_SHIFT) |
                                    (uint32) feature_record->traffic_event_flags_low;
              if((track_feature_flags & VEHICLE_TRACK_FEATURE_PRESENT_FLAG) != 0)
                {
                  product = g_VEHICLE_SPAWN_RANDOM_STATE.multiplier *
                            g_VEHICLE_SPAWN_RANDOM_STATE.low_word;
                  g_VEHICLE_SPAWN_RANDOM_STATE.product = product;
                  g_VEHICLE_SPAWN_RANDOM_STATE.low_word =
                    product & VEHICLE_TRAFFIC_EVENT_RANDOM_WORD_MASK;
                  random_value = (int32) ((product >> VEHICLE_TRACK_FEATURE_RANDOM_SHIFT) &
                                          VEHICLE_TRACK_FEATURE_RANDOM_MASK);

                  track_feature_flags = ((uint32) feature_record->traffic_event_flags_high
                                         << VEHICLE_TRAFFIC_EVENT_FLAG_HIGH_SHIFT) |
                                        (uint32) feature_record->traffic_event_flags_low;
                  probability_index =
                    (track_feature_flags & VEHICLE_TRACK_FEATURE_TABLE_INDEX_MASK) >>
                    VEHICLE_TRACK_FEATURE_TABLE_INDEX_SHIFT;
                  if(random_value > g_VEHICLE_TRACK_FEATURE_PROBABILITIES[probability_index])
                    {
                      g_VEHICLE_LAST_TRACK_FEATURE_SEGMENT = current_segment;
                      return;
                    }

                  feature_source                     = &feature_vehicle->simulation_source;
                  feature_source->lane_change_active = 1;
                  feature_source->status_flags &= ~VEHICLE_TRACK_FEATURE_STATUS_PENDING_FLAG;
                  reference_state =
                    (VehicleTrackFeatureReferenceState *) feature_source->refresh_state;
                  track_feature_flags = ((uint32) feature_record->traffic_event_flags_high
                                         << VEHICLE_TRAFFIC_EVENT_FLAG_HIGH_SHIFT) |
                                        (uint32) feature_record->traffic_event_flags_low;
                  if((track_feature_flags & VEHICLE_TRACK_FEATURE_DIRECTION_FLAG) == 0)
                    {
                      placement_mode   = VEHICLE_TRACK_FEATURE_PLACEMENT_POSITIVE;
                      lateral_position = ((int32) feature_record->positive_road_edge_width *
                                          VEHICLE_TRACK_FEATURE_EDGE_WIDTH_SCALE) -
                                         reference_state->lateral_reference;
                    }
                  else
                    {
                      placement_mode   = VEHICLE_TRACK_FEATURE_PLACEMENT_NEGATIVE;
                      lateral_position = reference_state->lateral_reference -
                                         ((int32) feature_record->negative_road_edge_width *
                                          VEHICLE_TRACK_FEATURE_EDGE_WIDTH_SCALE);
                    }

                  initialize_vehicle_simulation_position(feature_source,
                                                         feature_segment +
                                                           VEHICLE_TRACK_FEATURE_SEGMENT_OFFSET,
                                                         placement_mode,
                                                         lateral_position,
                                                         VEHICLE_TRACK_FEATURE_ACTIVATE_RECORD);
                  feature_runtime->runtime_flags |= VEHICLE_TRACK_FEATURE_ACTIVE_FLAG;

                  track_feature_flags = ((uint32) feature_record->traffic_event_flags_high
                                         << VEHICLE_TRAFFIC_EVENT_FLAG_HIGH_SHIFT) |
                                        (uint32) feature_record->traffic_event_flags_low;
                  if((track_feature_flags & VEHICLE_TRACK_FEATURE_DISABLE_LIMIT_FLAG) == 0)
                    {
                      feature_source->status_flags |= VEHICLE_TRACK_FEATURE_SPEED_HOLD_FLAG;
                    }
                  break;
                }
            }
          feature_segment++;
        }
    }

  g_VEHICLE_LAST_TRACK_FEATURE_SEGMENT = current_segment;
}

void
update_vehicle_overtake_statistics(void)
{
  const TrackSegment             *segment_record;
  VehicleDriveStatisticsRecord  *statistics;
  VehicleSimulationState        *active_vehicle;
  VehicleSimulationState        *racer;
  SimulationSource              *racer_source;
  uint32                         lane_boundaries;
  int32                          active_segment;
  int32                          negative_lane_boundary;
  int32                          positive_lane_boundary;
  int32                          racer_slot;

  active_vehicle   = (VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE;
  active_segment   = active_vehicle->simulation_source.track_segment_index;
  statistics       = (VehicleDriveStatisticsRecord *) g_VEHICLE_GEAR_SHIFT_STATISTICS;

  for(racer_slot = VEHICLE_OVERTAKE_FIRST_RACER_SLOT;
      racer_slot <= VEHICLE_OVERTAKE_LAST_RACER_SLOT;
      ++racer_slot)
    {
      racer        = g_VEHICLE_SIMULATION_STATES[racer_slot];
      racer_source = &racer->simulation_source;
      if((racer_source->update_callback == update_traffic_vehicle_simulation) ||
         ((racer_source->status_flags & VEHICLE_TRAFFIC_LANE_REVERSE_FLAG) != 0))
        {
          continue;
        }
      if((racer_slot == VEHICLE_OVERTAKE_FIRST_RACER_SLOT) &&
         (g_OPPONENT_VEHICLE_INDEX == VEHICLE_SIMULATION_DISABLED_MODE))
        {
          continue;
        }

      if(racer == (VehicleSimulationState *) G_ACTIVE_VEHICLE_RUNTIME_RECORD)
        {
          g_VEHICLE_DRIVE_STATISTICS.summary.tracked_racer_ahead =
            (uint32) (racer_source->lane_change_active == VEHICLE_OVERTAKE_INACTIVE_MARKER);
        }

      if(racer_source->lane_change_active == VEHICLE_OVERTAKE_INACTIVE_MARKER)
        {
          if(racer_source->track_segment_index <= active_segment)
            continue;

          racer_source->lane_change_active = VEHICLE_OVERTAKE_ACTIVE_MARKER;
          if(racer == (VehicleSimulationState *) G_ACTIVE_VEHICLE_RUNTIME_RECORD)
            {
              statistics->tracked_racer_encounter_count++;
              request_sound_effect(VEHICLE_OVERTAKE_SOUND_ID);
            }
        }

      if(racer_source->lane_change_active != VEHICLE_OVERTAKE_ACTIVE_MARKER)
        continue;
      if(racer_source->track_segment_index >= active_segment)
        continue;

      racer_source->lane_change_active = VEHICLE_OVERTAKE_INACTIVE_MARKER;
      segment_record                   = &g_TRACK_SEGMENTS[active_segment];
      lane_boundaries                  = segment_record->lane_boundary_offsets;
      negative_lane_boundary = VEHICLE_OVERTAKE_NEGATIVE_LANE_BASE -
                               (int32) (lane_boundaries >> VEHICLE_OVERTAKE_LANE_NIBBLE_SHIFT);
      positive_lane_boundary = (int32) (lane_boundaries & VEHICLE_OVERTAKE_LANE_NIBBLE_MASK) +
                               VEHICLE_OVERTAKE_POSITIVE_LANE_BASE;

      if(racer == (VehicleSimulationState *) G_ACTIVE_VEHICLE_RUNTIME_RECORD)
        {
          statistics->tracked_racer_pass_count++;
          request_sound_effect(VEHICLE_OVERTAKE_SOUND_ID);
          if((g_VEHICLE_TRAFFIC_ACTIVE_LANE_INDEX <= negative_lane_boundary) ||
             (g_VEHICLE_TRAFFIC_ACTIVE_LANE_INDEX >= positive_lane_boundary))
            {
              statistics->tracked_racer_off_road_pass_count++;
            }
        }
      else
        {
          statistics->other_racer_pass_count++;
          if((g_VEHICLE_TRAFFIC_ACTIVE_LANE_INDEX <= negative_lane_boundary) ||
             (g_VEHICLE_TRAFFIC_ACTIVE_LANE_INDEX >= positive_lane_boundary))
            {
              statistics->other_racer_off_road_pass_count++;
            }
        }
    }
}

void
update_racer_proximity_event(VehicleSimulationState *state_,
                             int32                   lane_index_,
                             Vector3                *avoidance_offset_)
{
  VehicleDriveStatisticsRecord  *statistics;
  VehicleSimulationState        *active_vehicle;
  VehicleSimulationState        *tracked_vehicle;
  SimulationSource              *active_source;
  SimulationSource              *source;
  const ReferenceDistanceRecord *segment_record;
  uint32                         status_flags;
  uint32                         lane_boundaries;
  int32                          active_segment;
  int32                          event_active;
  int32                          object_present;
  int32                          segment_gap;
  int32                          segment_index;
  int32                          speed;
  int32                          speed_limit;

  source            = &state_->simulation_source;
  active_vehicle    = (VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE;
  active_source     = &active_vehicle->simulation_source;
  active_segment    = active_source->track_segment_index;
  segment_gap       = source->track_segment_index - active_segment;
  statistics = (VehicleDriveStatisticsRecord *) g_VEHICLE_GEAR_SHIFT_STATISTICS;

  status_flags = source->status_flags;
  if((segment_gap < 0) && ((status_flags & VEHICLE_PROXIMITY_EVENT_FLAGS) != 0) &&
     ((status_flags & VEHICLE_PROXIMITY_EVENT_RECORDED_FLAG) == 0))
    {
      source->status_flags |= VEHICLE_PROXIMITY_EVENT_RECORDED_FLAG;
      if(G_FRONTEND_EXIT_REQUESTED == VEHICLE_PROXIMITY_GAME_ACTIVE)
        statistics->pending_event_count++;
    }

  if((g_VEHICLE_SPAWN_CONFIGURATION.track_position_margin <=
      g_VEHICLE_TRAFFIC_COORDINATION_COUNTER) &&
     ((g_VEHICLE_TRAFFIC_COORDINATION_COUNTER - VEHICLE_PROXIMITY_EVENT_WINDOW_SEGMENTS) <
      g_VEHICLE_SPAWN_CONFIGURATION.track_position_margin) &&
     ((source->status_flags & VEHICLE_PROXIMITY_EVENT_FLAGS) != 0) &&
     (G_FRONTEND_EXIT_REQUESTED == VEHICLE_PROXIMITY_GAME_ACTIVE))
    {
      g_VEHICLE_SNAPSHOT_EVENT_STATE++;
    }

  if(((source->status_flags & VEHICLE_PROXIMITY_PRIMARY_EVENT_FLAG) != 0) &&
     (g_RUNTIME_STATE.collision_simulation_enabled == 0))
    {
      if((source->minimum_track_speed < VEHICLE_PROXIMITY_FIXED_ONE) &&
         (active_source->speed_magnitude < VEHICLE_PROXIMITY_FIXED_ONE) &&
         ((active_source->status_flags & VEHICLE_PROXIMITY_ACTIVE_EVENT_FLAG) != 0))
        {
          statistics->unresolved_event_count = statistics->pending_event_count -
                                               statistics->resolved_event_count -
                                               statistics->expired_event_count;

          if((statistics->resolved_event_count <
              (uint32) (g_VEHICLE_SPAWN_CONFIGURATION.placement_attempt_count -
                        VEHICLE_PROXIMITY_ATTEMPT_LIMIT_ADJUSTMENT)) ||
             (g_VEHICLE_TRAFFIC_COORDINATION_COUNTER <
              g_VEHICLE_SPAWN_CONFIGURATION.track_position_margin) ||
             (G_FRONTEND_EXIT_REQUESTED != VEHICLE_PROXIMITY_GAME_ACTIVE))
            {
              if(g_VEHICLE_TRAFFIC_COORDINATION_COUNTER <
                 g_VEHICLE_SPAWN_CONFIGURATION.track_position_margin)
                {
                  if(G_FRONTEND_EXIT_REQUESTED == VEHICLE_PROXIMITY_GAME_ACTIVE)
                    statistics->expired_event_count++;
                  activate_driving_warning_display();
                  install_driving_warning_display_callback(
                    (WarningDisplayCallbackState *) active_source,
                    g_VEHICLE_SPAWN_CONFIGURATION.minimum_refresh_delay);
                }
              else
                {
                  if(G_FRONTEND_EXIT_REQUESTED == VEHICLE_PROXIMITY_GAME_ACTIVE)
                    statistics->resolved_event_count++;
                  activate_speeding_warning_display();
                  install_driving_warning_display_callback(
                    (WarningDisplayCallbackState *) active_source,
                    g_VEHICLE_SPAWN_CONFIGURATION.maximum_refresh_delay);
                }
            }
          else
            {
              g_VEHICLE_DRIVE_STATISTICS.summary.event_track_segment =
                (uint32) g_VEHICLE_TRAFFIC_COORDINATION_COUNTER;
              g_GAME_RUNTIME_STATE.flags_14 = VEHICLE_PROXIMITY_GAME_EVENT_FLAG;
              g_GAME_RUNTIME_STATE.flags_18 = VEHICLE_PROXIMITY_GAME_EVENT_FLAG;
              statistics->resolved_event_count++;
              statistics->primary_weight =
                G_GAME_FRAME_COUNTER - g_GAME_RUNTIME_STATE.race_start_frame;
              statistics->shared_value =
                (uint32) (active_segment * VEHICLE_PROXIMITY_TRACK_POSITION_SCALE);
              if(statistics->secondary_weight == 0)
                {
                  tracked_vehicle = (VehicleSimulationState *) G_ACTIVE_VEHICLE_RUNTIME_RECORD;
                  statistics->secondary_value =
                    (uint32) (tracked_vehicle->simulation_source.track_segment_index *
                              VEHICLE_PROXIMITY_TRACK_POSITION_SCALE);
                }
              update_vehicle_drive_statistics_derived_values(statistics);
            }

          source->status_flags &= ~VEHICLE_PROXIMITY_EVENT_CLEAR_FLAGS;
          active_source->status_flags &= ~VEHICLE_PROXIMITY_ACTIVE_EVENT_FLAG;
          g_VEHICLE_COLLISION_UPDATE_PENDING         = 0;
          g_VEHICLE_TRAFFIC_COORDINATION_COUNTER     = 0;
          reset_vehicle_collision_simulation();
          reposition_vehicle_at_track_boundary(active_source, lane_index_ >= 0);
        }
      else
        {
          event_active = 0;
          if(segment_gap >= VEHICLE_PROXIMITY_ACTIVE_SEGMENT_LIMIT)
            {
              event_active = 1;
            }
          else if(segment_gap >= 0)
            {
              speed = active_source->speed_magnitude;
              if(speed < VEHICLE_PROXIMITY_SPEED_ACTIVATION_LIMIT)
                speed = source->minimum_track_speed;
              if(speed < VEHICLE_PROXIMITY_SPEED_ACTIVATION_LIMIT)
                event_active = 1;
            }
          if(event_active != 0)
            {
              active_source->status_flags |= VEHICLE_PROXIMITY_ACTIVE_EVENT_FLAG;
            }
        }
    }

  if(((source->status_flags & VEHICLE_PROXIMITY_LANE_AVOIDANCE_FLAG) != 0) ||
     ((active_source->status_flags & VEHICLE_PROXIMITY_ACTIVE_EVENT_FLAG) != 0))
    {
      segment_index  = source->track_segment_index;
      object_present = 0;
      if((seek_road_object_record(segment_index) != 0) ||
         (seek_road_object_record(segment_index + 1) != 0) ||
         (seek_road_object_record(segment_index + VEHICLE_PROXIMITY_LANE_SCALE) != 0))
        {
          object_present = 1;
        }

      segment_record  = &g_REFERENCE_DISTANCE_RECORDS[segment_index];
      lane_boundaries = segment_record->lane_boundary_offsets;
      if(((lane_boundaries & VEHICLE_PROXIMITY_NEGATIVE_LANE_MASK) != 0) ||
         ((int32) (lane_boundaries & VEHICLE_OVERTAKE_LANE_NIBBLE_MASK) <=
          (lane_index_ * VEHICLE_PROXIMITY_LANE_SCALE)) ||
         (object_present != 0))
        {
          if(object_present == 0)
            {
              avoidance_offset_->x -= VEHICLE_PROXIMITY_AVOIDANCE_OFFSET;
              avoidance_offset_->z += VEHICLE_PROXIMITY_AVOIDANCE_OFFSET;
            }
        }
      else
        {
          avoidance_offset_->x += VEHICLE_PROXIMITY_AVOIDANCE_OFFSET;
          if(((lane_index_ * VEHICLE_PROXIMITY_LANE_SCALE) + 1) ==
             (int32) (lane_boundaries & VEHICLE_OVERTAKE_LANE_NIBBLE_MASK))
            {
              avoidance_offset_->z += VEHICLE_PROXIMITY_AVOIDANCE_OFFSET;
            }
          else
            {
              avoidance_offset_->z -= VEHICLE_PROXIMITY_AVOIDANCE_OFFSET;
            }
        }
    }

  if(segment_gap < 0)
    segment_gap = -segment_gap;

  if((source->status_flags & VEHICLE_PROXIMITY_EVENT_FLAGS) != 0)
    active_source->status_flags |= VEHICLE_PROXIMITY_PRIMARY_EVENT_FLAG;

  if((segment_gap < VEHICLE_PROXIMITY_NEAR_SEGMENT_LIMIT) &&
     ((source->status_flags & VEHICLE_PROXIMITY_LANE_AVOIDANCE_FLAG) == 0))
    {
      active_source->status_flags |= VEHICLE_PROXIMITY_NEARBY_FLAG;
      if((segment_gap < VEHICLE_PROXIMITY_TRIGGER_SEGMENT_LIMIT) &&
         (g_VEHICLE_COLLISION_UPDATE_PENDING == 0))
        {
          speed_limit =
            (int32) g_TRACK_RUNTIME_DATA[((active_segment >> VEHICLE_PROXIMITY_TRACK_GROUP_SHIFT) *
                                          VEHICLE_PROXIMITY_TRACK_GROUP_STRIDE) +
                                         VEHICLE_PROXIMITY_TRACK_SPEED_OFFSET] *
            VEHICLE_PROXIMITY_FIXED_ONE;
          if(active_source->minimum_track_speed <= speed_limit)
            goto finalize_statistics;
          g_VEHICLE_TRAFFIC_COORDINATION_COUNTER = 0;
          g_VEHICLE_COLLISION_UPDATE_PENDING     = 1;
        }
      else if(g_VEHICLE_COLLISION_UPDATE_PENDING == 0)
        {
          goto finalize_statistics;
        }

      if((source->track_segment_index < active_segment) &&
         ((source->status_flags & VEHICLE_PROXIMITY_SPEED_HOLD_FLAG) != 0))
        {
          source->status_flags &= ~VEHICLE_PROXIMITY_SPEED_HOLD_FLAG;
        }

      status_flags = source->status_flags;
      if((g_VEHICLE_COLLISION_UPDATE_PENDING != 0) &&
         ((status_flags & VEHICLE_PROXIMITY_PRIMARY_EVENT_FLAG) == 0))
        {
          if(((status_flags & VEHICLE_PROXIMITY_REVERSE_FLAG) == 0) &&
             (source->track_segment_index < active_segment))
            {
              source->status_flags |= VEHICLE_PROXIMITY_PRIMARY_EVENT_FLAG;
            }
          else
            {
              source->status_flags |= VEHICLE_PROXIMITY_SECONDARY_EVENT_FLAG;
            }
        }
    }

finalize_statistics:
  statistics->unresolved_event_count = statistics->pending_event_count -
                                       statistics->resolved_event_count -
                                       statistics->expired_event_count;
}

void
update_traffic_vehicle_lane_response(VehicleSimulationState *state_,
                                     int32                   lane_index_,
                                     Vector3                *avoidance_offset_)
{
  const ReferenceDistanceRecord *segment_record;
  const ReferenceDistanceRecord *previous_segment_record;
  VehicleTrafficResponseState   *response_state;
  VehicleSimulationRefreshState *active_refresh_state;
  VehicleSimulationRefreshState *refresh_state;
  VehicleDriveStatisticsRecord  *statistics;
  VehicleSimulationState        *active_vehicle;
  VehicleSimulationState        *tracked_vehicle;
  SimulationSource              *active_source;
  SimulationSource              *source;
  uint32                         product;
  uint32                         random_limit;
  uint32                         speed_threshold;
  uint32                         lane_boundaries;
  int32                          active_segment;
  int32                          current_direction;
  int32                          previous_direction;
  int32                          direction_difference;
  int32                          forward_projection;
  int32                          projected_distance;
  int32                          response_limit;
  int32                          response_target;
  int32                          response_difference;
  int32                          speed_difference;
  int32                          speed_threshold_index;
  int32                          segment_gap;
  int32                          selected_lane;

  source          = &state_->simulation_source;
  active_vehicle  = (VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE;
  active_source   = &active_vehicle->simulation_source;
  active_refresh_state = (VehicleSimulationRefreshState *) active_source->refresh_state;
  active_segment  = active_source->track_segment_index;
  response_state  = (VehicleTrafficResponseState *) state_;
  refresh_state   = (VehicleSimulationRefreshState *) source->refresh_state;
  statistics      = (VehicleDriveStatisticsRecord *) g_VEHICLE_GEAR_SHIFT_STATISTICS;

  if(g_GAME_RUNTIME_STATE.track_segment_count < source->track_segment_index)
    {
      segment_record  = &g_REFERENCE_DISTANCE_RECORDS[source->track_segment_index];
      lane_boundaries = segment_record->lane_boundary_offsets;
      selected_lane   = VEHICLE_OVERTAKE_NEGATIVE_LANE_BASE -
                        (int32) (lane_boundaries >> VEHICLE_OVERTAKE_LANE_NIBBLE_SHIFT);
      if(selected_lane != lane_index_)
        {
          selected_lane = (int32) (lane_boundaries & VEHICLE_OVERTAKE_LANE_NIBBLE_MASK) +
                          VEHICLE_OVERTAKE_POSITIVE_LANE_BASE;
          if(selected_lane != lane_index_)
            avoidance_offset_->z += VEHICLE_TRAFFIC_RESPONSE_FINISH_OFFSET;
        }
    }

  speed_threshold =
    (uint32)
      g_TRACK_RUNTIME_DATA[(source->track_segment_index >> VEHICLE_PROXIMITY_TRACK_GROUP_SHIFT) *
                           VEHICLE_PROXIMITY_TRACK_GROUP_STRIDE] *
    VEHICLE_TRAFFIC_RESPONSE_FIXED_ONE;
  if((g_GAME_RUNTIME_STATE.result != VEHICLE_TRAFFIC_RESPONSE_RESULT_A) &&
     (g_GAME_RUNTIME_STATE.result != VEHICLE_TRAFFIC_RESPONSE_RESULT_B) &&
     (statistics->secondary_maximum_candidate < (int32) speed_threshold))
    {
      statistics->secondary_maximum_candidate = (int32) speed_threshold;
    }

  speed_threshold_index = VEHICLE_TRAFFIC_RESPONSE_FIRST_SCALE_INDEX;
  do
    {
      speed_difference =
        source->minimum_track_speed - response_state->speed_thresholds[speed_threshold_index];
      if((speed_difference > 0) && (speed_difference < VEHICLE_TRAFFIC_RESPONSE_RATE_LIMIT) &&
         ((source->minimum_track_speed >> VEHICLE_TRAFFIC_RESPONSE_RANDOM_OUTPUT_SHIFT) != 0) &&
         (refresh_state->pending_state == 0))
        {
          refresh_state->pending_state = g_VEHICLE_TRAFFIC_REFRESH_MODES[g_OPPONENT_VEHICLE_INDEX];
        }

      if(response_state->speed_thresholds[speed_threshold_index] > source->minimum_track_speed)
        {
          break;
        }
      speed_threshold_index++;
    }
  while(speed_threshold_index < VEHICLE_TRAFFIC_RESPONSE_SPEED_THRESHOLD_COUNT);

  if(refresh_state->pending_state < 1)
    {
      response_target = MulSF16(source->minimum_track_speed,
                                response_state->response_scales[speed_threshold_index]);
      response_target = MulSF16(response_target, response_state->maximum_response);
      response_target = (response_target >> VEHICLE_TRAFFIC_RESPONSE_PRIMARY_TARGET_SHIFT) +
                        (response_target >> VEHICLE_TRAFFIC_RESPONSE_SECONDARY_TARGET_SHIFT);
    }
  else
    {
      response_target = VEHICLE_TRAFFIC_RESPONSE_PENDING_VALUE;
    }

  response_difference = response_target - response_state->smoothed_response;
  if(response_difference < 0)
    response_difference += VEHICLE_TRAFFIC_RESPONSE_FILTER_BIAS;
  response_state->smoothed_response += response_difference >> VEHICLE_TRAFFIC_RESPONSE_FILTER_SHIFT;
  response_limit =
    (response_state->maximum_response >> VEHICLE_TRAFFIC_RESPONSE_RANDOM_OUTPUT_SHIFT);
  if((response_limit - VEHICLE_TRAFFIC_RESPONSE_MAXIMUM_MARGIN) < response_state->smoothed_response)
    {
      response_state->smoothed_response = response_limit;
    }

  segment_record          = &g_REFERENCE_DISTANCE_RECORDS[source->track_segment_index];
  previous_segment_record = segment_record - 1;
  current_direction =
    (int32) (int16) (((uint16) segment_record
                        ->packed_track_direction[VEHICLE_TRAFFIC_RESPONSE_DIRECTION_HIGH_INDEX]
                      << VEHICLE_TRAFFIC_RESPONSE_BIG_ENDIAN_SHIFT) |
                     (uint16) segment_record
                       ->packed_track_direction[VEHICLE_TRAFFIC_RESPONSE_DIRECTION_LOW_INDEX]);
  current_direction *= VEHICLE_TRAFFIC_RESPONSE_DIRECTION_SCALE;
  if(current_direction > VEHICLE_TRAFFIC_RESPONSE_DIRECTION_HALF_TURN)
    {
      current_direction -= VEHICLE_TRAFFIC_RESPONSE_DIRECTION_FULL_TURN;
    }
  previous_direction =
    (int32) (int16) (((uint16) previous_segment_record
                        ->packed_track_direction[VEHICLE_TRAFFIC_RESPONSE_DIRECTION_HIGH_INDEX]
                      << VEHICLE_TRAFFIC_RESPONSE_BIG_ENDIAN_SHIFT) |
                     (uint16) previous_segment_record
                       ->packed_track_direction[VEHICLE_TRAFFIC_RESPONSE_DIRECTION_LOW_INDEX]);
  previous_direction *= VEHICLE_TRAFFIC_RESPONSE_DIRECTION_SCALE;
  if(previous_direction > VEHICLE_TRAFFIC_RESPONSE_DIRECTION_HALF_TURN)
    {
      previous_direction -= VEHICLE_TRAFFIC_RESPONSE_DIRECTION_FULL_TURN;
    }
  direction_difference = current_direction - previous_direction;

  selected_lane   = lane_index_ - 1;
  lane_boundaries = segment_record->lane_boundary_offsets;
  if((lane_index_ == 0) ||
     ((selected_lane != (VEHICLE_OVERTAKE_NEGATIVE_LANE_BASE -
                         (int32) (lane_boundaries >> VEHICLE_OVERTAKE_LANE_NIBBLE_SHIFT))) &&
      (selected_lane != ((int32) (lane_boundaries & VEHICLE_OVERTAKE_LANE_NIBBLE_MASK) +
                         VEHICLE_OVERTAKE_POSITIVE_LANE_BASE))))
    {
      if(direction_difference > 0)
        {
          avoidance_offset_->x +=
            direction_difference * g_VEHICLE_SPAWN_CONFIGURATION.initial_spawn_delay;
        }
      if(direction_difference < 0)
        {
          avoidance_offset_->x +=
            direction_difference * -g_VEHICLE_SPAWN_CONFIGURATION.spawn_delay_variance;
        }
    }

  if((source->track_segment_index >=
      (active_segment - VEHICLE_TRAFFIC_RESPONSE_FORWARD_RANGE_ADJUSTMENT)) &&
     (source->track_segment_index <
      (active_segment + g_VEHICLE_SPAWN_CONFIGURATION.target_traffic_count)) &&
     (source->track_segment_index > VEHICLE_TRAFFIC_RESPONSE_MINIMUM_TRACK_SEGMENT))
    {
      forward_projection = ((source->track_basis.m22 >> VEHICLE_TRAFFIC_RESPONSE_PROJECTION_SHIFT) *
                            ((active_source->position.z - source->position.z) >>
                             VEHICLE_TRAFFIC_RESPONSE_PROJECTION_SHIFT)) +
                           ((source->track_basis.m20 >> VEHICLE_TRAFFIC_RESPONSE_PROJECTION_SHIFT) *
                            ((active_source->position.x - source->position.x) >>
                             VEHICLE_TRAFFIC_RESPONSE_PROJECTION_SHIFT));
      projected_distance = -forward_projection;

      if(g_VEHICLE_TRAFFIC_ACTIVE_LANE_INDEX == lane_index_)
        {
          speed_threshold =
            MulUF16(VEHICLE_TRAFFIC_RESPONSE_UNSIGNED_SCALE_ONE, (uint32) projected_distance);
          if(active_source->speed_magnitude < 0)
            response_limit = -active_source->speed_magnitude;
          else
            response_limit = active_source->speed_magnitude;

          if((speed_threshold < (uint32) response_limit) &&
             (forward_projection != -VEHICLE_TRAFFIC_RESPONSE_PROJECTION_EXCLUSION) &&
             (projected_distance > VEHICLE_TRAFFIC_RESPONSE_MINIMUM_PROJECTION))
            {
              active_refresh_state->activation_countdown++;
              product = g_VEHICLE_SPAWN_RANDOM_STATE.multiplier *
                        g_VEHICLE_SPAWN_RANDOM_STATE.low_word;
              g_VEHICLE_SPAWN_RANDOM_STATE.product  = product;
              g_VEHICLE_SPAWN_RANDOM_STATE.low_word =
                product & VEHICLE_TRAFFIC_RESPONSE_RANDOM_MASK;
              statistics->traffic_proximity_trigger_count++;
              g_VEHICLE_RESISTANCE_REDUCTION_ENABLED = VEHICLE_TRAFFIC_RESPONSE_ACTIVE_FLAG;
            }
          else
            {
              g_VEHICLE_RESISTANCE_REDUCTION_ENABLED = VEHICLE_TRAFFIC_RESPONSE_INACTIVE_FLAG;
              active_refresh_state->activation_countdown = 0;
            }
        }

      if(g_VEHICLE_TRAFFIC_ACTIVE_LANE_INDEX == lane_index_)
        {
          response_limit = VEHICLE_TRAFFIC_RESPONSE_SAME_LANE_THRESHOLD;
        }
      else
        {
          response_limit =
            G_VEHICLE_TRAFFIC_SPACING_PRESETS[g_VEHICLE_SPAWN_CONFIGURATION_MODE] /
            g_VEHICLE_COLLISION_SCORE;
          if(response_limit == 0)
            {
              response_limit = VEHICLE_TRAFFIC_RESPONSE_WIDE_SPACING_THRESHOLD;
            }
          else if(response_limit == 1)
            {
              response_limit = VEHICLE_TRAFFIC_RESPONSE_MEDIUM_SPACING_THRESHOLD;
            }
          else
            {
              response_limit = VEHICLE_TRAFFIC_RESPONSE_NARROW_SPACING_THRESHOLD;
            }
        }

      if((projected_distance >> VEHICLE_TRAFFIC_RESPONSE_RANDOM_OUTPUT_SHIFT) < response_limit)
        {
          g_VEHICLE_COLLISION_STATISTICS_SUPPRESSED = VEHICLE_TRAFFIC_RESPONSE_INACTIVE_FLAG;
        }
      else
        {
          g_VEHICLE_COLLISION_STATISTICS_SUPPRESSED = VEHICLE_TRAFFIC_RESPONSE_ACTIVE_FLAG;
          if(g_VEHICLE_TRAFFIC_ACTIVE_LANE_INDEX < lane_index_)
            {
              avoidance_offset_->x += VEHICLE_TRAFFIC_RESPONSE_AVOIDANCE_OFFSET;
            }
          else if(g_VEHICLE_TRAFFIC_ACTIVE_LANE_INDEX == lane_index_)
            {
              avoidance_offset_->y += VEHICLE_TRAFFIC_RESPONSE_AVOIDANCE_OFFSET;
              g_VEHICLE_COLLISION_STATISTICS_SUPPRESSED =
                VEHICLE_TRAFFIC_RESPONSE_INACTIVE_FLAG;
            }
          else
            {
              avoidance_offset_->z += VEHICLE_TRAFFIC_RESPONSE_AVOIDANCE_OFFSET;
            }
        }
    }
  else
    {
      g_VEHICLE_COLLISION_STATISTICS_SUPPRESSED = VEHICLE_TRAFFIC_RESPONSE_INACTIVE_FLAG;
    }

  if((source->track_segment_index < active_segment) &&
     (g_VEHICLE_TRAFFIC_ACTIVE_LANE_INDEX == lane_index_))
    {
      segment_gap = active_segment - source->track_segment_index;
      speed_threshold =
        MulUF16(VEHICLE_TRAFFIC_RESPONSE_UNSIGNED_SCALE_TWO,
                (uint32) (segment_gap * VEHICLE_TRAFFIC_RESPONSE_TRACK_POSITION_SCALE));
      tracked_vehicle = (VehicleSimulationState *) G_ACTIVE_VEHICLE_RUNTIME_RECORD;
      response_limit  = tracked_vehicle->simulation_source.minimum_track_speed;
      if(response_limit < 0)
        response_limit = -response_limit;

      if(speed_threshold < (uint32) response_limit)
        {
          refresh_state->activation_countdown++;
          product = g_VEHICLE_SPAWN_RANDOM_STATE.multiplier *
                    g_VEHICLE_SPAWN_RANDOM_STATE.low_word;
          g_VEHICLE_SPAWN_RANDOM_STATE.product  = product;
          g_VEHICLE_SPAWN_RANDOM_STATE.low_word = product & VEHICLE_TRAFFIC_RESPONSE_RANDOM_MASK;
          random_limit = (((product & VEHICLE_TRAFFIC_RESPONSE_RANDOM_VALUE_MASK) >>
                           VEHICLE_TRAFFIC_RESPONSE_RANDOM_VALUE_SHIFT) *
                          VEHICLE_TRAFFIC_RESPONSE_RANDOM_SCALE) +
                         VEHICLE_TRAFFIC_RESPONSE_RANDOM_BIAS;
          random_limit >>= VEHICLE_TRAFFIC_RESPONSE_RANDOM_OUTPUT_SHIFT;
          if(random_limit < (uint32) refresh_state->activation_countdown)
            {
              request_traffic_vehicle_spatial_sound(
                ((VehicleTrafficLaneState *) state_)->runtime_slot_index,
                VEHICLE_TRAFFIC_RESPONSE_SOUND_VARIANT);
              refresh_state->activation_countdown = 0;
            }
        }
    }
  else
    {
      refresh_state->activation_countdown = 0;
    }

  segment_gap = active_segment - source->track_segment_index;
  if((segment_gap != VEHICLE_TRAFFIC_RESPONSE_SOUND_GAP_ONE) &&
     (segment_gap != VEHICLE_TRAFFIC_RESPONSE_SOUND_GAP_TWO))
    {
      return;
    }
  if(source->minimum_track_speed <= VEHICLE_TRAFFIC_RESPONSE_SOUND_SPEED_LIMIT)
    {
      return;
    }
  if(active_source->update_callback !=
     (SimulationUpdateCallback) complete_driving_warning_display_callback)
    {
      return;
    }

  request_traffic_vehicle_spatial_sound(((VehicleTrafficLaneState *) state_)->runtime_slot_index,
                                        VEHICLE_TRAFFIC_RESPONSE_SOUND_VARIANT);
}

void
trigger_vehicle_track_collision_event(VehicleSimulationState *state_)
{
  const VehicleSimulationState   *active_vehicle;
  const ReferenceDistanceRecord  *track_record;
  SimulationSource               *source;
  MatrixControllerState          *transform;
  uint32                          product;
  uint32                          probability_index;
  int32                           random_value;
  uint32                          track_event_flags;
  Vector3                         alternate_impulse;
  Vector3                         impulse;
  int32                           segment_distance;
  int32                           track_speed;

  source      = &state_->simulation_source;
  track_speed = calculate_vehicle_track_speed(source);
  if(source->minimum_track_speed <
     (track_speed - (track_speed >> VEHICLE_TRAFFIC_EVENT_SPEED_MARGIN_SHIFT)))
    {
      return;
    }

  product = g_VEHICLE_SPAWN_RANDOM_STATE.multiplier * g_VEHICLE_SPAWN_RANDOM_STATE.low_word;
  g_VEHICLE_SPAWN_RANDOM_STATE.product  = product;
  g_VEHICLE_SPAWN_RANDOM_STATE.low_word = product & VEHICLE_TRAFFIC_EVENT_RANDOM_WORD_MASK;
  random_value           = (int32) ((product >> VEHICLE_TRAFFIC_EVENT_PROBABILITY_SHIFT) &
                                    VEHICLE_TRAFFIC_EVENT_RANDOM_VALUE_MASK);

  track_record = &g_REFERENCE_DISTANCE_RECORDS[source->track_segment_index];
  track_event_flags =
    ((uint32) track_record->traffic_event_flags_high << VEHICLE_TRAFFIC_EVENT_FLAG_HIGH_SHIFT) |
    (uint32) track_record->traffic_event_flags_low;
  probability_index = (track_event_flags & VEHICLE_TRAFFIC_EVENT_TABLE_INDEX_MASK) >>
                      VEHICLE_TRAFFIC_EVENT_TABLE_INDEX_SHIFT;
  if(random_value >= g_VEHICLE_TRAFFIC_EVENT_PROBABILITIES[probability_index])
    {
      return;
    }

  active_vehicle = (const VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE;
  segment_distance =
    source->track_segment_index - active_vehicle->simulation_source.track_segment_index;
  if((source->status_flags & VEHICLE_TRAFFIC_EVENT_STATUS_TRAFFIC_FLAG) == 0)
    {
      if((segment_distance >= VEHICLE_TRAFFIC_EVENT_SOUND_MINIMUM_DISTANCE) &&
         (segment_distance < VEHICLE_TRAFFIC_EVENT_SOUND_MAXIMUM_DISTANCE))
        {
          request_sound_effect(VEHICLE_TRAFFIC_EVENT_SOUND_ID);
        }
    }
  else
    {
      if((segment_distance >= VEHICLE_TRAFFIC_EVENT_SOUND_MINIMUM_DISTANCE) &&
         (segment_distance < VEHICLE_TRAFFIC_EVENT_SOUND_MAXIMUM_DISTANCE))
        {
          request_sound_effect(VEHICLE_TRAFFIC_EVENT_SOUND_ID);
        }
      clear_vehicle_simulation_contact_flags(0);
    }

  prepare_simulation_collision_transform((SimulationSource *) state_);
  transform           = (MatrixControllerState *) source->refresh_state;
  impulse.x           = transform->matrix.m10 * VEHICLE_TRAFFIC_EVENT_IMPULSE_SCALE;
  impulse.y           = transform->matrix.m11 * VEHICLE_TRAFFIC_EVENT_IMPULSE_SCALE;
  impulse.z           = transform->matrix.m12 * VEHICLE_TRAFFIC_EVENT_IMPULSE_SCALE;
  alternate_impulse.x = transform->matrix.m20 * VEHICLE_TRAFFIC_EVENT_IMPULSE_SCALE;
  alternate_impulse.y = transform->matrix.m21 * VEHICLE_TRAFFIC_EVENT_IMPULSE_SCALE;
  alternate_impulse.z = transform->matrix.m22 * VEHICLE_TRAFFIC_EVENT_IMPULSE_SCALE;

  product = g_VEHICLE_SPAWN_RANDOM_STATE.multiplier * g_VEHICLE_SPAWN_RANDOM_STATE.low_word;
  g_VEHICLE_SPAWN_RANDOM_STATE.product  = product;
  g_VEHICLE_SPAWN_RANDOM_STATE.low_word = product & VEHICLE_TRAFFIC_EVENT_RANDOM_WORD_MASK;
  random_value           = (int32) ((product >> VEHICLE_TRAFFIC_EVENT_IMPULSE_RANDOM_SHIFT) &
                                    VEHICLE_TRAFFIC_EVENT_RANDOM_VALUE_MASK);
  track_event_flags =
    ((uint32) track_record->traffic_event_flags_high << VEHICLE_TRAFFIC_EVENT_FLAG_HIGH_SHIFT) |
    (uint32) track_record->traffic_event_flags_low;
  if((track_event_flags & VEHICLE_TRAFFIC_EVENT_DIRECTION_FLAG) == 0)
    {
      impulse.x           = -impulse.x;
      impulse.y           = -impulse.y;
      impulse.z           = -impulse.z;
      alternate_impulse.x = -alternate_impulse.x;
      alternate_impulse.y = -alternate_impulse.y;
      alternate_impulse.z = -alternate_impulse.z;
    }

  if(random_value < VEHICLE_TRAFFIC_EVENT_PRIMARY_IMPULSE_LIMIT)
    {
      transform->angular_velocity.x += impulse.x;
      transform->angular_velocity.y += impulse.y;
      transform->angular_velocity.z += impulse.z;
    }
  else if(random_value > VEHICLE_TRAFFIC_EVENT_ALTERNATE_IMPULSE_MINIMUM)
    {
      if(random_value < VEHICLE_TRAFFIC_EVENT_RANDOM_RANGE_LIMIT)
        {
          transform->angular_velocity.x += alternate_impulse.x;
          transform->angular_velocity.y += alternate_impulse.y;
          transform->angular_velocity.z += alternate_impulse.z;
        }
      else if(random_value < VEHICLE_TRAFFIC_EVENT_RANDOM_RANGE_LIMIT)
        {
          transform->angular_velocity.x += alternate_impulse.x;
          transform->angular_velocity.y += alternate_impulse.y;
          transform->angular_velocity.z += alternate_impulse.z;
          transform->angular_velocity.x -= impulse.x;
          transform->angular_velocity.y -= impulse.y;
          transform->angular_velocity.z -= impulse.z;
        }
    }
}

void
collect_traffic_lane_neighbor_limits(
  VehicleSimulationState *state_,
  const DoublyLinkedList *vehicle_list_,
  const uint32            vehicle_lane_masks_[VEHICLE_SIMULATION_STATE_COUNT],
  int32                   current_lane_,
  uint32                 *matched_lane_mask_,
  int32                   primary_speed_limits_[VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT],
  int32                   secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT],
  VehicleSimulationState *lane_neighbors_[VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT])
{
  const VehicleTrafficLaneState *current;
  const VehicleTrafficLaneState *neighbor;
  const VehicleSimulationState  *neighbor_vehicle;
  const DoublyLinkedNode        *node;
  SimulationSource              *source;
  const SimulationSource        *neighbor_source;
  uint32                         center_lane_mask;
  uint32                         left_lane_mask;
  uint32                         right_lane_mask;
  int32                          found_lane_count;
  int32                          lane_index;
  int32                          lateral_limit;
  int32                          neighbor_index;
  int32                          primary_limit;
  int32                          relative_speed;
  int32                          secondary_limit;
  int32                          segment_distance;
  int32                          segment_limit;

  current          = (const VehicleTrafficLaneState *) state_;
  source           = &state_->simulation_source;
  center_lane_mask = (uint32) 1 << current_lane_;
  left_lane_mask   = center_lane_mask >> 1;
  right_lane_mask  = center_lane_mask << 1;

  if((source->status_flags & VEHICLE_TRAFFIC_LANE_REVERSE_FLAG) == 0)
    {
      node             = vehicle_list_->head_sentinel->next;
      neighbor_index   = 0;
      found_lane_count = 0;
      while((node->next != NULL) && (found_lane_count != VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT))
        {
          neighbor         = (const VehicleTrafficLaneState *) node;
          neighbor_vehicle = (const VehicleSimulationState *) neighbor;
          if((neighbor != current) &&
             ((neighbor->runtime_flags & VEHICLE_TRAFFIC_LANE_RUNTIME_ACTIVE_FLAG) != 0) &&
             (neighbor->update_callback !=
              (SimulationUpdateCallback) update_traffic_vehicle_simulation))
            {
              neighbor_source  = &neighbor_vehicle->simulation_source;
              segment_distance = neighbor_source->track_segment_index - source->track_segment_index;
              segment_limit    = VEHICLE_TRAFFIC_LANE_DEFAULT_SEARCH_DISTANCE;
              if(((source->status_flags & VEHICLE_TRAFFIC_LANE_PLAYER_FLAG) != 0) &&
                 ((neighbor_source->status_flags & VEHICLE_TRAFFIC_LANE_REVERSE_FLAG) != 0))
                {
                  segment_limit = g_VEHICLE_SPAWN_CONFIGURATION.minimum_spawn_spacing;
                }

              if((segment_distance > 0) && (segment_distance < segment_limit))
                {
                  segment_distance -= VEHICLE_TRAFFIC_LANE_SEGMENT_GAP_ADJUSTMENT;
                  if(((source->status_flags & VEHICLE_TRAFFIC_LANE_PLAYER_OVERRIDE_MASK) != 0) &&
                     (segment_distance < 1))
                    {
                      segment_distance = 1;
                    }

                  if(((vehicle_lane_masks_[neighbor_index] & center_lane_mask) != 0) &&
                     ((*matched_lane_mask_ & center_lane_mask) == 0))
                    {
                      secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX] =
                        (segment_distance * VEHICLE_TRAFFIC_LANE_SECONDARY_SEGMENT_SCALE) +
                        neighbor_source->minimum_track_speed;
                      primary_speed_limits_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX] =
                        neighbor_source->minimum_track_speed +
                        (segment_distance * VEHICLE_TRAFFIC_LANE_PRIMARY_SEGMENT_SCALE);
                      *matched_lane_mask_ |= center_lane_mask;
                      found_lane_count++;
                      lane_neighbors_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX] =
                        (VehicleSimulationState *) neighbor;
                    }

                  if((current_lane_ > 0) &&
                     ((vehicle_lane_masks_[neighbor_index] & left_lane_mask) != 0) &&
                     ((*matched_lane_mask_ & left_lane_mask) == 0))
                    {
                      secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_LEFT_SPEED_INDEX] =
                        (segment_distance * VEHICLE_TRAFFIC_LANE_SECONDARY_SEGMENT_SCALE) +
                        neighbor_source->minimum_track_speed;
                      primary_speed_limits_[VEHICLE_TRAFFIC_LANE_LEFT_SPEED_INDEX] =
                        neighbor_source->minimum_track_speed +
                        (segment_distance * VEHICLE_TRAFFIC_LANE_PRIMARY_SEGMENT_SCALE);
                      *matched_lane_mask_ |= left_lane_mask;
                      found_lane_count++;
                      lane_neighbors_[VEHICLE_TRAFFIC_LANE_LEFT_SPEED_INDEX] =
                        (VehicleSimulationState *) neighbor;
                    }

                  if((current_lane_ < VEHICLE_TRAFFIC_LANE_MAXIMUM_INDEX) &&
                     ((vehicle_lane_masks_[neighbor_index] & right_lane_mask) != 0) &&
                     ((*matched_lane_mask_ & right_lane_mask) == 0))
                    {
                      secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_RIGHT_SPEED_INDEX] =
                        (segment_distance * VEHICLE_TRAFFIC_LANE_SECONDARY_SEGMENT_SCALE) +
                        neighbor_source->minimum_track_speed;
                      primary_speed_limits_[VEHICLE_TRAFFIC_LANE_RIGHT_SPEED_INDEX] =
                        neighbor_source->minimum_track_speed +
                        (segment_distance * VEHICLE_TRAFFIC_LANE_PRIMARY_SEGMENT_SCALE);
                      *matched_lane_mask_ |= right_lane_mask;
                      found_lane_count++;
                      lane_neighbors_[VEHICLE_TRAFFIC_LANE_RIGHT_SPEED_INDEX] =
                        (VehicleSimulationState *) neighbor;
                    }
                }
            }

          node = node->next;
          neighbor_index++;
        }

      neighbor_vehicle = lane_neighbors_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX];
      neighbor         = (const VehicleTrafficLaneState *) neighbor_vehicle;
      if((current->runtime_slot_index > VEHICLE_TRAFFIC_LANE_PLAYER_SLOT_MINIMUM - 1) &&
         (neighbor != NULL) &&
         ((neighbor_vehicle->simulation_source.track_segment_index - source->track_segment_index) <
          VEHICLE_TRAFFIC_LANE_CLOSE_SEGMENT_DISTANCE))
        {
          primary_speed_limits_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX] >>= 1;
          secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX] >>= 1;
          if(source->target_track_speed <= neighbor_vehicle->simulation_source.minimum_track_speed)
            {
              primary_speed_limits_[VEHICLE_TRAFFIC_LANE_LEFT_SPEED_INDEX] >>= 1;
              secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_LEFT_SPEED_INDEX] >>= 1;
              primary_speed_limits_[VEHICLE_TRAFFIC_LANE_RIGHT_SPEED_INDEX] >>= 1;
              secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_RIGHT_SPEED_INDEX] >>= 1;
            }
        }

      lane_index =
        (int32) g_REFERENCE_DISTANCE_RECORDS[source->track_segment_index].lane_boundary_offsets;
      if((current_lane_ + 1) == ((lane_index & VEHICLE_TRAFFIC_LANE_BOUNDARY_NIBBLE_MASK) +
                                 VEHICLE_TRAFFIC_LANE_POSITIVE_BOUNDARY_BASE))
        {
          primary_speed_limits_[VEHICLE_TRAFFIC_LANE_RIGHT_SPEED_INDEX] >>= 1;
          secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_RIGHT_SPEED_INDEX] >>= 1;
        }
      else if((current_lane_ - 1) == (VEHICLE_TRAFFIC_LANE_NEGATIVE_BOUNDARY_BASE -
                                      (lane_index >> VEHICLE_TRAFFIC_LANE_NEGATIVE_BOUNDARY_SHIFT)))
        {
          primary_speed_limits_[VEHICLE_TRAFFIC_LANE_LEFT_SPEED_INDEX] >>= 1;
          secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_LEFT_SPEED_INDEX] >>= 1;
        }
    }
  else
    {
      node             = vehicle_list_->tail_sentinel->previous;
      neighbor_index   = VEHICLE_SIMULATION_STATE_COUNT - 1;
      found_lane_count = 0;
      while((node->previous != NULL) &&
            (found_lane_count != VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT))
        {
          neighbor         = (const VehicleTrafficLaneState *) node;
          neighbor_vehicle = (const VehicleSimulationState *) neighbor;
          if((neighbor != current) &&
             ((neighbor->runtime_flags & VEHICLE_TRAFFIC_LANE_RUNTIME_ACTIVE_FLAG) != 0) &&
             (neighbor->update_callback !=
              (SimulationUpdateCallback) update_traffic_vehicle_simulation))
            {
              neighbor_source  = &neighbor_vehicle->simulation_source;
              segment_distance = neighbor_source->track_segment_index - source->track_segment_index;
              if((segment_distance < 1) &&
                 (-segment_distance < VEHICLE_TRAFFIC_LANE_DEFAULT_SEARCH_DISTANCE))
                {
                  segment_distance += VEHICLE_TRAFFIC_LANE_SEGMENT_GAP_ADJUSTMENT;

                  if(((vehicle_lane_masks_[neighbor_index] & center_lane_mask) != 0) &&
                     ((*matched_lane_mask_ & center_lane_mask) == 0))
                    {
                      secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX] =
                        (segment_distance * VEHICLE_TRAFFIC_LANE_SECONDARY_SEGMENT_SCALE) +
                        neighbor_source->minimum_track_speed;
                      primary_speed_limits_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX] =
                        neighbor_source->minimum_track_speed +
                        (segment_distance * VEHICLE_TRAFFIC_LANE_PRIMARY_SEGMENT_SCALE);
                      *matched_lane_mask_ |= center_lane_mask;
                      found_lane_count++;
                      lane_neighbors_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX] =
                        (VehicleSimulationState *) neighbor;
                    }

                  if((current_lane_ > 0) &&
                     ((vehicle_lane_masks_[neighbor_index] & left_lane_mask) != 0) &&
                     ((*matched_lane_mask_ & left_lane_mask) == 0))
                    {
                      secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_LEFT_SPEED_INDEX] =
                        (segment_distance * VEHICLE_TRAFFIC_LANE_SECONDARY_SEGMENT_SCALE) +
                        neighbor_source->minimum_track_speed;
                      primary_speed_limits_[VEHICLE_TRAFFIC_LANE_LEFT_SPEED_INDEX] =
                        neighbor_source->minimum_track_speed +
                        (segment_distance * VEHICLE_TRAFFIC_LANE_PRIMARY_SEGMENT_SCALE);
                      *matched_lane_mask_ |= left_lane_mask;
                      found_lane_count++;
                      lane_neighbors_[VEHICLE_TRAFFIC_LANE_LEFT_SPEED_INDEX] =
                        (VehicleSimulationState *) neighbor;
                    }

                  if((current_lane_ < VEHICLE_TRAFFIC_LANE_MAXIMUM_INDEX) &&
                     ((vehicle_lane_masks_[neighbor_index] & right_lane_mask) != 0) &&
                     ((*matched_lane_mask_ & right_lane_mask) == 0))
                    {
                      secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_RIGHT_SPEED_INDEX] =
                        (segment_distance * VEHICLE_TRAFFIC_LANE_SECONDARY_SEGMENT_SCALE) +
                        neighbor_source->minimum_track_speed;
                      primary_speed_limits_[VEHICLE_TRAFFIC_LANE_RIGHT_SPEED_INDEX] =
                        neighbor_source->minimum_track_speed +
                        (segment_distance * VEHICLE_TRAFFIC_LANE_PRIMARY_SEGMENT_SCALE);
                      *matched_lane_mask_ |= right_lane_mask;
                      found_lane_count++;
                      lane_neighbors_[VEHICLE_TRAFFIC_LANE_RIGHT_SPEED_INDEX] =
                        (VehicleSimulationState *) neighbor;
                    }
                }
            }

          node = node->previous;
          neighbor_index--;
        }

      if(current->runtime_slot_index > VEHICLE_TRAFFIC_LANE_PLAYER_SLOT_MINIMUM - 1)
        {
          neighbor_vehicle = lane_neighbors_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX];
          neighbor         = (const VehicleTrafficLaneState *) neighbor_vehicle;
          if(neighbor_vehicle == NULL)
            return;
          if((source->track_segment_index -
              neighbor_vehicle->simulation_source.track_segment_index) <
             VEHICLE_TRAFFIC_LANE_CLOSE_SEGMENT_DISTANCE)
            {
              primary_speed_limits_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX] >>= 1;
              secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX] >>= 1;
              if(neighbor_vehicle->simulation_source.minimum_track_speed <=
                 source->target_track_speed)
                {
                  primary_speed_limits_[VEHICLE_TRAFFIC_LANE_LEFT_SPEED_INDEX] >>= 1;
                  secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_LEFT_SPEED_INDEX] >>= 1;
                  primary_speed_limits_[VEHICLE_TRAFFIC_LANE_RIGHT_SPEED_INDEX] >>= 1;
                  secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_RIGHT_SPEED_INDEX] >>= 1;
                }
            }
        }
    }

  neighbor_vehicle = lane_neighbors_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX];
  neighbor         = (const VehicleTrafficLaneState *) neighbor_vehicle;
  if((neighbor_vehicle != NULL) &&
     (neighbor == (const VehicleTrafficLaneState *) G_ACTIVE_SIMULATION_SOURCE))
    {
      segment_distance =
        source->track_segment_index - neighbor_vehicle->simulation_source.track_segment_index;
      if(segment_distance < 1)
        segment_distance = -segment_distance;
      if((segment_distance > VEHICLE_TRAFFIC_LANE_PLAYER_MINIMUM_DISTANCE - 1) &&
         (segment_distance < VEHICLE_TRAFFIC_LANE_PLAYER_MAXIMUM_DISTANCE))
        {
          relative_speed =
            source->minimum_track_speed - neighbor_vehicle->simulation_source.minimum_track_speed;
          if(relative_speed < 1)
            relative_speed = -relative_speed;
          lateral_limit =
            limit_vehicle_lateral_acceleration(source, 1, source->minimum_track_speed);
          if(lateral_limit < 1)
            lateral_limit = -lateral_limit;
          primary_limit = MulSF16(VEHICLE_TRAFFIC_LANE_PLAYER_RELATIVE_SCALE, relative_speed);
          secondary_limit =
            MulSF16(segment_distance * VEHICLE_TRAFFIC_LANE_SEGMENT_DISTANCE_SCALE, lateral_limit);
          primary_limit = MulSF16(primary_limit, VEHICLE_TRAFFIC_LANE_PLAYER_THRESHOLD_SCALE);
          if(secondary_limit <= primary_limit)
            {
              primary_speed_limits_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX]   = 0;
              secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX] = 0;
            }
        }
    }
}

void
refresh_vehicle_traffic_lane_state(VehicleSimulationState *state_)
{
  VehicleSimulationState          *active_vehicle;
  VehicleTrafficLaneState         *lane_state;
  VehicleTrafficProjectionExtent  *extent;
  SimulationSource                *active_source;
  SimulationSource                *source;
  const TrackSegment               *track_record;
  int32                            axis_projection;
  int32                            delta_x;
  int32                            delta_y;
  int32                            delta_z;
  int32                            segment_difference;
  int32                            track_speed;
  int32                            value_x;
  int32                            value_y;
  int32                            value_z;
  uint32                           lateral_extent;
  uint32                           projection;
  uint32                           status_flags;

  lane_state     = (VehicleTrafficLaneState *) state_;
  source         = &state_->simulation_source;
  active_vehicle = (VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE;
  active_source  = &active_vehicle->simulation_source;
  track_record   = &g_TRACK_SEGMENTS[source->track_segment_index];

  if(source->transform_active == 0)
    {
      rebuild_vehicle_track_frame(source);
      initialize_matrix3x3_rotation_xzy(&source->track_transform,
                                        source->direction.x,
                                        source->direction.z,
                                        source->direction.y);
    }

  delta_x =
    (source->position.x - track_record->surface_reference.x) >>
    VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT;
  delta_y =
    (source->position.y - track_record->surface_reference.y) >>
    VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT;
  delta_z =
    (source->position.z - track_record->surface_reference.z) >>
    VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT;
  value_x    = source->track_basis.m00 >> VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT;
  value_y    = source->track_basis.m01 >> VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT;
  value_z    = source->track_basis.m02 >> VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT;
  projection = (uint32) delta_x * (uint32) value_x;
  projection += (uint32) delta_y * (uint32) value_y;
  projection += (uint32) delta_z * (uint32) value_z;
  source->lateral_position = (int32) projection;

  extent     = (VehicleTrafficProjectionExtent *) source->refresh_state;
  projection = (uint32) (source->track_transform.m00 >> VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT) *
               (uint32) value_x;
  projection += (uint32) (source->track_transform.m01 >> VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT) *
                (uint32) value_y;
  projection += (uint32) (source->track_transform.m02 >> VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT) *
                (uint32) value_z;
  axis_projection = (int32) projection;
  if(axis_projection < 1)
    axis_projection = (int32) (0U - (uint32) axis_projection);
  lateral_extent = (uint32) (axis_projection >> VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT) *
                   (uint32) (extent->axis_extents[VEHICLE_TRAFFIC_PROJECTION_EXTENT_X_INDEX] >>
                             VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT);

  projection = (uint32) (source->track_transform.m10 >> VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT) *
               (uint32) value_x;
  projection += (uint32) (source->track_transform.m11 >> VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT) *
                (uint32) value_y;
  projection += (uint32) (source->track_transform.m12 >> VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT) *
                (uint32) value_z;
  axis_projection = (int32) projection;
  if(axis_projection < 1)
    axis_projection = (int32) (0U - (uint32) axis_projection);
  lateral_extent += (uint32) (axis_projection >> VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT) *
                    (uint32) (extent->axis_extents[VEHICLE_TRAFFIC_PROJECTION_EXTENT_Y_INDEX] >>
                              VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT);

  projection = (uint32) (source->track_transform.m20 >> VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT) *
               (uint32) value_x;
  projection += (uint32) (source->track_transform.m21 >> VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT) *
                (uint32) value_y;
  projection += (uint32) (source->track_transform.m22 >> VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT) *
                (uint32) value_z;
  axis_projection = (int32) projection;
  if(axis_projection < 1)
    axis_projection = (int32) (0U - (uint32) axis_projection);
  lateral_extent += (uint32) (axis_projection >> VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT) *
                    (uint32) (extent->axis_extents[VEHICLE_TRAFFIC_PROJECTION_EXTENT_Z_INDEX] >>
                              VEHICLE_TRAFFIC_PROJECTION_FIXED8_SHIFT);
  lane_state->projected_lateral_extent = (int32) lateral_extent;

  if((G_FRONTEND_EXIT_REQUESTED != VEHICLE_TRAFFIC_PROJECTION_PALETTE_DISABLED_MODE) &&
     (lane_state->runtime_slot_index >= VEHICLE_TRAFFIC_PROJECTION_PALETTE_SLOT_MINIMUM) &&
     (source->art_variant != state_->render_resource_state->active_palette_shape))
    {
      select_vehicle_render_palette(state_->render_resource_state, (int32) source->art_variant);
    }

  if(state_ != active_vehicle)
    {
      status_flags = source->status_flags;
      if((status_flags & VEHICLE_TRAFFIC_PROJECTION_STATUS_REVERSE_FLAG) != 0)
        {
          source->target_track_speed =
            randomize_vehicle_track_speed(source, VEHICLE_TRAFFIC_PROJECTION_REVERSE_SPEED);
        }
      else
        {
          if((status_flags & VEHICLE_TRAFFIC_PROJECTION_STATUS_PLAYER_FLAG) == 0)
            {
              source->target_track_speed = (g_VEHICLE_SPAWN_CONFIGURATION.speed_average >> 1) +
                                           (active_source->minimum_track_speed >> 1);
              source->target_track_speed =
                randomize_vehicle_track_speed(source, source->target_track_speed);
            }
          else
            {
              source->target_track_speed =
                (g_VEHICLE_SPAWN_CONFIGURATION.maximum_spawn_spacing >> 1) +
                (active_source->minimum_track_speed >> 1);
            }

          if((status_flags & VEHICLE_TRAFFIC_PROJECTION_STATUS_OVERRIDE_MASK) == 0)
            {
              track_speed =
                (int32) ((uint32)
                           g_TRACK_RUNTIME_DATA[(source->track_segment_index >>
                                                 VEHICLE_TRAFFIC_PROJECTION_TRACK_GROUP_SHIFT) *
                                                  VEHICLE_TRAFFIC_PROJECTION_TRACK_GROUP_STRIDE +
                                                VEHICLE_TRAFFIC_PROJECTION_TRACK_SPEED_OFFSET]
                         << VEHICLE_TRAFFIC_PROJECTION_TRACK_SPEED_SHIFT);
              track_speed = randomize_vehicle_track_speed(source, track_speed);
              if(track_speed < source->target_track_speed)
                source->target_track_speed = track_speed;
            }
          else
            {
              track_speed = active_source->minimum_track_speed +
                            g_VEHICLE_SPAWN_CONFIGURATION.minimum_traffic_speed;
              if(source->target_track_speed < track_speed)
                source->target_track_speed = track_speed;

              if(((status_flags & VEHICLE_TRAFFIC_PROJECTION_STATUS_TRAFFIC_FLAG) != 0) &&
                 ((status_flags & VEHICLE_TRAFFIC_PROJECTION_STATUS_ACTIVE_FLAG) != 0))
                {
                  segment_difference =
                    source->track_segment_index - active_source->track_segment_index;
                  track_speed = (int32) ((uint32) g_TRACK_RUNTIME_DATA
                                           [(source->track_segment_index >>
                                             VEHICLE_TRAFFIC_PROJECTION_TRACK_GROUP_SHIFT) *
                                              VEHICLE_TRAFFIC_PROJECTION_TRACK_GROUP_STRIDE +
                                            VEHICLE_TRAFFIC_PROJECTION_TRACK_SPEED_OFFSET]
                                         << VEHICLE_TRAFFIC_PROJECTION_TRACK_SPEED_SHIFT);
                  if((segment_difference > VEHICLE_TRAFFIC_PROJECTION_CLOSE_SEGMENT_MAXIMUM) ||
                     ((segment_difference >= VEHICLE_TRAFFIC_PROJECTION_CLOSE_SEGMENT_MINIMUM) &&
                      (g_VEHICLE_TRAFFIC_COORDINATION_COUNTER <
                       VEHICLE_TRAFFIC_PROJECTION_EVENT_LIMIT) &&
                      (track_speed < active_source->minimum_track_speed)))
                    {
                      source->target_track_speed = active_source->speed_magnitude;
                    }
                }
            }

          track_speed = calculate_vehicle_track_speed(source);
          if((track_speed < source->target_track_speed) ||
             ((status_flags & VEHICLE_TRAFFIC_PROJECTION_STATUS_PLAYER_FLAG) != 0))
            {
              source->target_track_speed = track_speed;
            }
        }
    }

  if(lane_state->update_callback != (SimulationUpdateCallback) update_vehicle_simulation_transform)
    {
      if(lane_state->update_callback == (SimulationUpdateCallback) update_player_vehicle_state)
        {
          source->minimum_track_speed =
            calculate_vehicle_motion_projection((const VehicleMotionProjectionState *) source);
        }
      else
        {
          source->minimum_track_speed = calculate_negative_x_vector_dot_product_fixed8(
            (const VehicleVectorProjectionState *) source);
        }
    }

  if((source->status_flags & VEHICLE_TRAFFIC_PROJECTION_STATUS_SPEED_HOLD_FLAG) != 0)
    {
      source->target_track_speed = 0;
    }
  source->status_flags = (source->status_flags & ~VEHICLE_TRAFFIC_PROJECTION_STATUS_CLEAR_MASK) |
                         VEHICLE_TRAFFIC_PROJECTION_STATUS_DEFAULT_FLAGS;
}

void
coordinate_vehicle_traffic_lanes(const DoublyLinkedList *vehicle_list_)
{
  VehicleSimulationState              *active_vehicle;
  VehicleSimulationState              *lane_neighbors[VEHICLE_TRAFFIC_COORDINATOR_SCORE_COUNT];
  VehicleSimulationState              *vehicle;
  VehicleTrafficCoordinatorReference  *reference;
  VehicleTrafficCoordinatorStatistics *statistics;
  VehicleTrafficLaneState             *lane_state;
  const DoublyLinkedNode              *node;
  const TrackSegment                  *segment;
  SimulationSource                    *active_source;
  SimulationSource                    *source;
  int32                                boundary_penalty;
  int32                                current_lane;
  int32                                edge_clearance;
  int32                                lane_counts[VEHICLE_TRAFFIC_COORDINATOR_LANE_COUNT];
  uint32                               lane_masks[VEHICLE_TRAFFIC_COORDINATOR_VEHICLE_COUNT];
  int32                                lane_scores[VEHICLE_TRAFFIC_COORDINATOR_SCORE_COUNT];
  int32                                left_lane;
  int32                                negative_boundary;
  int32                                positive_boundary;
  int32  primary_speed_limits[VEHICLE_TRAFFIC_COORDINATOR_SCORE_COUNT];
  int32  right_lane;
  int32  secondary_speed_limits[VEHICLE_TRAFFIC_COORDINATOR_SCORE_COUNT];
  int32  segment_difference;
  int32  selected_lane;
  int32  target_position;
  int32  target_speed_before_scoring;
  int32  vehicle_index;
  uint32 boundary_offsets;
  uint32 matched_lane_mask;
  uint32 product;
  uint32 status_flags;

  if(g_VEHICLE_TRAFFIC_COORDINATION_DISABLED != 0)
    return;
  g_VEHICLE_TRAFFIC_COORDINATION_COUNTER += VEHICLE_TRAFFIC_COORDINATOR_FRAME_STEP;

  active_vehicle = (VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE;
  active_source  = &active_vehicle->simulation_source;
  current_lane = (((active_source->lateral_position >> VEHICLE_TRAFFIC_COORDINATOR_POSITION_SHIFT) *
                   VEHICLE_TRAFFIC_COORDINATOR_LANE_SCALE) >>
                  VEHICLE_TRAFFIC_COORDINATOR_SCALE_SHIFT) +
                 VEHICLE_TRAFFIC_COORDINATOR_LANE_BIAS;
  if(current_lane < VEHICLE_TRAFFIC_COORDINATOR_LANE_MINIMUM)
    current_lane = VEHICLE_TRAFFIC_COORDINATOR_LANE_MINIMUM;
  else if(current_lane > VEHICLE_TRAFFIC_COORDINATOR_LANE_MAXIMUM)
    current_lane = VEHICLE_TRAFFIC_COORDINATOR_LANE_MAXIMUM;
  g_VEHICLE_TRAFFIC_ACTIVE_LANE_INDEX = current_lane;

  statistics = (VehicleTrafficCoordinatorStatistics *) g_VEHICLE_GEAR_SHIFT_STATISTICS;
  if(statistics->active_lane_index != current_lane)
    {
      statistics->lane_change_debounce++;
      if(statistics->lane_change_debounce > VEHICLE_TRAFFIC_COORDINATOR_LANE_DEBOUNCE_LIMIT)
        {
          statistics->active_lane_index    = current_lane;
          statistics->lane_change_debounce = 0;
          statistics->lane_change_count++;
        }
    }

  update_vehicle_overtake_statistics();
  advance_vehicle_track_feature_event();

  for(current_lane = 0; current_lane < VEHICLE_TRAFFIC_COORDINATOR_LANE_COUNT; ++current_lane)
    {
      lane_counts[current_lane] = 0;
    }

  node          = vehicle_list_->head_sentinel->next;
  vehicle_index = 0;
  while(node->next != NULL)
    {
      lane_state = (VehicleTrafficLaneState *) node;
      vehicle    = (VehicleSimulationState *) lane_state;
      if(lane_state->update_callback !=
         (SimulationUpdateCallback) update_traffic_vehicle_simulation)
        {
          refresh_vehicle_traffic_lane_state(vehicle);
          source    = &vehicle->simulation_source;
          left_lane = ((((source->lateral_position - lane_state->projected_lateral_extent) +
                         VEHICLE_TRAFFIC_COORDINATOR_EXTENT_ROUNDING) >>
                        VEHICLE_TRAFFIC_COORDINATOR_POSITION_SHIFT) *
                         VEHICLE_TRAFFIC_COORDINATOR_LANE_SCALE >>
                       VEHICLE_TRAFFIC_COORDINATOR_SCALE_SHIFT) +
                      VEHICLE_TRAFFIC_COORDINATOR_LANE_BIAS;
          if(left_lane < VEHICLE_TRAFFIC_COORDINATOR_LANE_MINIMUM)
            left_lane = VEHICLE_TRAFFIC_COORDINATOR_LANE_MINIMUM;
          else if(left_lane > VEHICLE_TRAFFIC_COORDINATOR_LANE_MAXIMUM)
            left_lane = VEHICLE_TRAFFIC_COORDINATOR_LANE_MAXIMUM;

          right_lane = (((source->lateral_position + lane_state->projected_lateral_extent -
                          VEHICLE_TRAFFIC_COORDINATOR_EXTENT_ROUNDING) >>
                         VEHICLE_TRAFFIC_COORDINATOR_POSITION_SHIFT) *
                          VEHICLE_TRAFFIC_COORDINATOR_LANE_SCALE >>
                        VEHICLE_TRAFFIC_COORDINATOR_SCALE_SHIFT) +
                       VEHICLE_TRAFFIC_COORDINATOR_LANE_BIAS;
          if(right_lane < VEHICLE_TRAFFIC_COORDINATOR_LANE_MINIMUM)
            right_lane = VEHICLE_TRAFFIC_COORDINATOR_LANE_MINIMUM;
          else if(right_lane > VEHICLE_TRAFFIC_COORDINATOR_LANE_MAXIMUM)
            right_lane = VEHICLE_TRAFFIC_COORDINATOR_LANE_MAXIMUM;

          if(left_lane == right_lane)
            {
              lane_masks[vehicle_index] = (uint32) VEHICLE_TRAFFIC_COORDINATOR_SINGLE_LANE_MASK
                                       << left_lane;
              lane_counts[left_lane]++;
            }
          else if((left_lane + 1) == right_lane)
            {
              lane_masks[vehicle_index] = (uint32) VEHICLE_TRAFFIC_COORDINATOR_DOUBLE_LANE_MASK
                                       << left_lane;
              lane_counts[left_lane]++;
              lane_counts[right_lane]++;
            }
          else
            {
              lane_masks[vehicle_index] = (uint32) VEHICLE_TRAFFIC_COORDINATOR_TRIPLE_LANE_MASK
                                       << left_lane;
              lane_counts[left_lane]++;
              lane_counts[left_lane + 1]++;
              lane_counts[right_lane]++;
            }
        }

      node = node->next;
      vehicle_index++;
    }

  active_source->status_flags &= ~VEHICLE_TRAFFIC_COORDINATOR_NEARBY_FLAGS;

  node = vehicle_list_->head_sentinel->next;
  while(node->next != NULL)
    {
      lane_state = (VehicleTrafficLaneState *) node;
      vehicle    = (VehicleSimulationState *) lane_state;
      source     = &vehicle->simulation_source;
      if((lane_state->update_callback !=
          (SimulationUpdateCallback) update_traffic_vehicle_simulation) &&
         ((lane_state->runtime_flags & VEHICLE_TRAFFIC_COORDINATOR_RUNTIME_ACTIVE_FLAG) != 0) &&
         (vehicle != active_vehicle) &&
         (lane_state->update_callback != (SimulationUpdateCallback) update_player_vehicle_state))
        {
          current_lane =
            (((source->lateral_position >> VEHICLE_TRAFFIC_COORDINATOR_POSITION_SHIFT) *
              VEHICLE_TRAFFIC_COORDINATOR_LANE_SCALE) >>
             VEHICLE_TRAFFIC_COORDINATOR_SCALE_SHIFT) +
            VEHICLE_TRAFFIC_COORDINATOR_LANE_BIAS;
          if(current_lane < VEHICLE_TRAFFIC_COORDINATOR_LANE_MINIMUM)
            current_lane = VEHICLE_TRAFFIC_COORDINATOR_LANE_MINIMUM;
          else if(current_lane > VEHICLE_TRAFFIC_COORDINATOR_LANE_MAXIMUM)
            current_lane = VEHICLE_TRAFFIC_COORDINATOR_LANE_MAXIMUM;

          status_flags = source->status_flags;
          if((status_flags & VEHICLE_TRAFFIC_COORDINATOR_REVERSE_FLAG) == 0)
            {
              lane_scores[VEHICLE_TRAFFIC_COORDINATOR_LEFT_SCORE_INDEX] =
                -VEHICLE_TRAFFIC_COORDINATOR_FIXED_ONE;
              lane_scores[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX] = 0;
              lane_scores[VEHICLE_TRAFFIC_COORDINATOR_RIGHT_SCORE_INDEX]  = 0;
            }
          else
            {
              lane_scores[VEHICLE_TRAFFIC_COORDINATOR_LEFT_SCORE_INDEX] =
                VEHICLE_TRAFFIC_COORDINATOR_FIXED_ONE;
              lane_scores[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX] = 0;
              lane_scores[VEHICLE_TRAFFIC_COORDINATOR_RIGHT_SCORE_INDEX] =
                -VEHICLE_TRAFFIC_COORDINATOR_FIXED_ONE;
            }

          lane_neighbors[VEHICLE_TRAFFIC_COORDINATOR_LEFT_SCORE_INDEX]   = NULL;
          lane_neighbors[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX] = NULL;
          lane_neighbors[VEHICLE_TRAFFIC_COORDINATOR_RIGHT_SCORE_INDEX]  = NULL;

          if((status_flags & VEHICLE_TRAFFIC_COORDINATOR_TRAFFIC_FLAG) != 0)
            {
              update_racer_proximity_event(vehicle, current_lane, (Vector3 *) lane_scores);
            }
          if((status_flags & VEHICLE_TRAFFIC_COORDINATOR_PLAYER_FLAG) != 0)
            {
              update_traffic_vehicle_lane_response(vehicle, current_lane, (Vector3 *) lane_scores);
            }

          segment =
            (const TrackSegment *) &g_REFERENCE_DISTANCE_RECORDS[source->track_segment_index];
          if(((status_flags & VEHICLE_TRAFFIC_COORDINATOR_EVENT_MASK) != 0) &&
             ((segment->status_flags & VEHICLE_TRAFFIC_COORDINATOR_TRACK_EVENT_FLAG) != 0))
            {
              segment_difference = source->track_segment_index - active_source->track_segment_index;
              if(segment_difference < 0)
                segment_difference = -segment_difference;
              if(segment_difference <= VEHICLE_TRAFFIC_COORDINATOR_EVENT_SEGMENT_LIMIT)
                {
                  trigger_vehicle_track_collision_event(vehicle);
                }
            }

          boundary_offsets = segment->lane_boundary_offsets;
          if(((boundary_offsets & VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_MASK) ==
              ((uint32) segment[VEHICLE_TRAFFIC_COORDINATOR_TRANSITION_SEGMENT_GAP]
                 .lane_boundary_offsets &
               VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_MASK)) &&
             (lane_counts[current_lane] == 1) &&
             ((((status_flags & VEHICLE_TRAFFIC_COORDINATOR_REVERSE_FLAG) == 0) &&
               (current_lane < VEHICLE_TRAFFIC_COORDINATOR_DIRECTION_DIVIDE)) ||
              (((status_flags & VEHICLE_TRAFFIC_COORDINATOR_REVERSE_FLAG) != 0) &&
               (current_lane >= VEHICLE_TRAFFIC_COORDINATOR_DIRECTION_DIVIDE))))
            {
              negative_boundary =
                VEHICLE_TRAFFIC_COORDINATOR_NEGATIVE_LANE_BASE -
                (int32) (boundary_offsets >> VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_SHIFT);
              positive_boundary =
                (int32) (boundary_offsets & VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_MASK) +
                VEHICLE_TRAFFIC_COORDINATOR_POSITIVE_LANE_BASE;
              if((negative_boundary < current_lane) && (current_lane < positive_boundary) &&
                 ((status_flags & VEHICLE_TRAFFIC_COORDINATOR_EVENT_MASK) == 0))
                {
                  source->track_position_offset =
                    current_lane * VEHICLE_TRAFFIC_COORDINATOR_LANE_POSITION_SCALE -
                    VEHICLE_TRAFFIC_COORDINATOR_LANE_POSITION_BIAS;
                  node = node->next;
                  continue;
                }
            }

          if((status_flags & VEHICLE_TRAFFIC_COORDINATOR_SPEED_HOLD_FLAG) == 0)
            {
              if((status_flags & VEHICLE_TRAFFIC_COORDINATOR_REVERSE_FLAG) == 0)
                {
                  if((current_lane - 1) < VEHICLE_TRAFFIC_COORDINATOR_DIRECTION_DIVIDE)
                    {
                      lane_scores[VEHICLE_TRAFFIC_COORDINATOR_LEFT_SCORE_INDEX] -=
                        VEHICLE_TRAFFIC_COORDINATOR_FORWARD_SIDE_PENALTY;
                    }
                  if(current_lane < VEHICLE_TRAFFIC_COORDINATOR_DIRECTION_DIVIDE)
                    {
                      lane_scores[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX] =
                        -VEHICLE_TRAFFIC_COORDINATOR_FORWARD_SIDE_PENALTY;
                    }
                  if((current_lane + 1) < VEHICLE_TRAFFIC_COORDINATOR_DIRECTION_DIVIDE)
                    {
                      lane_scores[VEHICLE_TRAFFIC_COORDINATOR_RIGHT_SCORE_INDEX] -=
                        VEHICLE_TRAFFIC_COORDINATOR_FORWARD_SIDE_PENALTY;
                    }
                }
              else
                {
                  if((current_lane - 1) >= VEHICLE_TRAFFIC_COORDINATOR_DIRECTION_DIVIDE)
                    {
                      lane_scores[VEHICLE_TRAFFIC_COORDINATOR_LEFT_SCORE_INDEX] -=
                        VEHICLE_TRAFFIC_COORDINATOR_REVERSE_SIDE_PENALTY;
                    }
                  if(current_lane >= VEHICLE_TRAFFIC_COORDINATOR_DIRECTION_DIVIDE)
                    {
                      lane_scores[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX] =
                        -VEHICLE_TRAFFIC_COORDINATOR_REVERSE_SIDE_PENALTY;
                    }
                  if((current_lane + 1) >= VEHICLE_TRAFFIC_COORDINATOR_DIRECTION_DIVIDE)
                    {
                      lane_scores[VEHICLE_TRAFFIC_COORDINATOR_RIGHT_SCORE_INDEX] -=
                        VEHICLE_TRAFFIC_COORDINATOR_REVERSE_SIDE_PENALTY;
                    }
                }

              if(((status_flags & VEHICLE_TRAFFIC_COORDINATOR_REVERSE_FLAG) == 0) &&
                 ((boundary_offsets & VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_MASK) >
                  ((uint32) segment[VEHICLE_TRAFFIC_COORDINATOR_TRANSITION_SEGMENT_GAP]
                     .lane_boundary_offsets &
                   VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_MASK)))
                {
                  negative_boundary =
                    VEHICLE_TRAFFIC_COORDINATOR_NEGATIVE_LANE_BASE -
                    (int32) (boundary_offsets >> VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_SHIFT);
                  positive_boundary =
                    (int32) (boundary_offsets & VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_MASK) +
                    VEHICLE_TRAFFIC_COORDINATOR_POSITIVE_LANE_BASE;
                  if(((current_lane + 1) == negative_boundary) ||
                     ((current_lane + 1) == positive_boundary))
                    {
                      lane_scores[VEHICLE_TRAFFIC_COORDINATOR_LEFT_SCORE_INDEX] +=
                        VEHICLE_TRAFFIC_COORDINATOR_BOUNDARY_BONUS;
                    }
                  else if(((current_lane + VEHICLE_TRAFFIC_COORDINATOR_SECOND_ADJACENT_LANE) ==
                           negative_boundary) ||
                          ((current_lane + VEHICLE_TRAFFIC_COORDINATOR_SECOND_ADJACENT_LANE) ==
                           positive_boundary))
                    {
                      lane_scores[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX] +=
                        VEHICLE_TRAFFIC_COORDINATOR_BOUNDARY_BONUS;
                    }
                }
              else if(((status_flags & VEHICLE_TRAFFIC_COORDINATOR_REVERSE_FLAG) == 0) &&
                      (source->track_segment_index >
                       VEHICLE_TRAFFIC_COORDINATOR_TRANSITION_SEGMENT_GAP))
                {
                  reference = (VehicleTrafficCoordinatorReference *) source->runtime_record;
                  if((reference->lane_transition_scale < VEHICLE_TRAFFIC_COORDINATOR_FIXED_ONE) &&
                     ((uint32) segment[-VEHICLE_TRAFFIC_COORDINATOR_TRANSITION_SEGMENT_GAP]
                        .lane_boundary_offsets &
                      VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_MASK) <
                       (boundary_offsets & VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_MASK))
                    {
                      negative_boundary =
                        VEHICLE_TRAFFIC_COORDINATOR_NEGATIVE_LANE_BASE -
                        (int32) (boundary_offsets >> VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_SHIFT);
                      positive_boundary =
                        (int32) (boundary_offsets & VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_MASK) +
                        VEHICLE_TRAFFIC_COORDINATOR_POSITIVE_LANE_BASE;
                      if(((current_lane + 1) == negative_boundary) ||
                         ((current_lane + 1) == positive_boundary))
                        {
                          lane_scores[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX] +=
                            VEHICLE_TRAFFIC_COORDINATOR_BOUNDARY_BONUS;
                        }
                      else
                        {
                          lane_scores[VEHICLE_TRAFFIC_COORDINATOR_RIGHT_SCORE_INDEX] +=
                            VEHICLE_TRAFFIC_COORDINATOR_BOUNDARY_BONUS;
                        }
                    }
                }

              if((status_flags & VEHICLE_TRAFFIC_COORDINATOR_PLAYER_FLAG) == 0)
                boundary_penalty = VEHICLE_TRAFFIC_COORDINATOR_TRAFFIC_EDGE_PENALTY;
              else
                boundary_penalty = VEHICLE_TRAFFIC_COORDINATOR_PLAYER_EDGE_PENALTY;

              negative_boundary =
                VEHICLE_TRAFFIC_COORDINATOR_NEGATIVE_LANE_BASE -
                (int32) (boundary_offsets >> VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_SHIFT);
              positive_boundary =
                (int32) (boundary_offsets & VEHICLE_TRAFFIC_COORDINATOR_LANE_NIBBLE_MASK) +
                VEHICLE_TRAFFIC_COORDINATOR_POSITIVE_LANE_BASE;
              reference = (VehicleTrafficCoordinatorReference *) source->runtime_record;

              left_lane = current_lane - 1;
              if((left_lane == negative_boundary) || (left_lane == positive_boundary))
                {
                  lane_scores[VEHICLE_TRAFFIC_COORDINATOR_LEFT_SCORE_INDEX] -= boundary_penalty;
                  edge_clearance = (int32) segment->negative_boundary_width *
                                     VEHICLE_TRAFFIC_COORDINATOR_BOUNDARY_WIDTH_SCALE -
                                   (int32) segment->midpoint_negative_width *
                                     VEHICLE_TRAFFIC_COORDINATOR_BOUNDARY_WIDTH_SCALE;
                  if(edge_clearance < reference->lateral_reference)
                    {
                      lane_scores[VEHICLE_TRAFFIC_COORDINATOR_LEFT_SCORE_INDEX] -=
                        VEHICLE_TRAFFIC_COORDINATOR_EDGE_CLEARANCE_PENALTY;
                    }
                }

              if((current_lane == negative_boundary) || (current_lane == positive_boundary))
                {
                  lane_scores[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX] -= boundary_penalty;
                  edge_clearance = (int32) segment->positive_boundary_width *
                                     VEHICLE_TRAFFIC_COORDINATOR_BOUNDARY_WIDTH_SCALE -
                                   (int32) segment->midpoint_positive_width *
                                     VEHICLE_TRAFFIC_COORDINATOR_BOUNDARY_WIDTH_SCALE;
                  if(edge_clearance < reference->lateral_reference)
                    {
                      lane_scores[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX] -=
                        VEHICLE_TRAFFIC_COORDINATOR_EDGE_CLEARANCE_PENALTY;
                    }
                }

              right_lane = current_lane + 1;
              if((right_lane == negative_boundary) || (right_lane == positive_boundary))
                {
                  lane_scores[VEHICLE_TRAFFIC_COORDINATOR_RIGHT_SCORE_INDEX] -= boundary_penalty;
                  edge_clearance = (int32) segment->positive_boundary_width *
                                     VEHICLE_TRAFFIC_COORDINATOR_BOUNDARY_WIDTH_SCALE -
                                   (int32) segment->midpoint_positive_width *
                                     VEHICLE_TRAFFIC_COORDINATOR_BOUNDARY_WIDTH_SCALE;
                  if(edge_clearance < reference->lateral_reference)
                    {
                      lane_scores[VEHICLE_TRAFFIC_COORDINATOR_RIGHT_SCORE_INDEX] -=
                        VEHICLE_TRAFFIC_COORDINATOR_EDGE_CLEARANCE_PENALTY;
                    }
                }

              if((left_lane < negative_boundary) || (left_lane > positive_boundary))
                {
                  lane_scores[VEHICLE_TRAFFIC_COORDINATOR_LEFT_SCORE_INDEX] -=
                    VEHICLE_TRAFFIC_COORDINATOR_OUTSIDE_LANE_PENALTY;
                }
              if((current_lane < negative_boundary) || (current_lane > positive_boundary))
                {
                  lane_scores[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX] -=
                    VEHICLE_TRAFFIC_COORDINATOR_OUTSIDE_LANE_PENALTY;
                }
              if((right_lane < negative_boundary) || (right_lane > positive_boundary))
                {
                  lane_scores[VEHICLE_TRAFFIC_COORDINATOR_RIGHT_SCORE_INDEX] -=
                    VEHICLE_TRAFFIC_COORDINATOR_OUTSIDE_LANE_PENALTY;
                }

              target_speed_before_scoring = source->target_track_speed;
              matched_lane_mask           = 0;
              primary_speed_limits[VEHICLE_TRAFFIC_COORDINATOR_LEFT_SCORE_INDEX] =
                source->target_track_speed;
              primary_speed_limits[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX] =
                source->target_track_speed;
              primary_speed_limits[VEHICLE_TRAFFIC_COORDINATOR_RIGHT_SCORE_INDEX] =
                source->target_track_speed;
              secondary_speed_limits[VEHICLE_TRAFFIC_COORDINATOR_LEFT_SCORE_INDEX] =
                source->target_track_speed;
              secondary_speed_limits[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX] =
                source->target_track_speed;
              secondary_speed_limits[VEHICLE_TRAFFIC_COORDINATOR_RIGHT_SCORE_INDEX] =
                source->target_track_speed;
              collect_traffic_lane_neighbor_limits(vehicle,
                                                   vehicle_list_,
                                                   lane_masks,
                                                   current_lane,
                                                   &matched_lane_mask,
                                                   primary_speed_limits,
                                                   secondary_speed_limits,
                                                   lane_neighbors);
              score_traffic_lane_speed_limits(vehicle,
                                              primary_speed_limits,
                                              secondary_speed_limits,
                                              lane_scores);

              if(((status_flags & VEHICLE_TRAFFIC_COORDINATOR_REVERSE_FLAG) != 0) &&
                 (lane_neighbors[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX] != NULL) &&
                 (lane_neighbors[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX]
                    ->simulation_source.minimum_track_speed >
                  VEHICLE_TRAFFIC_COORDINATOR_REVERSE_NEIGHBOR_SPEED))
                {
                  lane_scores[VEHICLE_TRAFFIC_COORDINATOR_RIGHT_SCORE_INDEX] -=
                    VEHICLE_TRAFFIC_COORDINATOR_REVERSE_NEIGHBOR_PENALTY;
                }

              if(((status_flags & VEHICLE_TRAFFIC_COORDINATOR_EVENT_MASK) == 0) &&
                 ((status_flags & VEHICLE_TRAFFIC_COORDINATOR_TRAFFIC_FLAG) == 0) &&
                 ((status_flags & VEHICLE_TRAFFIC_COORDINATOR_REVERSE_FLAG) != 0) &&
                 (source->track_segment_index > active_source->track_segment_index) &&
                 ((source->track_segment_index - active_source->track_segment_index) <
                  VEHICLE_TRAFFIC_COORDINATOR_SOUND_SEGMENT_LIMIT) &&
                 (target_speed_before_scoring > VEHICLE_TRAFFIC_COORDINATOR_SOUND_SPEED_MINIMUM))
                {
                  product = g_VEHICLE_SPAWN_RANDOM_STATE.multiplier *
                            g_VEHICLE_SPAWN_RANDOM_STATE.low_word;
                  g_VEHICLE_SPAWN_RANDOM_STATE.product = product;
                  g_VEHICLE_SPAWN_RANDOM_STATE.low_word =
                    product & VEHICLE_TRAFFIC_COORDINATOR_SOUND_RANDOM_MASK;
                  if(((((product & VEHICLE_TRAFFIC_COORDINATOR_SOUND_VALUE_MASK) >>
                        VEHICLE_TRAFFIC_COORDINATOR_SOUND_VALUE_SHIFT) &
                       VEHICLE_TRAFFIC_COORDINATOR_SOUND_BYTE_MASK) <
                      VEHICLE_TRAFFIC_COORDINATOR_SOUND_PROBABILITY) &&
                     (source->minimum_track_speed != 0) &&
                     (g_VEHICLE_PROXIMITY_ALERT_RATE_INDEX == 0))
                    {
                      request_traffic_vehicle_spatial_sound(lane_state->runtime_slot_index,
                                                            lane_state->spatial_sound_variant);
                    }
                }

              selected_lane   = current_lane;
              target_position = lane_scores[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX];
              if(target_position < lane_scores[VEHICLE_TRAFFIC_COORDINATOR_LEFT_SCORE_INDEX])
                {
                  target_position = lane_scores[VEHICLE_TRAFFIC_COORDINATOR_LEFT_SCORE_INDEX];
                  selected_lane   = current_lane - 1;
                }
              if(target_position < lane_scores[VEHICLE_TRAFFIC_COORDINATOR_RIGHT_SCORE_INDEX])
                {
                  selected_lane = current_lane + 1;
                }

              source->track_position_offset =
                selected_lane * VEHICLE_TRAFFIC_COORDINATOR_LANE_POSITION_SCALE -
                VEHICLE_TRAFFIC_COORDINATOR_LANE_POSITION_BIAS;
              target_position = (int32) segment->positive_boundary_width *
                                  VEHICLE_TRAFFIC_COORDINATOR_BOUNDARY_WIDTH_SCALE -
                                reference->lateral_reference;
              if(target_position < source->track_position_offset)
                source->track_position_offset = target_position;
              target_position =
                reference->lateral_reference - (int32) segment->negative_boundary_width *
                                                 VEHICLE_TRAFFIC_COORDINATOR_BOUNDARY_WIDTH_SCALE;
              if(source->track_position_offset < target_position)
                source->track_position_offset = target_position;

              if((status_flags & VEHICLE_TRAFFIC_COORDINATOR_REVERSE_FLAG) == 0)
                {
                  source->track_position_offset -= source->direction.z;
                }
              else
                {
                  source->track_position_offset += source->direction.z;
                }

              if((g_VEHICLE_TRAFFIC_LANE_CONSTRAINTS_ENABLED != 0) &&
                 (selected_lane != current_lane))
                {
                  constrain_traffic_vehicle_lane_speed(
                    vehicle,
                    (const VehicleTrafficLaneNeighbor
                       *) &lane_neighbors[VEHICLE_TRAFFIC_COORDINATOR_CENTER_SCORE_INDEX],
                    current_lane,
                    selected_lane,
                    primary_speed_limits);
                }
            }
        }

      node = node->next;
    }
}

void
score_traffic_lane_speed_limits(
  VehicleSimulationState *state_,
  const int32             primary_speed_limits_[VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT],
  const int32             secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT],
  int32                   lane_scores_[VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT])
{
  SimulationSource *source;
  int32             lane_index;
  int32             minimum_track_speed;
  int32             speed_difference;
  int32             speed_limit;
  int32             target_track_speed;

  source              = &state_->simulation_source;
  target_track_speed  = source->target_track_speed;
  minimum_track_speed = source->minimum_track_speed;
  if((source->status_flags & VEHICLE_TRAFFIC_LANE_REVERSE_FLAG) == 0)
    {
      for(lane_index = 0; lane_index < VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT; ++lane_index)
        {
          speed_limit = primary_speed_limits_[lane_index];
          if(speed_limit < target_track_speed)
            {
              speed_difference =
                (target_track_speed - speed_limit) >> VEHICLE_TRAFFIC_LANE_SPEED_DIFFERENCE_SHIFT;
              lane_scores_[lane_index] +=
                speed_difference * VEHICLE_TRAFFIC_LANE_PRIMARY_TARGET_SCORE_SCALE;
            }

          speed_limit = secondary_speed_limits_[lane_index];
          if(speed_limit < target_track_speed)
            {
              speed_difference =
                (target_track_speed - speed_limit) >> VEHICLE_TRAFFIC_LANE_SPEED_DIFFERENCE_SHIFT;
              lane_scores_[lane_index] +=
                speed_difference * VEHICLE_TRAFFIC_LANE_SECONDARY_TARGET_SCORE_SCALE;
            }

          speed_limit = primary_speed_limits_[lane_index];
          if(speed_limit < minimum_track_speed)
            {
              speed_difference =
                (minimum_track_speed - speed_limit) >> VEHICLE_TRAFFIC_LANE_SPEED_DIFFERENCE_SHIFT;
              lane_scores_[lane_index] +=
                speed_difference * VEHICLE_TRAFFIC_LANE_PRIMARY_SPEED_SCORE_SCALE;
            }

          speed_limit = secondary_speed_limits_[lane_index];
          if(speed_limit < minimum_track_speed)
            {
              speed_difference =
                (minimum_track_speed - speed_limit) >> VEHICLE_TRAFFIC_LANE_SPEED_DIFFERENCE_SHIFT;
              lane_scores_[lane_index] +=
                speed_difference * VEHICLE_TRAFFIC_LANE_SECONDARY_SPEED_SCORE_SCALE;
            }
        }

      speed_limit = primary_speed_limits_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX];
      if(speed_limit < target_track_speed)
        source->target_track_speed = speed_limit;
      if(source->target_track_speed < -VEHICLE_TRAFFIC_LANE_TARGET_SPEED_LIMIT)
        source->target_track_speed = -VEHICLE_TRAFFIC_LANE_TARGET_SPEED_LIMIT;
      return;
    }

  for(lane_index = 0; lane_index < VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT; ++lane_index)
    {
      speed_limit = primary_speed_limits_[lane_index];
      if(target_track_speed < speed_limit)
        {
          speed_difference =
            (speed_limit - target_track_speed) >> VEHICLE_TRAFFIC_LANE_SPEED_DIFFERENCE_SHIFT;
          lane_scores_[lane_index] +=
            speed_difference * VEHICLE_TRAFFIC_LANE_PRIMARY_TARGET_SCORE_SCALE;
        }

      speed_limit = secondary_speed_limits_[lane_index];
      if(target_track_speed < speed_limit)
        {
          speed_difference =
            (speed_limit - target_track_speed) >> VEHICLE_TRAFFIC_LANE_SPEED_DIFFERENCE_SHIFT;
          lane_scores_[lane_index] +=
            speed_difference * VEHICLE_TRAFFIC_LANE_SECONDARY_TARGET_SCORE_SCALE;
        }

      speed_limit = primary_speed_limits_[lane_index];
      if(minimum_track_speed < speed_limit)
        {
          speed_difference =
            (speed_limit - minimum_track_speed) >> VEHICLE_TRAFFIC_LANE_SPEED_DIFFERENCE_SHIFT;
          lane_scores_[lane_index] +=
            speed_difference * VEHICLE_TRAFFIC_LANE_PRIMARY_SPEED_SCORE_SCALE;
        }

      speed_limit = secondary_speed_limits_[lane_index];
      if(minimum_track_speed < speed_limit)
        {
          speed_difference =
            (speed_limit - minimum_track_speed) >> VEHICLE_TRAFFIC_LANE_SPEED_DIFFERENCE_SHIFT;
          lane_scores_[lane_index] +=
            speed_difference * VEHICLE_TRAFFIC_LANE_SECONDARY_SPEED_SCORE_SCALE;
        }
    }

  speed_limit = primary_speed_limits_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX];
  if(target_track_speed < speed_limit)
    source->target_track_speed = speed_limit;
  if(source->target_track_speed > VEHICLE_TRAFFIC_LANE_TARGET_SPEED_LIMIT)
    source->target_track_speed = VEHICLE_TRAFFIC_LANE_TARGET_SPEED_LIMIT;
}

void
constrain_traffic_vehicle_lane_speed(
  VehicleSimulationState           *state_,
  const VehicleTrafficLaneNeighbor *neighbor_,
  int32                             current_lane_,
  int32                             selected_lane_,
  const int32                       speed_limits_[VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT])
{
  const VehicleSimulationState  *neighbor_vehicle;
  const VehicleTrafficLaneState *callback_state;
  SimulationSource              *source;
  const SimulationSource        *neighbor_source;
  int32                          lane_speed_limit;
  int32                          lateral_limit;
  int32                          relative_motion;
  int32                          relative_speed;
  int32                          segment_distance;
  int32                          track_position_distance;

  source           = &state_->simulation_source;
  lane_speed_limit = speed_limits_[VEHICLE_TRAFFIC_LANE_CENTER_SPEED_INDEX];
  if(current_lane_ != selected_lane_)
    {
      neighbor_vehicle = neighbor_->vehicle;
      if(neighbor_vehicle != NULL)
        {
          callback_state = (const VehicleTrafficLaneState *) neighbor_vehicle;
          if(callback_state->update_callback !=
             (SimulationUpdateCallback) update_player_vehicle_state)
            {
              neighbor_source  = &neighbor_vehicle->simulation_source;
              segment_distance = source->track_segment_index - neighbor_source->track_segment_index;
              if(segment_distance <= 0)
                segment_distance = -segment_distance;
              segment_distance *= VEHICLE_TRAFFIC_LANE_SEGMENT_DISTANCE_SCALE;

              relative_speed = source->minimum_track_speed - neighbor_source->minimum_track_speed;
              if(relative_speed <= 0)
                relative_speed = -relative_speed;

              track_position_distance = source->track_position_offset - source->lateral_position;
              if(track_position_distance <= 0)
                track_position_distance = -track_position_distance;

              lateral_limit =
                limit_vehicle_lateral_acceleration(source, 1, source->minimum_track_speed);
              if(lateral_limit <= 0)
                lateral_limit = -lateral_limit;

              relative_motion  = MulSF16(track_position_distance, relative_speed);
              segment_distance = MulSF16(segment_distance, lateral_limit);
              relative_motion =
                MulSF16(relative_motion, VEHICLE_TRAFFIC_LANE_RELATIVE_MOTION_SCALE);
              if(relative_motion < segment_distance)
                {
                  if(selected_lane_ > current_lane_)
                    {
                      lane_speed_limit = speed_limits_[VEHICLE_TRAFFIC_LANE_RIGHT_SPEED_INDEX];
                    }
                  else
                    {
                      lane_speed_limit = speed_limits_[VEHICLE_TRAFFIC_LANE_LEFT_SPEED_INDEX];
                    }
                }
            }
        }
    }

  if((source->status_flags & VEHICLE_TRAFFIC_LANE_REVERSE_FLAG) == 0)
    {
      if(lane_speed_limit < source->target_track_speed)
        source->target_track_speed = lane_speed_limit;
      if(source->target_track_speed < -VEHICLE_TRAFFIC_LANE_TARGET_SPEED_LIMIT)
        {
          source->target_track_speed = -VEHICLE_TRAFFIC_LANE_TARGET_SPEED_LIMIT;
        }
    }
  else
    {
      if(source->target_track_speed < lane_speed_limit)
        source->target_track_speed = lane_speed_limit;
      if(source->target_track_speed > VEHICLE_TRAFFIC_LANE_TARGET_SPEED_LIMIT)
        {
          source->target_track_speed = VEHICLE_TRAFFIC_LANE_TARGET_SPEED_LIMIT;
        }
    }
}
