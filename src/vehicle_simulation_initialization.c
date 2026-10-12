#include "doubly_linked_list.h"
#include "fixed_math.h"
#include "frontend_transition.h"
#include "game_bootstrap.h"
#include "road_data.h"
#include "simulation_thread_signal.h"
#include "track_position.h"
#include "vehicle_callbacks.h"
#include "vehicle_drive_initialization.h"
#include "vehicle_runtime_pool.h"
#include "vehicle_selection_state.h"
#include "vehicle_simulation_state.h"
#include "vehicle_traffic_randomization.h"

#include "operamath.h"

#define VEHICLE_REFRESH_STATE_BYTES 0x94
#define VEHICLE_RUNTIME_RECORD_BYTES 0x4E4
#define VEHICLE_FIXED8_SHIFT 8
#define VEHICLE_FIXED_ONE 0x10000
#define VEHICLE_DEFAULT_TRACK_SPEED 0x5B520
#define VEHICLE_REVERSE_TRACK_SPEED (-910222)
#define VEHICLE_REFRESH_PERIOD 0xA0
#define VEHICLE_STATUS_PLAYER_FLAG 0x00000004
#define VEHICLE_STATUS_TRAFFIC_FLAG 0x00000008
#define VEHICLE_STATUS_REVERSE_FLAG 0x00001000
#define VEHICLE_STATUS_REPOSITION_FLAG 0x00002000
#define VEHICLE_STATUS_ACTIVE_FLAG 0x00000400
#define VEHICLE_STATUS_RESET_MASK 0x00028400
#define VEHICLE_STATUS_SPEED_RESET_MASK 0x00001010
#define VEHICLE_RUNTIME_ACTIVE_FLAG 0x00000004
#define VEHICLE_TRACK_SPECIAL_MASK 0xF0
#define VEHICLE_TRACK_MATRIX_DIMENSION 3
#define VEHICLE_TRACK_SPEED_OVERRIDE_MASK 0x00000404
#define VEHICLE_LOW_WORD_MASK 0x0000FFFF
#define VEHICLE_RANDOM_VALUE_MASK 0x00FFFFFF
#define VEHICLE_RANDOM_ROUNDING_VALUE 0x8000
#define VEHICLE_RANDOM_VALUE_SHIFT 8
#define VEHICLE_RANDOM_SCALE_SHIFT 16
#define VEHICLE_SIMULATION_RENDER_MODE 6
#define VEHICLE_SIMULATION_DEFAULT_MODE 4
#define VEHICLE_TRACK_END_MARGIN 56
#define VEHICLE_TRAFFIC_POSITION_BASE 25
#define VEHICLE_MAXIMUM_ACTIVE_COUNT 4
#define VEHICLE_TRACK_NEAR_DISTANCE 54
#define VEHICLE_TRACK_MINIMUM_DISTANCE (-20)
#define VEHICLE_HEADING_RECOVERY_STEP 0x13333
#define VEHICLE_PLAYER_HEADING_RECOVERY_STEP 0x18000
#define VEHICLE_PENDING_POSITION_Z 0x80000
#define VEHICLE_STEERING_RANDOM_ROUNDING 0x8000
#define VEHICLE_TRACK_COMPONENT_BYTE_SHIFT 8
#define VEHICLE_TRACK_NORMAL_COMPONENT_SCALE 2
#define VEHICLE_STATUS_ACTIVE_CONTACT_RESET_MASK 0x00012400
#define VEHICLE_RANDOM_DIRECTION_THRESHOLD 0x7FFF
#define VEHICLE_DIRECTION_COUNT_LIMIT 2
#define VEHICLE_TRAFFIC_REVERSE_TRACK_OFFSET 18
#define VEHICLE_SPECIAL_REVERSE_TRACK_OFFSET 12
#define VEHICLE_FORWARD_TRACK_OFFSET 43
#define VEHICLE_FORWARD_TRACK_RANGE 11
#define VEHICLE_RANDOM_TRACK_MASK 0x0F
#define VEHICLE_TRACK_LANE_NIBBLE_SHIFT 4
#define VEHICLE_LANE_SPACING 0x50000
#define VEHICLE_LANE_CENTER_OFFSET 0x28000
#define VEHICLE_PLAYER_TRACK_REFERENCE_OFFSET 0xC8000
#define VEHICLE_ACTIVE_PLAYER_POSITION_X (-0x28000)
#define VEHICLE_ACTIVE_SPEED_REVERSE_LIMIT 0x10000
#define VEHICLE_ART_MINIMUM_VARIANT_COUNT 2
#define VEHICLE_TRACK_SPEED_GROUP_SHIFT 2
#define VEHICLE_TRACK_SPEED_GROUP_BYTES 3
#define VEHICLE_SELECTED_INDEX_MAXIMUM 7
#define VEHICLE_TRACK_HEADING_SHIFT 10
#define VEHICLE_ANGLE_FULL_TURN 0x1000000
#define VEHICLE_ANGLE_HALF_TURN 0x800000
#define VEHICLE_TRACK_CURVATURE_SHIFT_PRIMARY 3
#define VEHICLE_TRACK_CURVATURE_SHIFT_SECONDARY 4
#define VEHICLE_TRACK_CURVATURE_SHIFT_TERTIARY 6
#define VEHICLE_TRACK_GAP_DIVISOR 10
#define VEHICLE_TRACK_GAP_BIAS 10
#define VEHICLE_TRACK_GAP_MAXIMUM 20
#define VEHICLE_TRACK_CURVATURE_BIAS 0xCCCC
#define VEHICLE_ACTIVE_SPEED_WARNING_LIMIT 0x20000
#define VEHICLE_TRACK_END_FADE_SCALE 0xCCC

#define COMPILE_TIME_SIZE_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]

typedef struct VehicleRuntimeRecord
{
  DoublyLinkedNode         links;
  uint8                    reserved_008[0x48];
  int32                    track_segment_index;
  uint8                    reserved_054[0x470];
  SimulationUpdateCallback update_callback;
  int32                    traffic_position;
  uint32                   status_flags;
  uint8                    reserved_4_d0[4];
  void                    *configuration;
  uint8                    reserved_4_d8[0x0C];
} VehicleRuntimeRecord;

typedef struct VehicleArtConfiguration
{
  uint8 reserved_000[0x110];
  int32 variant_count;
} VehicleArtConfiguration;

typedef struct VehicleTrackSpeedActiveState
{
  uint8                    reserved_000[0x50];
  int32                    track_segment_index;
  uint8                    reserved_054[0x1C];
  int32                    speed_magnitude;
  uint8                    reserved_074[0x450];
  SimulationUpdateCallback update_callback;
} VehicleTrackSpeedActiveState;

extern void
update_player_vehicle_state(void *state_);

static void
_insert_vehicle_record_by_track_position(SimulationSource *source_, int32 *track_segment_index_);

static void
_select_vehicle_track_speed(SimulationSource *source_, int32 placement_mode_);

static void
_copy_vehicle_track_transform(SimulationSource *source_);

static void
_activate_vehicle_record(SimulationSource *source_);

static uint32
_next_vehicle_spawn_random_value(void);

static int32
_read_signed_track_heading(const int16 *heading_);

static int32
_read_track_normal_component(const int16 *component_)
{
  const uint8 *bytes;
  int32        value;

  bytes = (const uint8 *) component_;
  value = ((int32) (int8) bytes[0] << VEHICLE_TRACK_COMPONENT_BYTE_SHIFT) | bytes[1];
  return value * VEHICLE_TRACK_NORMAL_COMPONENT_SCALE;
}

void
update_traffic_vehicle_simulation(void *state_)
{
  SimulationSource              *source;
  SimulationSource              *active_source;
  VehicleRuntimeRecord          *record;
  VehicleSimulationRefreshState *refresh_state;
  int32                          active_track_segment;

  source        = (SimulationSource *) state_;
  active_source = &((VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE)->simulation_source;
  record        = (VehicleRuntimeRecord *) source->runtime_record;
  refresh_state = (VehicleSimulationRefreshState *) source->refresh_state;
  active_track_segment = active_source->track_segment_index;

  source->position.y          = 0;
  source->track_segment_index = 1;
  source->position.x          = 0;
  source->position.z          = VEHICLE_PENDING_POSITION_Z;

  refresh_state->activation_countdown--;
  if(refresh_state->activation_countdown != 0)
    return;

  if(((g_GAME_RUNTIME_STATE.track_segment_count - VEHICLE_TRACK_END_MARGIN) <=
      active_track_segment) ||
     ((VEHICLE_TRAFFIC_POSITION_BASE + (record->traffic_position << 1)) > active_track_segment) ||
     ((VEHICLE_MAXIMUM_ACTIVE_COUNT - g_VEHICLE_SPAWN_CONFIGURATION.maximum_active_vehicle_count) >=
      g_VEHICLE_SPAWN_POSITION_STATE.active_vehicle_count) ||
     ((source->status_flags & VEHICLE_STATUS_PLAYER_FLAG) != 0))
    {
      refresh_state->activation_countdown = 1;
      return;
    }

  record->update_callback = (SimulationUpdateCallback) update_vehicle_simulation_transform;
  g_VEHICLE_SPAWN_POSITION_STATE.active_vehicle_count--;
  reposition_vehicle_simulation(source, 0);
}

void
update_recovering_vehicle_simulation(void *state_)
{
  SimulationSource     *source;
  SimulationSource     *active_source;
  VehicleRuntimeRecord *record;
  int32                 track_gap;
  int32                 recovery_step;

  source        = (SimulationSource *) state_;
  active_source = &((VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE)->simulation_source;
  record        = (VehicleRuntimeRecord *) source->runtime_record;
  track_gap     = source->track_segment_index - active_source->track_segment_index;

  if((source->status_flags & VEHICLE_STATUS_TRAFFIC_FLAG) != 0)
    clear_vehicle_simulation_contact_flags(1);

  recovery_step = VEHICLE_HEADING_RECOVERY_STEP;
  if((source->status_flags & VEHICLE_STATUS_PLAYER_FLAG) != 0)
    recovery_step = VEHICLE_PLAYER_HEADING_RECOVERY_STEP;

  if(source->heading_velocity > 0)
    {
      source->heading_velocity -= recovery_step;
      if(source->heading_velocity < 0)
        source->heading_velocity = 0;
      else if(source->heading_velocity > 0)
        source->heading_velocity = 0;
    }
  else if(source->heading_velocity < 0)
    {
      source->heading_velocity += recovery_step;
      if(source->heading_velocity > 0)
        source->heading_velocity = 0;
    }

  if((track_gap >= VEHICLE_TRACK_MINIMUM_DISTANCE) && (track_gap <= VEHICLE_TRACK_NEAR_DISTANCE))
    {
      source->target_track_speed = 0;
      update_vehicle_simulation_transform(source);
      return;
    }

  if(record->update_callback == (SimulationUpdateCallback) update_traffic_vehicle_simulation)
    g_VEHICLE_SPAWN_POSITION_STATE.active_vehicle_count--;
  record->update_callback = (SimulationUpdateCallback) update_vehicle_simulation_transform;
}

void
rebuild_vehicle_track_frame(SimulationSource *source_)
{
  TrackSegment *segment;
  TrackSegment *next_segment;
  Vector3       tangent;
  Vector3       normal;
  uint32        product;
  uint32        random_value;
  int32         threshold;

  segment      = &g_TRACK_SEGMENTS[source_->track_segment_index];
  next_segment = segment + 1;

  source_->track_reference_position.x = segment->surface_reference.x;
  source_->track_reference_position.y = segment->surface_reference.y;
  source_->track_reference_position.z = segment->surface_reference.z;

  tangent.x = next_segment->surface_reference.x - segment->surface_reference.x;
  tangent.y = next_segment->surface_reference.y - segment->surface_reference.y;
  tangent.z = next_segment->surface_reference.z - segment->surface_reference.z;
  normalize_vector3_fixed8_in_place(&tangent);

  normal.x = _read_track_normal_component(&segment->surface_offset_x);
  normal.y = _read_track_normal_component(&segment->surface_offset_y);
  normal.z = _read_track_normal_component(&segment->surface_offset_z);

  source_->track_basis.m00 = normal.x;
  source_->track_basis.m01 = normal.y;
  source_->track_basis.m02 = normal.z;
  source_->track_basis.m10 =
    ((normal.z >> VEHICLE_FIXED8_SHIFT) * (tangent.y >> VEHICLE_FIXED8_SHIFT)) -
    ((normal.y >> VEHICLE_FIXED8_SHIFT) * (tangent.z >> VEHICLE_FIXED8_SHIFT));
  source_->track_basis.m11 =
    ((normal.x >> VEHICLE_FIXED8_SHIFT) * (tangent.z >> VEHICLE_FIXED8_SHIFT)) -
    ((normal.z >> VEHICLE_FIXED8_SHIFT) * (tangent.x >> VEHICLE_FIXED8_SHIFT));
  source_->track_basis.m12 =
    ((normal.y >> VEHICLE_FIXED8_SHIFT) * (tangent.x >> VEHICLE_FIXED8_SHIFT)) -
    ((normal.x >> VEHICLE_FIXED8_SHIFT) * (tangent.y >> VEHICLE_FIXED8_SHIFT));
  source_->track_basis.m20 = tangent.x;
  source_->track_basis.m21 = tangent.y;
  source_->track_basis.m22 = tangent.z;

  if(source_ == &((VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE)->simulation_source)
    return;

  product = (g_VEHICLE_SPAWN_RANDOM_STATE.low_word * g_VEHICLE_SPAWN_RANDOM_STATE.multiplier);
  g_VEHICLE_SPAWN_RANDOM_STATE.product  = product;
  g_VEHICLE_SPAWN_RANDOM_STATE.low_word = product & VEHICLE_LOW_WORD_MASK;
  random_value = ((product & VEHICLE_RANDOM_VALUE_MASK) >> VEHICLE_RANDOM_VALUE_SHIFT);
  threshold    = g_VEHICLE_STEERING_RANDOM_THRESHOLDS[g_VEHICLE_SPAWN_CONFIGURATION_MODE];
  if(((threshold * random_value + VEHICLE_STEERING_RANDOM_ROUNDING) >>
      VEHICLE_RANDOM_SCALE_SHIFT) == 1)
    randomize_vehicle_steering_offset(source_);
}

void
reposition_vehicle_simulation(SimulationSource *source_, int32 activate_record_)
{
  SimulationSource        *active_source;
  VehicleRuntimeRecord    *record;
  VehicleArtConfiguration *art_configuration;
  int32                    active_track_segment;
  int32                    previous_direction;
  int32                    selected_direction;
  int32                    selected_segment;
  int32                    position_offset;
  int32                    lane_count;
  int32                   *direction_counts;
  uint32                   random_value;
  uint32                   status_flags;

  active_source = &((VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE)->simulation_source;
  active_track_segment             = active_source->track_segment_index;
  selected_segment                 = source_->track_segment_index;
  source_->warning_display_index   = 0;
  source_->warning_display_pending = 0;
  if(source_ == active_source)
    active_source->status_flags &= ~VEHICLE_STATUS_ACTIVE_CONTACT_RESET_MASK;

  status_flags = source_->status_flags;
  if(((status_flags & VEHICLE_STATUS_REPOSITION_FLAG) == 0) &&
     (((status_flags & VEHICLE_STATUS_PLAYER_FLAG) == 0) ||
      (active_track_segment <= source_->track_segment_index)) &&
     (((status_flags & VEHICLE_STATUS_TRAFFIC_FLAG) == 0) ||
      ((g_TRACK_SEGMENTS[selected_segment].lane_boundary_offsets & VEHICLE_TRACK_SPECIAL_MASK) !=
       0)))
    {
      if((status_flags & VEHICLE_STATUS_TRAFFIC_FLAG) != 0)
        selected_direction = 0;
      else
        {
          random_value = _next_vehicle_spawn_random_value();
          if(random_value <= VEHICLE_RANDOM_DIRECTION_THRESHOLD)
            selected_direction = 0;
          else
            selected_direction = 1;
          if((g_TRACK_SEGMENTS[selected_segment].lane_boundary_offsets &
              VEHICLE_TRACK_SPECIAL_MASK) == 0)
            selected_direction = 1;

          if((status_flags & VEHICLE_STATUS_PLAYER_FLAG) == 0)
            {
              direction_counts = &g_VEHICLE_TRAFFIC_DIRECTION_COUNTS.reverse_count;
              if((status_flags & VEHICLE_STATUS_REVERSE_FLAG) == 0)
                previous_direction = 1;
              else
                previous_direction = 0;
              direction_counts[previous_direction]--;
              if(direction_counts[previous_direction] < 0)
                direction_counts[previous_direction] = 0;
              if(direction_counts[selected_direction] > VEHICLE_DIRECTION_COUNT_LIMIT)
                selected_direction = 1 - selected_direction;
              direction_counts[selected_direction]++;
            }
        }

      if((active_source->speed_magnitude < VEHICLE_ACTIVE_SPEED_REVERSE_LIMIT) &&
         (selected_direction == 1) && ((status_flags & VEHICLE_STATUS_PLAYER_FLAG) == 0))
        {
          random_value = _next_vehicle_spawn_random_value();
          if((status_flags & VEHICLE_STATUS_TRAFFIC_FLAG) == 0)
            selected_segment = (active_track_segment - VEHICLE_SPECIAL_REVERSE_TRACK_OFFSET -
                                ((int32) random_value & VEHICLE_RANDOM_TRACK_MASK));
          else
            selected_segment = (active_track_segment - VEHICLE_TRAFFIC_REVERSE_TRACK_OFFSET -
                                ((int32) random_value & VEHICLE_RANDOM_TRACK_MASK));
        }
      else
        {
          random_value     = _next_vehicle_spawn_random_value();
          selected_segment = (active_track_segment + VEHICLE_FORWARD_TRACK_OFFSET +
                              (int32) (((random_value * VEHICLE_FORWARD_TRACK_RANGE) +
                                        VEHICLE_RANDOM_ROUNDING_VALUE) >>
                                       VEHICLE_RANDOM_SCALE_SHIFT));
        }

      if((selected_direction == 0) && ((status_flags & VEHICLE_STATUS_PLAYER_FLAG) == 0))
        {
          random_value = _next_vehicle_spawn_random_value();
          lane_count   = ((int32) g_TRACK_SEGMENTS[selected_segment].lane_boundary_offsets >>
                          VEHICLE_TRACK_LANE_NIBBLE_SHIFT);
          position_offset =
            (((int32) ((((lane_count - 1) * random_value) + VEHICLE_RANDOM_ROUNDING_VALUE) >>
                       VEHICLE_RANDOM_SCALE_SHIFT) +
              1) *
             -VEHICLE_LANE_SPACING) +
            VEHICLE_LANE_CENTER_OFFSET;
        }
      else
        {
          random_value = _next_vehicle_spawn_random_value();
          lane_count   = ((int32) g_TRACK_SEGMENTS[selected_segment].lane_boundary_offsets &
                          VEHICLE_RANDOM_TRACK_MASK);
          position_offset =
            (((int32) ((((lane_count - 1) * random_value) + VEHICLE_RANDOM_ROUNDING_VALUE) >>
                       VEHICLE_RANDOM_SCALE_SHIFT) +
              1) *
             VEHICLE_LANE_SPACING) -
            VEHICLE_LANE_CENTER_OFFSET;
        }
    }
  else
    {
      selected_direction = 1;
      if((status_flags & VEHICLE_STATUS_TRAFFIC_FLAG) == 0)
        selected_segment = active_track_segment - VEHICLE_SPECIAL_REVERSE_TRACK_OFFSET;
      else
        selected_segment = active_track_segment - VEHICLE_TRAFFIC_REVERSE_TRACK_OFFSET;
      random_value = _next_vehicle_spawn_random_value();
      lane_count   = ((int32) g_TRACK_SEGMENTS[selected_segment].lane_boundary_offsets &
                      VEHICLE_RANDOM_TRACK_MASK);
      position_offset =
        (((int32) ((((lane_count - 1) * random_value) + VEHICLE_RANDOM_ROUNDING_VALUE) >>
                   VEHICLE_RANDOM_SCALE_SHIFT) +
          1) *
         VEHICLE_LANE_SPACING) -
        VEHICLE_LANE_CENTER_OFFSET;
    }

  position_offset += source_->direction.z;
  source_->warning_display_state = (active_track_segment < selected_segment);
  if(((status_flags & VEHICLE_STATUS_PLAYER_FLAG) != 0) &&
     (g_OPPONENT_VEHICLE_INDEX != VEHICLE_SIMULATION_DISABLED_MODE))
    {
      selected_segment          = active_track_segment;
      active_source->position.x = VEHICLE_ACTIVE_PLAYER_POSITION_X;
      if((g_TRACK_SEGMENTS[selected_segment].lane_boundary_offsets & VEHICLE_TRACK_SPECIAL_MASK) ==
         0)
        {
          position_offset = ((g_TRACK_SEGMENTS[selected_segment].surface_reference.x +
                              VEHICLE_PLAYER_TRACK_REFERENCE_OFFSET) -
                             source_->direction.z);
        }
      else
        position_offset = VEHICLE_LANE_CENTER_OFFSET - source_->direction.z;
    }

  initialize_vehicle_simulation_position(source_,
                                         selected_segment,
                                         selected_direction,
                                         position_offset,
                                         activate_record_);

  record            = (VehicleRuntimeRecord *) source_->runtime_record;
  art_configuration = (VehicleArtConfiguration *) record->configuration;
  if(art_configuration->variant_count >= VEHICLE_ART_MINIMUM_VARIANT_COUNT)
    {
      random_value         = _next_vehicle_spawn_random_value();
      source_->art_variant = (uint8) ((((art_configuration->variant_count - 1) * random_value) +
                                       VEHICLE_RANDOM_ROUNDING_VALUE) >>
                                      VEHICLE_RANDOM_SCALE_SHIFT);
    }
}

int32
calculate_vehicle_track_speed(SimulationSource *source_)
{
  VehicleTrackSpeedActiveState *active_state;
  const TrackSegment           *segment;
  int32                         base_speed;
  int32                         selected_index;
  int32                         tuning_mode;
  int32                         heading_change;
  int32                         curvature;
  int32                         gap_index;
  int32                         speed_scale;
  int32                         scaled_curvature;
  int32                         result;
  int32                         fade;

  base_speed =
    ((int32)
       g_TRACK_RUNTIME_DATA[(source_->track_segment_index >> VEHICLE_TRACK_SPEED_GROUP_SHIFT) *
                            VEHICLE_TRACK_SPEED_GROUP_BYTES]
     << VEHICLE_RANDOM_SCALE_SHIFT);
  if((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) == 0)
    {
      selected_index = g_PLAYER_VEHICLE_INDEX;
      if((selected_index < 0) || (selected_index > VEHICLE_SELECTED_INDEX_MAXIMUM))
        return base_speed;
      tuning_mode = g_VEHICLE_SPAWN_CONFIGURATION_MODE;
      return multiply_fixed16(base_speed,
                              g_VEHICLE_RENDER_TUNING_SCALES[tuning_mode][selected_index]);
    }

  active_state = (VehicleTrackSpeedActiveState *) G_ACTIVE_SIMULATION_SOURCE;
  tuning_mode  = g_VEHICLE_SPAWN_CONFIGURATION_MODE;
  segment      = &g_TRACK_SEGMENTS[source_->track_segment_index];
  heading_change =
    ((_read_signed_track_heading(&segment->heading) << VEHICLE_TRACK_HEADING_SHIFT) -
     (_read_signed_track_heading(&(segment - 1)->heading) << VEHICLE_TRACK_HEADING_SHIFT));
  if(heading_change < 0)
    heading_change = -heading_change;
  g_VEHICLE_SPAWN_POSITION_STATE.track_heading_change = heading_change;
  if(heading_change > VEHICLE_ANGLE_HALF_TURN)
    heading_change = VEHICLE_ANGLE_FULL_TURN - heading_change;

  curvature =
    ((source_->track_speed_scale[tuning_mode] >> VEHICLE_FIXED8_SHIFT) *
     ((source_->track_speed_reference[tuning_mode] - heading_change) >> VEHICLE_FIXED8_SHIFT));
  curvature = ((curvature >> VEHICLE_TRACK_CURVATURE_SHIFT_PRIMARY) +
               (curvature >> VEHICLE_TRACK_CURVATURE_SHIFT_SECONDARY) +
               (curvature >> VEHICLE_TRACK_CURVATURE_SHIFT_TERTIARY));
  g_VEHICLE_SPAWN_POSITION_STATE.track_curvature = curvature;

  gap_index = source_->track_segment_index - active_state->track_segment_index;
  if(gap_index < 0)
    gap_index = -gap_index;
  gap_index /= VEHICLE_TRACK_GAP_DIVISOR;
  gap_index += VEHICLE_TRACK_GAP_BIAS;
  if(gap_index < 0)
    gap_index = 0;
  else if(gap_index > VEHICLE_TRACK_GAP_MAXIMUM)
    gap_index = VEHICLE_TRACK_GAP_MAXIMUM;
  g_VEHICLE_SPAWN_POSITION_STATE.track_gap_index = gap_index;

  speed_scale      = g_VEHICLE_SPAWN_CONFIGURATION.track_speed_scales[gap_index];
  scaled_curvature = ((curvature + VEHICLE_TRACK_CURVATURE_BIAS) >> VEHICLE_FIXED8_SHIFT);
  if(((active_state->speed_magnitude < VEHICLE_ACTIVE_SPEED_WARNING_LIMIT) &&
      (active_state->update_callback ==
       (SimulationUpdateCallback) update_driving_warning_vehicle_state)) ||
     ((source_->track_segment_index < active_state->track_segment_index) &&
      (active_state->update_callback == (SimulationUpdateCallback) update_player_vehicle_state) &&
      (speed_scale > VEHICLE_FIXED_ONE)))
    speed_scale = VEHICLE_FIXED_ONE;

  if(speed_scale < VEHICLE_FIXED_ONE)
    {
      result =
        ((source_->track_speed_response[tuning_mode] >> VEHICLE_FIXED8_SHIFT) *
         (((speed_scale >> VEHICLE_FIXED8_SHIFT) * scaled_curvature) >> VEHICLE_FIXED8_SHIFT));
    }
  else
    {
      result = (scaled_curvature * (speed_scale >> VEHICLE_FIXED8_SHIFT));
    }
  result = ((result >> VEHICLE_FIXED8_SHIFT) * (base_speed >> VEHICLE_FIXED8_SHIFT));
  if(source_->track_segment_index <= g_GAME_RUNTIME_STATE.track_segment_count)
    return result;

  fade = ((VEHICLE_TRACK_GAP_MAXIMUM -
           (source_->track_segment_index - g_GAME_RUNTIME_STATE.track_segment_count)) *
          VEHICLE_TRACK_END_FADE_SCALE);
  if(fade < 0)
    fade = 0;
  return ((fade >> VEHICLE_FIXED8_SHIFT) * (result >> VEHICLE_FIXED8_SHIFT));
}

COMPILE_TIME_SIZE_ASSERT(VehicleRefreshStateBytes,
                         sizeof(VehicleSimulationRefreshState) == VEHICLE_REFRESH_STATE_BYTES);
COMPILE_TIME_SIZE_ASSERT(VehicleRuntimeRecordBytes,
                         sizeof(VehicleRuntimeRecord) == VEHICLE_RUNTIME_RECORD_BYTES);

void
initialize_vehicle_simulation_position(SimulationSource *source_,
                                       int32             track_segment_index_,
                                       int32             placement_mode_,
                                       int32             position_offset_,
                                       int32             activate_record_)
{
  VehicleSimulationRefreshState *refresh_state;
  VehicleRuntimeRecord          *record;
  VehicleSimulationState        *player_state;
  Vector3                        position_delta;
  int32                          active_track_segment;
  int32                          original_segment_index;
  int32                          adjusted_segment_index;

  refresh_state          = (VehicleSimulationRefreshState *) source_->refresh_state;
  active_track_segment   = G_ACTIVE_SIMULATION_SOURCE->simulation_source.track_segment_index;
  original_segment_index = source_->track_segment_index;
  if(track_segment_index_ < 0)
    track_segment_index_ = 0;

  randomize_vehicle_refresh_speed(source_);
  randomize_vehicle_steering_offset(source_);
  refresh_state->pending_state        = 0;
  refresh_state->activation_countdown = 0;
  refresh_state->control_state        = 0;
  refresh_state->activation_period    = VEHICLE_REFRESH_PERIOD;
  source_->direction.y             = 0;
  source_->heading_rotation        = 0;
  source_->heading_velocity        = 0;

  if(((source_->status_flags & VEHICLE_STATUS_TRAFFIC_FLAG) != 0) &&
     (G_FRONTEND_EXIT_REQUESTED != 1) &&
     (g_SIMULATION_RENDER_STATE.requested_mode == VEHICLE_SIMULATION_RENDER_MODE))
    {
      g_SIMULATION_RENDER_STATE.requested_mode = VEHICLE_SIMULATION_DEFAULT_MODE;
      player_state = g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_PLAYER];
      initialize_simulation_state_from_source(&g_SIMULATION_RENDER_STATE,
                                              &player_state->simulation_source,
                                              VEHICLE_SIMULATION_DEFAULT_MODE);
    }

  source_->status_flags &= ~VEHICLE_STATUS_RESET_MASK;
  if((source_->status_flags & VEHICLE_STATUS_REPOSITION_FLAG) != 0)
    {
      source_->status_flags &= ~VEHICLE_STATUS_REPOSITION_FLAG;
      source_->status_flags |= VEHICLE_STATUS_ACTIVE_FLAG;
    }
  else if((((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) != 0) &&
           (source_->track_segment_index < active_track_segment)) ||
          (((source_->status_flags & VEHICLE_STATUS_TRAFFIC_FLAG) != 0) &&
           ((g_TRACK_SEGMENTS[original_segment_index].lane_boundary_offsets &
             VEHICLE_TRACK_SPECIAL_MASK) == 0)))
    {
      if((source_->status_flags & VEHICLE_STATUS_TRAFFIC_FLAG) != 0)
        g_VEHICLE_COLLISION_UPDATE_PENDING = 0;
    }
  else if((source_->status_flags & VEHICLE_STATUS_TRAFFIC_FLAG) != 0)
    g_VEHICLE_COLLISION_UPDATE_PENDING = 0;

  if(((source_->status_flags & VEHICLE_STATUS_TRAFFIC_FLAG) != 0) &&
     ((source_->status_flags & VEHICLE_STATUS_ACTIVE_FLAG) == 0))
    {
      record = (VehicleRuntimeRecord *) source_->runtime_record;
      record->status_flags &= ~VEHICLE_RUNTIME_ACTIVE_FLAG;
    }

  if((g_TRACK_SEGMENTS[original_segment_index].lane_boundary_offsets &
      VEHICLE_TRACK_SPECIAL_MASK) == 0)
    placement_mode_ = 1;

  _insert_vehicle_record_by_track_position(source_, &track_segment_index_);
  _select_vehicle_track_speed(source_, placement_mode_);
  source_->track_segment_index   = track_segment_index_;
  source_->track_position_offset = position_offset_;
  rebuild_vehicle_track_frame(source_);

  source_->position = source_->track_reference_position;
  source_->position.z +=
    ((source_->position.z ^ G_ACTIVE_SIMULATION_SOURCE->simulation_source.position.z) &
     VEHICLE_LOW_WORD_MASK);
  position_delta.x = ((source_->track_basis.m00 >> VEHICLE_FIXED8_SHIFT) *
                      (position_offset_ >> VEHICLE_FIXED8_SHIFT));
  position_delta.y = ((source_->track_basis.m01 >> VEHICLE_FIXED8_SHIFT) *
                      (position_offset_ >> VEHICLE_FIXED8_SHIFT));
  position_delta.z = ((source_->track_basis.m02 >> VEHICLE_FIXED8_SHIFT) *
                      (position_offset_ >> VEHICLE_FIXED8_SHIFT));
  source_->position.x += position_delta.x;
  source_->position.y += position_delta.y;
  source_->position.z += position_delta.z;

  adjusted_segment_index = source_->track_segment_index;
  if(adjust_track_segment_index(&source_->position, &adjusted_segment_index) != 0)
    {
      source_->track_segment_index = adjusted_segment_index;
      rebuild_vehicle_track_frame(source_);
    }

  _copy_vehicle_track_transform(source_);
  refresh_state->control_state = 0;
  randomize_vehicle_refresh_speed(source_);
  if(activate_record_ != 0)
    _activate_vehicle_record(source_);
}

static void
_insert_vehicle_record_by_track_position(SimulationSource *source_, int32 *track_segment_index_)
{
  VehicleRuntimeRecord *record;
  VehicleRuntimeRecord *current;
  DoublyLinkedNode     *node;

  record = (VehicleRuntimeRecord *) source_->runtime_record;
  remove_list_node(&record->links);
  node    = g_VEHICLE_RUNTIME_POOL_STATE.active_record_list->list.head_sentinel->next;
  current = (VehicleRuntimeRecord *) node;
  while((node->next != NULL) && (current->track_segment_index <= *track_segment_index_))
    {
      if((current->track_segment_index == *track_segment_index_) &&
         ((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) == 0))
        *track_segment_index_ += 1;
      node    = node->next;
      current = (VehicleRuntimeRecord *) node;
    }

  insert_list_node_before(node, &record->links);
}

static void
_select_vehicle_track_speed(SimulationSource *source_, int32 placement_mode_)
{
  int32 track_speed;

  if((placement_mode_ == 0) && ((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) == 0))
    {
      source_->minimum_track_speed = VEHICLE_REVERSE_TRACK_SPEED;
      source_->target_track_speed  = VEHICLE_REVERSE_TRACK_SPEED;
      source_->status_flags &= ~VEHICLE_STATUS_SPEED_RESET_MASK;
      source_->status_flags |= VEHICLE_STATUS_REVERSE_FLAG;
      return;
    }

  (void) calculate_vehicle_track_speed(source_);
  track_speed = (g_VEHICLE_SPAWN_CONFIGURATION.speed_average >> 1) +
                (G_ACTIVE_SIMULATION_SOURCE->simulation_source.minimum_track_speed >> 1);
  source_->target_track_speed = randomize_vehicle_track_speed(source_, track_speed);
  if((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) != 0)
    source_->minimum_track_speed = 0;
  else if((source_->status_flags & VEHICLE_TRACK_SPEED_OVERRIDE_MASK) != 0)
    {
      source_->minimum_track_speed =
        G_ACTIVE_SIMULATION_SOURCE->simulation_source.minimum_track_speed;
      source_->target_track_speed = source_->minimum_track_speed;
    }
  else
    source_->minimum_track_speed = source_->target_track_speed;

  source_->status_flags &= ~VEHICLE_STATUS_SPEED_RESET_MASK;
}

static void
_copy_vehicle_track_transform(SimulationSource *source_)
{
  Matrix3x3 rotation;

  if((source_->status_flags & VEHICLE_STATUS_REVERSE_FLAG) == 0)
    source_->track_transform = source_->track_basis;
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

  if(source_->heading_rotation == 0)
    return;

  initialize_matrix3x3_rotation_y(&rotation, source_->heading_rotation);
  MulMat33Mat33_F16((frac16(*)[VEHICLE_TRACK_MATRIX_DIMENSION]) & source_->track_transform,
                    (frac16(*)[VEHICLE_TRACK_MATRIX_DIMENSION]) & rotation,
                    (frac16(*)[VEHICLE_TRACK_MATRIX_DIMENSION]) & source_->track_transform);
}

static void
_activate_vehicle_record(SimulationSource *source_)
{
  VehicleRuntimeRecord          *record;
  VehicleSimulationRefreshState *refresh_state;
  uint32                         product;
  uint32                         random_value;

  if(((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) != 0) &&
     (g_OPPONENT_VEHICLE_INDEX != VEHICLE_SIMULATION_DISABLED_MODE))
    return;
  if((source_->status_flags & VEHICLE_STATUS_TRAFFIC_FLAG) != 0)
    return;

  record = (VehicleRuntimeRecord *) source_->runtime_record;
  if((VehicleSimulationState *) record == G_ACTIVE_SIMULATION_SOURCE)
    return;

  refresh_state = (VehicleSimulationRefreshState *) source_->refresh_state;
  product       = (g_VEHICLE_SPAWN_RANDOM_STATE.low_word * g_VEHICLE_SPAWN_RANDOM_STATE.multiplier);
  g_VEHICLE_SPAWN_RANDOM_STATE.product  = product;
  g_VEHICLE_SPAWN_RANDOM_STATE.low_word = (product & VEHICLE_LOW_WORD_MASK);
  random_value = ((product & VEHICLE_RANDOM_VALUE_MASK) >> VEHICLE_RANDOM_VALUE_SHIFT);
  refresh_state->activation_countdown =
    (((g_VEHICLE_SPAWN_CONFIGURATION.activation_range * random_value) +
      VEHICLE_RANDOM_ROUNDING_VALUE) >>
     VEHICLE_RANDOM_SCALE_SHIFT) +
    1;
  record->update_callback = update_traffic_vehicle_simulation;
  if((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) == 0)
    g_VEHICLE_SPAWN_POSITION_STATE.active_vehicle_count += 1;
}

static uint32
_next_vehicle_spawn_random_value(void)
{
  uint32 product;

  product = (g_VEHICLE_SPAWN_RANDOM_STATE.low_word * g_VEHICLE_SPAWN_RANDOM_STATE.multiplier);
  g_VEHICLE_SPAWN_RANDOM_STATE.product  = product;
  g_VEHICLE_SPAWN_RANDOM_STATE.low_word = (product & VEHICLE_LOW_WORD_MASK);
  return ((product & VEHICLE_RANDOM_VALUE_MASK) >> VEHICLE_RANDOM_VALUE_SHIFT);
}

static int32
_read_signed_track_heading(const int16 *heading_)
{
  const uint8 *bytes;

  bytes = (const uint8 *) heading_;
  return (int32) (int16) ((((uint32) bytes[0]) << VEHICLE_TRACK_COMPONENT_BYTE_SHIFT) |
                          (uint32) bytes[1]);
}
