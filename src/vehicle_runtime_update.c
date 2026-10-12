#include "driving_warning_display.h"
#include "frontend_transition.h"
#include "game_audio_system.h"
#include "game_bootstrap.h"
#include "game_frame_count.h"
#include "geometry.h"
#include "player_vehicle_state.h"
#include "render_thread_signal.h"
#include "road_data.h"
#include "simulation_snapshot.h"
#include "simulation_thread_signal.h"
#include "sound_spatial.h"
#include "track_position.h"
#include "vbl_delay.h"
#include "vehicle_drive_statistics.h"
#include "vehicle_dynamics_state.h"
#include "vehicle_pair_collision.h"
#include "vehicle_parameter_loader.h"
#include "vehicle_render_resource_selection.h"
#include "vehicle_render_snapshot.h"
#include "vehicle_runtime_pool.h"
#include "vehicle_simulation_state.h"

#include "operamath.h"

#include "stddef.h"


/* This linker-visible entry point has no direct C caller. */
void
clamp_vehicle_heading_to_reference_direction(SimulationSource *source_);

#define VEHICLE_RUNTIME_RENDER_MODE_COMPLETE 4
#define VEHICLE_RUNTIME_RENDER_MODE_BLOCKED 2
#define VEHICLE_RUNTIME_RENDER_MODE_AUDIO_DISABLED 3
#define VEHICLE_RUNTIME_RENDER_FLAG_UPDATE 1
#define VEHICLE_RUNTIME_RENDER_FLAG_CALLBACKS 4
#define VEHICLE_RUNTIME_RECORD_ACTIVE_FLAG 4
#define VEHICLE_RUNTIME_AUDIO_STATE_TIMER_FLAG 2
#define VEHICLE_RUNTIME_CALLBACK_ADDRESS_MASK 0xFFFFFFFCUL
#define VEHICLE_RUNTIME_PLAYER_MOTION_LIMIT 12
#define VEHICLE_RUNTIME_PLAYER_TRACK_LIMIT 7
#define VEHICLE_RUNTIME_PLAYER_LATERAL_LIMIT 0x8000
#define VEHICLE_RUNTIME_GAME_STATE_FLAG 0x10
#define VEHICLE_RUNTIME_GAME_STATE_SCALE 0x1E0000
#define VEHICLE_RUNTIME_AUDIO_UPDATE_LIMIT 3
#define VEHICLE_RUNTIME_WAIT_LIMIT 30
#define VEHICLE_RUNTIME_WAIT_COUNT 1
#define VEHICLE_RUNTIME_AUDIO_DISTANCE_LIMIT 0x96000
#define VEHICLE_RUNTIME_AUDIO_MOTION_BIAS 0x1BCCCC
#define VEHICLE_RUNTIME_AUDIO_MOTION_SCALE 0x1BCCCC00
#define VEHICLE_RUNTIME_AUDIO_RELATIVE_DISTANCE_LIMIT 0x180000
#define VEHICLE_RUNTIME_AUDIO_DISTANCE_MINOR_SHIFT 2
#define VEHICLE_RUNTIME_AUDIO_SELECTION_DISTANCE 0x230000
#define VEHICLE_RUNTIME_COLLISION_DISTANCE 0x400000
#define VEHICLE_RUNTIME_COLLISION_GAIN_DISTANCE 0x1000
#define VEHICLE_RUNTIME_COLLISION_FREQUENCY 0x2300
#define VEHICLE_RUNTIME_COLLISION_TRACK_GAP 2
#define VEHICLE_RUNTIME_COLLISION_CONTACT_LIMIT 100
#define VEHICLE_RUNTIME_COLLISION_SOUND_PRIMARY 0x66
#define VEHICLE_RUNTIME_COLLISION_SOUND_SECONDARY 0x67
#define VEHICLE_RUNTIME_ENGINE_SOUND_PRIMARY 0x64
#define VEHICLE_RUNTIME_ENGINE_SOUND_SECONDARY 0x65
#define VEHICLE_RUNTIME_COLLISION_SOUND_CLASS 0x0C
#define VEHICLE_RUNTIME_ENGINE_SOUND_CLASS 0x13
#define VEHICLE_RUNTIME_PLAYER_ENGINE_SOUND_CLASS 0x17
#define VEHICLE_RUNTIME_PLAYER_ENGINE_GAIN_MINIMUM 0x2B
#define VEHICLE_RUNTIME_DEFAULT_ENGINE_FREQUENCY 0x3FFF
#define VEHICLE_RUNTIME_AUDIO_FREQUENCY_TABLE_BASE 5
#define VEHICLE_RUNTIME_AUDIO_FREQUENCY_TABLE_STRIDE 2
#define VEHICLE_RUNTIME_TRAFFIC_AUDIO_FLAG 8U
#define VEHICLE_RUNTIME_COLLISION_STATUS_FLAGS 0x000C0000U
#define VEHICLE_RUNTIME_WARNING_TIMER_PHASE_FLAG 4
#define VEHICLE_RUNTIME_PROXIMITY_TRACK_RANGE 64
#define VEHICLE_RUNTIME_PROXIMITY_RATE_SHIFT 3
#define VEHICLE_RUNTIME_PROXIMITY_RATE_MAXIMUM 6
#define VEHICLE_RUNTIME_PROXIMITY_REAR_RATE_DIVISOR 2
#define VEHICLE_RUNTIME_AUDIO_MODE_RESET (-99)
#define VEHICLE_RUNTIME_FIXED16_SHIFT 16
#define VEHICLE_RUNTIME_FIXED8_SHIFT 8
#define VEHICLE_RUNTIME_GAIN_PRODUCT_SHIFT 7
#define VEHICLE_RUNTIME_PRIMARY_AMPLITUDE_SHIFT 2
#define VEHICLE_RUNTIME_SECONDARY_AMPLITUDE_SHIFT 1
#define VEHICLE_RUNTIME_COLLISION_GAIN_SHIFT 12
#define VEHICLE_RUNTIME_COLLISION_GAIN_MAXIMUM 0x7FFF
#define VEHICLE_RUNTIME_COLLISION_ROUNDING_BIAS 0xFF
#define VEHICLE_RUNTIME_COLLISION_FREQUENCY_STEP 100
#define VEHICLE_RUNTIME_SECONDARY_DISTANCE_BIAS 8
#define VEHICLE_RUNTIME_SOUND_CONFIGURATION_SHIFT 8
#define VEHICLE_RUNTIME_SOUND_VARIANT_MASK 0xFF
#define VEHICLE_RUNTIME_SORT_KEY_OFFSET 0x50
#define VEHICLE_RUNTIME_SORT_RECORD_BYTES 0x3E4
#define VEHICLE_RUNTIME_CALLBACK_SOURCE_OFFSET 0x0C

typedef void (*VehicleRuntimeRecordCallback)(void *record_);

typedef struct VehicleRuntimeFrameRecord
{
  DoublyLinkedNode links;
  uint8            reserved_008[4];
  int32            audio_position_x;
  uint8            reserved_010[4];
  int32            audio_position_z;
  uint8            reserved_018[0x0C];
  int32            player_render_scale;
  uint8            reserved_028[0x28];
  int32            track_position;
  uint8            reserved_054[0x1C];
  int32            lateral_position;
  uint8            reserved_074[0x114];
  uint32           status_flags;
  uint8            reserved_18_c[0x1C0];
  int32            engine_frequency_limit;
  uint8            reserved_350[0x98];
  int32            audio_distance;
  uint8            reserved_3_ec[8];
  int32            audio_render_mode;
  int32            special_render_value;
  uint8            reserved_3_fc[0x0C];
  int32            audio_engine_speed;
  uint8            reserved_40_c[0x28];
  int32            warning_mode;
  int32            reserved_438;
  int32            warning_pending;
  uint8            reserved_440[0x10];
  int32            spatial_sound_gain;
  uint32           spatial_sound_configuration;
  uint8            reserved_458[0x19];
  uint8            audio_state_flags;
  uint8            reserved_472[0x3E];
  void            *vehicle_specification;
  uint8            reserved_4_b4[0x0C];
  void            *state_update_callback;
  void            *render_update_callback;
  int32            slot_index;
  uint32           runtime_flags;
} VehicleRuntimeFrameRecord;

typedef char VehicleRuntimeFrameRecordStateCallbackOffset
  [(offsetof(VehicleRuntimeFrameRecord, state_update_callback) == 0x4C0) ? 1 : -1];
typedef char VehicleRuntimeFrameRecordRenderCallbackOffset
  [(offsetof(VehicleRuntimeFrameRecord, render_update_callback) == 0x4C4) ? 1 : -1];
typedef char VehicleRuntimeFrameRecordFlagsOffset
  [(offsetof(VehicleRuntimeFrameRecord, runtime_flags) == 0x4CC) ? 1 : -1];
typedef char VehicleRuntimeFrameRecordStatusFlagsOffset
  [(offsetof(VehicleRuntimeFrameRecord, status_flags) == 0x188) ? 1 : -1];
typedef char VehicleRuntimeFrameRecordEngineLimitOffset
  [(offsetof(VehicleRuntimeFrameRecord, engine_frequency_limit) == 0x34C) ? 1 : -1];
typedef char VehicleRuntimeFrameRecordEngineSpeedOffset
  [(offsetof(VehicleRuntimeFrameRecord, audio_engine_speed) == 0x408) ? 1 : -1];
typedef char VehicleRuntimeFrameRecordSoundGainOffset
  [(offsetof(VehicleRuntimeFrameRecord, spatial_sound_gain) == 0x450) ? 1 : -1];
typedef char VehicleRuntimeFrameRecordSoundConfigurationOffset
  [(offsetof(VehicleRuntimeFrameRecord, spatial_sound_configuration) == 0x454) ? 1 : -1];
typedef char VehicleRuntimeFrameRecordAudioDistanceOffset
  [(offsetof(VehicleRuntimeFrameRecord, audio_distance) == 0x3E8) ? 1 : -1];
typedef char VehicleRuntimeFrameRecordAudioRenderModeOffset
  [(offsetof(VehicleRuntimeFrameRecord, audio_render_mode) == 0x3F4) ? 1 : -1];
/* The retail coordinator resets this shared handoff counter after 30 waits. */
int32 g_VEHICLE_RUNTIME_FRAME_DELAY_COUNTER;

int32                      g_VEHICLE_COLLISION_SOUND_PENDING;
int32                      g_VEHICLE_COLLISION_SOUND_ACTIVE;
int32                      g_VEHICLE_COLLISION_SECONDARY_COUNTDOWN = 1;
int32                      g_VEHICLE_COLLISION_TRACK_POSITION;
int32                      g_VEHICLE_COLLISION_CONTACT_FRAMES;
VehicleRuntimeFrameRecord *g_PRIMARY_VEHICLE_AUDIO_RECORD;
VehicleRuntimeFrameRecord *g_SECONDARY_VEHICLE_AUDIO_RECORD;
int32                      g_VEHICLE_PROXIMITY_ALERT_PHASE;
int32                      g_VEHICLE_RECOVERY_STEERING_MODE;
int32                      g_VEHICLE_RECOVERY_STEERING_PROJECTION;
int32                      g_VEHICLE_RECOVERY_STEERING_FRAME_COUNT;
int32                      g_VEHICLE_RECOVERY_STEERING_ANGLE;

#define READ_SIGNED_BIG_ENDIAN_INT16(value_)                                                       \
  ((int32) (int16) ((((uint32) ((const uint8 *) (value_))[0]) << 8) |                              \
                    (uint32) ((const uint8 *) (value_))[1]))
#define VEHICLE_RUNTIME_TRACK_HEADING_SHIFT 10
#define VEHICLE_RUNTIME_RECOVERY_STATE_PENDING 2
#define VEHICLE_RUNTIME_RECOVERY_STATE_COMPLETE 3
#define VEHICLE_RUNTIME_RECOVERY_TRACK_SCALE 0x60000
#define VEHICLE_RUNTIME_RECOVERY_SOUND_ID 100
#define VEHICLE_RUNTIME_RECOVERY_SIMULATION_MODE 10
#define VEHICLE_RUNTIME_RECOVERY_MOTION_LIMIT 0x1999
#define VEHICLE_RUNTIME_RECOVERY_FLAG 1
#define VEHICLE_RUNTIME_HEADING_QUARTER_TURN 0x400000
#define VEHICLE_RUNTIME_HEADING_HALF_TURN 0x800000
#define VEHICLE_RUNTIME_HEADING_FULL_TURN 0x1000000
#define VEHICLE_RUNTIME_HEADING_ALIGNMENT_LIMIT 0x80000
#define VEHICLE_RUNTIME_STEERING_MODE_SEARCH 1
#define VEHICLE_RUNTIME_STEERING_MODE_ALIGN 2
#define VEHICLE_RUNTIME_STEERING_ANGLE_STEP 0x30000
#define VEHICLE_RUNTIME_STEERING_ANGLE_LIMIT 0x190000
#define VEHICLE_RUNTIME_STEERING_SEARCH_FRAMES 13
#define VEHICLE_RUNTIME_STEERING_BOUNDARY_SCALE 0x2000
#define VEHICLE_RUNTIME_RECOVERY_BRAKE_VALUE 250
#define VEHICLE_RUNTIME_RECOVERY_FRAME_MINIMUM 100
#define VEHICLE_RUNTIME_MOTION_DAMPING_SHIFT 3
#define VEHICLE_RUNTIME_MOTION_DAMPING_BIAS 7
#define VEHICLE_RUNTIME_PROJECTION_COMPONENT_SHIFT 8
#define VEHICLE_RUNTIME_REFERENCE_RECORD_BYTES 0x90
#define VEHICLE_RUNTIME_REFERENCE_HEADING_HIGH_OFFSET 0x18
#define VEHICLE_RUNTIME_REFERENCE_HEADING_LOW_OFFSET 0x19
#define VEHICLE_RUNTIME_REFERENCE_HEADING_BYTE_SHIFT 8
#define VEHICLE_RUNTIME_REFERENCE_HEADING_LIMIT 0x300000

void
clamp_vehicle_heading_to_reference_direction(SimulationSource *source_)
{
  const uint8 *reference_record;
  int32        reference_heading;
  int32        heading_difference;

  reference_record = (const uint8 *) g_REFERENCE_DISTANCE_RECORDS +
                     (source_->track_segment_index * VEHICLE_RUNTIME_REFERENCE_RECORD_BYTES);
  reference_heading =
    (int32) (int16) (((uint32) reference_record[VEHICLE_RUNTIME_REFERENCE_HEADING_HIGH_OFFSET]
                      << VEHICLE_RUNTIME_REFERENCE_HEADING_BYTE_SHIFT) |
                     (uint32) reference_record[VEHICLE_RUNTIME_REFERENCE_HEADING_LOW_OFFSET]);

  heading_difference = source_->heading_rotation - reference_heading;
  if(heading_difference > VEHICLE_RUNTIME_HEADING_HALF_TURN)
    heading_difference -= VEHICLE_RUNTIME_HEADING_FULL_TURN;
  else if(heading_difference < -VEHICLE_RUNTIME_HEADING_HALF_TURN)
    heading_difference += VEHICLE_RUNTIME_HEADING_FULL_TURN;

  if(heading_difference > VEHICLE_RUNTIME_REFERENCE_HEADING_LIMIT)
    {
      source_->heading_rotation = reference_heading + VEHICLE_RUNTIME_REFERENCE_HEADING_LIMIT;
    }
  else if(heading_difference < -VEHICLE_RUNTIME_REFERENCE_HEADING_LIMIT)
    {
      source_->heading_rotation = reference_heading - VEHICLE_RUNTIME_REFERENCE_HEADING_LIMIT;
    }

  if(source_->heading_rotation < 0)
    source_->heading_rotation += VEHICLE_RUNTIME_HEADING_FULL_TURN;
  else if(source_->heading_rotation > VEHICLE_RUNTIME_HEADING_FULL_TURN)
    source_->heading_rotation -= VEHICLE_RUNTIME_HEADING_FULL_TURN;
}

static void
update_vehicle_runtime_track_offset(SimulationSource *source_)
{
  const TrackSegment *segment;
  int32               heading;
  int32               lateral_offset;
  int32               sine;

  segment = &g_TRACK_SEGMENTS[source_->track_segment_index];
  heading = READ_SIGNED_BIG_ENDIAN_INT16(&segment->heading) << VEHICLE_RUNTIME_TRACK_HEADING_SHIFT;
  sine    = SinF16(heading);
  lateral_offset = -MulSF16(sine, segment->surface_reference.x - source_->position.x);
  lateral_offset -= MulSF16(CosF16(heading), segment->surface_reference.z - source_->position.z);
  source_->track_interpolation = lateral_offset;
}

void
update_vehicle_runtime_recovery_steering(void)
{
  VehicleSimulationState     *player;
  SimulationSource           *source;
  const TrackSegment         *segment;
  const uint8                *segment_bytes;
  VehicleGearShiftStatistics *statistics;
  int32                       track_heading;
  int32                       heading_difference;
  int32                       projection;
  int32                       negative_distance;
  int32                       positive_distance;
  int32                       value;

  player             = (VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE;
  source             = &player->simulation_source;
  segment            = &g_TRACK_SEGMENTS[source->track_segment_index];
  segment_bytes      = (const uint8 *) segment;
  statistics         = g_VEHICLE_GEAR_SHIFT_STATISTICS;
  track_heading      = READ_SIGNED_BIG_ENDIAN_INT16(&segment_bytes[offsetof(TrackSegment, heading)])
                    << VEHICLE_RUNTIME_TRACK_HEADING_SHIFT;
  heading_difference = track_heading - source->heading_rotation;
  if(heading_difference < 0)
    heading_difference = -heading_difference;
  if(heading_difference > VEHICLE_RUNTIME_HEADING_HALF_TURN)
    {
      heading_difference = VEHICLE_RUNTIME_HEADING_FULL_TURN - heading_difference;
    }
  heading_difference -= VEHICLE_RUNTIME_HEADING_QUARTER_TURN;
  if(heading_difference < 0)
    heading_difference = -heading_difference;

  if((heading_difference < VEHICLE_RUNTIME_HEADING_ALIGNMENT_LIMIT) ||
     (g_VEHICLE_RECOVERY_STEERING_MODE == VEHICLE_RUNTIME_STEERING_MODE_ALIGN))
    {
      g_ANTI_LOCK_BRAKES_ENABLED      = 0;
      source->warning_display_state   = 0;
      source->traffic_brake_indicator = VEHICLE_RUNTIME_RECOVERY_BRAKE_VALUE;
      if((statistics->first_recovery_track_position < VEHICLE_RUNTIME_RECOVERY_FRAME_MINIMUM) ||
         (statistics->recovery_frame_duration < statistics->first_recovery_track_position) ||
         (g_OPPONENT_VEHICLE_INDEX == VEHICLE_SIMULATION_DISABLED_MODE))
        {
          value = source->longitudinal_axle_span;
          source->longitudinal_axle_span -=
            (value + ((value < 0) ? VEHICLE_RUNTIME_MOTION_DAMPING_BIAS : 0)) >>
            VEHICLE_RUNTIME_MOTION_DAMPING_SHIFT;
          source->pitch_motion_scale = 0;
        }

      if(source->longitudinal_acceleration <= 0)
        {
          if(g_VEHICLE_RECOVERY_STEERING_ANGLE < VEHICLE_RUNTIME_STEERING_ANGLE_LIMIT)
            {
              g_VEHICLE_RECOVERY_STEERING_ANGLE += VEHICLE_RUNTIME_STEERING_ANGLE_STEP;
            }
        }
      else if(g_VEHICLE_RECOVERY_STEERING_ANGLE > -VEHICLE_RUNTIME_STEERING_ANGLE_LIMIT)
        {
          g_VEHICLE_RECOVERY_STEERING_ANGLE -= VEHICLE_RUNTIME_STEERING_ANGLE_STEP;
        }
      source->heading_rotation         = g_VEHICLE_RECOVERY_STEERING_ANGLE;
      g_VEHICLE_RECOVERY_STEERING_MODE = VEHICLE_RUNTIME_STEERING_MODE_ALIGN;
    }
  else
    {
      if(g_VEHICLE_RECOVERY_STEERING_MODE == 0)
        {
          g_VEHICLE_RECOVERY_STEERING_FRAME_COUNT = 0;
          g_VEHICLE_RECOVERY_STEERING_ANGLE       = 0;
          projection = (((segment->surface_reference.z - source->position.z) >>
                         VEHICLE_RUNTIME_PROJECTION_COMPONENT_SHIFT) *
                        (SinF16(track_heading) >> VEHICLE_RUNTIME_PROJECTION_COMPONENT_SHIFT)) -
                       (((segment->surface_reference.x - source->position.x) >>
                         VEHICLE_RUNTIME_PROJECTION_COMPONENT_SHIFT) *
                        (CosF16(track_heading) >> VEHICLE_RUNTIME_PROJECTION_COMPONENT_SHIFT));
          g_VEHICLE_RECOVERY_STEERING_PROJECTION = projection;
        }

      g_VEHICLE_RECOVERY_STEERING_FRAME_COUNT++;
      negative_distance =
        g_VEHICLE_RECOVERY_STEERING_PROJECTION +
        (int32) segment->negative_boundary_width * VEHICLE_RUNTIME_STEERING_BOUNDARY_SCALE;
      if(negative_distance < 0)
        negative_distance = -negative_distance;
      positive_distance =
        g_VEHICLE_RECOVERY_STEERING_PROJECTION -
        (int32) segment->positive_boundary_width * VEHICLE_RUNTIME_STEERING_BOUNDARY_SCALE;
      if(positive_distance < 0)
        positive_distance = -positive_distance;

      if(negative_distance < positive_distance)
        {
          if(g_VEHICLE_RECOVERY_STEERING_FRAME_COUNT < VEHICLE_RUNTIME_STEERING_SEARCH_FRAMES)
            source->heading_rotation = -VEHICLE_RUNTIME_STEERING_ANGLE_LIMIT;
          else
            source->heading_rotation = VEHICLE_RUNTIME_STEERING_ANGLE_LIMIT;
        }
      else if(g_VEHICLE_RECOVERY_STEERING_FRAME_COUNT < VEHICLE_RUNTIME_STEERING_SEARCH_FRAMES)
        {
          source->heading_rotation = VEHICLE_RUNTIME_STEERING_ANGLE_LIMIT;
        }
      else
        {
          source->heading_rotation = -VEHICLE_RUNTIME_STEERING_ANGLE_LIMIT;
        }
      source->audio_pan_target         = 0;
      source->warning_display_state    = 1;
      g_VEHICLE_RECOVERY_STEERING_MODE = VEHICLE_RUNTIME_STEERING_MODE_SEARCH;
    }

  value = source->longitudinal_acceleration;
  source->longitudinal_acceleration -=
    (value + ((value < 0) ? VEHICLE_RUNTIME_MOTION_DAMPING_BIAS : 0)) >>
    VEHICLE_RUNTIME_MOTION_DAMPING_SHIFT;
  value = source->pitch_motion_scale;
  source->pitch_motion_scale -= (value + ((value < 0) ? VEHICLE_RUNTIME_MOTION_DAMPING_BIAS : 0)) >>
                                VEHICLE_RUNTIME_MOTION_DAMPING_SHIFT;
}

void
prepare_vehicle_runtime_render(void)
{
  VehicleSimulationState     *player;
  VehicleSimulationState     *opponent;
  VehicleGearShiftStatistics *statistics;
  int32                       motion;

  player     = (VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE;
  opponent   = (VehicleSimulationState *) G_ACTIVE_VEHICLE_RUNTIME_RECORD;
  statistics = g_VEHICLE_GEAR_SHIFT_STATISTICS;

  if(g_GAME_RUNTIME_STATE.track_segment_count < player->simulation_source.track_segment_index)
    {
      update_vehicle_runtime_recovery_steering();
      if((g_GAME_RUNTIME_STATE.result != VEHICLE_RUNTIME_RECOVERY_STATE_PENDING) &&
         (g_GAME_RUNTIME_STATE.result != VEHICLE_RUNTIME_RECOVERY_STATE_COMPLETE))
        {
          g_GAME_RUNTIME_STATE.result = VEHICLE_RUNTIME_RECOVERY_STATE_PENDING;
          g_GAME_RUNTIME_STATE.race_finish_elapsed_frames =
            G_GAME_FRAME_COUNTER - g_GAME_RUNTIME_STATE.race_start_frame;
          statistics->recovery_frame_duration = g_GAME_RUNTIME_STATE.race_finish_elapsed_frames;
          statistics->recovery_track_position =
            g_GAME_RUNTIME_STATE.track_segment_count * VEHICLE_RUNTIME_RECOVERY_TRACK_SCALE;
          statistics->latest_recovery_track_position =
            opponent->simulation_source.track_segment_index * VEHICLE_RUNTIME_RECOVERY_TRACK_SCALE;
          g_SIMULATION_RENDER_STATE.requested_mode = VEHICLE_RUNTIME_RECOVERY_SIMULATION_MODE;
          request_sound_effect(VEHICLE_RUNTIME_RECOVERY_SOUND_ID);
        }

      motion = player->simulation_source.pitch_motion_scale;
      if(motion < 0)
        motion = -motion;
      if(player->simulation_source.longitudinal_acceleration < 0)
        motion -= player->simulation_source.longitudinal_acceleration;
      else
        motion += player->simulation_source.longitudinal_acceleration;
      if((motion < VEHICLE_RUNTIME_RECOVERY_MOTION_LIMIT) &&
         (g_GAME_RUNTIME_STATE.result != VEHICLE_RUNTIME_RECOVERY_STATE_COMPLETE))
        {
          update_vehicle_drive_statistics_derived_values(
            (VehicleDriveStatisticsRecord *) statistics);
          g_GAME_RUNTIME_STATE.result   = VEHICLE_RUNTIME_RECOVERY_STATE_COMPLETE;
          g_GAME_RUNTIME_STATE.flags_14 = VEHICLE_RUNTIME_RECOVERY_FLAG;
          g_GAME_RUNTIME_STATE.flags_18 = VEHICLE_RUNTIME_RECOVERY_FLAG;
        }
    }
  else
    {
      g_VEHICLE_RECOVERY_STEERING_MODE = 0;
    }

  if(g_GAME_RUNTIME_STATE.track_segment_count < opponent->simulation_source.track_segment_index)
    {
      if(statistics->first_recovery_track_position == 0)
        {
          statistics->first_recovery_track_position =
            G_GAME_FRAME_COUNTER - g_GAME_RUNTIME_STATE.race_start_frame;
          statistics->latest_recovery_track_position =
            g_GAME_RUNTIME_STATE.track_segment_count * VEHICLE_RUNTIME_RECOVERY_TRACK_SCALE;
        }
    }
}

int32
calculate_vehicle_audio_motion_scale(VehicleRuntimeFrameRecord *record_)
{
  int32 x_distance;
  int32 z_distance;
  int32 distance;
  int32 distance_change;
  int32 absolute_distance_change;
  int32 result;

  z_distance = record_->audio_position_z - g_SIMULATION_RENDER_STATE.current_position.z;
  if(z_distance < 0)
    z_distance = -z_distance;
  x_distance = record_->audio_position_x - g_SIMULATION_RENDER_STATE.current_position.x;
  if(x_distance < 0)
    x_distance = -x_distance;

  if(x_distance < z_distance)
    distance = z_distance + (x_distance >> VEHICLE_RUNTIME_AUDIO_DISTANCE_MINOR_SHIFT);
  else
    distance = x_distance + (z_distance >> VEHICLE_RUNTIME_AUDIO_DISTANCE_MINOR_SHIFT);

  distance_change         = distance - record_->audio_distance;
  record_->audio_distance = distance;
  result                  = 0;
  if(record_->audio_render_mode == (int32) g_SIMULATION_RENDER_STATE.mode)
    {
      absolute_distance_change = distance_change;
      if(absolute_distance_change < 0)
        absolute_distance_change = -absolute_distance_change;
      if(absolute_distance_change < VEHICLE_RUNTIME_AUDIO_DISTANCE_LIMIT)
        {
          result = (VEHICLE_RUNTIME_AUDIO_MOTION_SCALE /
                    (distance_change + VEHICLE_RUNTIME_AUDIO_MOTION_BIAS));
        }
    }
  record_->audio_render_mode = (int32) g_SIMULATION_RENDER_STATE.mode;

  return result;
}

static int32
calculate_vehicle_audio_distance_scale(const ApproximateDistancePoint *record_)
{
  const ApproximateDistancePoint *player_record;
  int32                           x_distance;
  int32                           y_distance;
  int32                           distance;

  player_record =
    (const ApproximateDistancePoint *) ((const uint8 *) g_SIMULATION_RENDER_STATE.source -
                                        VEHICLE_RUNTIME_CALLBACK_SOURCE_OFFSET);
  y_distance = player_record->y - record_->y;
  if(y_distance < 0)
    y_distance = -y_distance;
  x_distance = player_record->x - record_->x;
  if(x_distance < 0)
    x_distance = -x_distance;

  if(x_distance < y_distance)
    distance = y_distance + (x_distance >> VEHICLE_RUNTIME_AUDIO_DISTANCE_MINOR_SHIFT);
  else
    distance = x_distance + (y_distance >> VEHICLE_RUNTIME_AUDIO_DISTANCE_MINOR_SHIFT);

  if(distance < VEHICLE_RUNTIME_AUDIO_RELATIVE_DISTANCE_LIMIT)
    {
      distance =
        calculate_clamped_approximate_distance_2d(record_,
                                                  VEHICLE_RUNTIME_AUDIO_RELATIVE_DISTANCE_LIMIT);
    }

  return distance;
}

static void
update_vehicle_runtime_audio_positions(DoublyLinkedListStorage *storage_)
{
  VehicleRuntimeFrameRecord *active_record;
  VehicleRuntimeFrameRecord *record;
  VehicleRuntimeFrameRecord *collision_record;
  VehicleRuntimeFrameRecord *primary_record;
  VehicleRuntimeFrameRecord *secondary_record;
  VehicleRuntimeFrameRecord *old_primary_record;
  VehicleRuntimeFrameRecord *old_secondary_record;
  DoublyLinkedNode          *node;
  const int32               *frequency_entry;
  int32                      primary_distance;
  int32                      secondary_distance;
  int32                      old_primary_distance;
  int32                      old_secondary_distance;
  int32                      x_distance;
  int32                      z_distance;
  int32                      distance;
  int32                      track_gap;
  int32                      pan;
  int32                      amplitude;
  int32                      frequency;
  int32                      motion_scale;
  int32                      distance_scale;
  int32                      proximity_event;
  uint32                     sound_configuration;

  active_record      = (VehicleRuntimeFrameRecord *) G_ACTIVE_SIMULATION_SOURCE;
  primary_distance   = VEHICLE_RUNTIME_AUDIO_SELECTION_DISTANCE;
  secondary_distance = VEHICLE_RUNTIME_AUDIO_SELECTION_DISTANCE;
  primary_record     = NULL;
  secondary_record   = NULL;
  collision_record   = NULL;
  proximity_event    = 0;

  if((uint8 *) active_record + VEHICLE_RUNTIME_CALLBACK_SOURCE_OFFSET !=
     (const uint8 *) g_SIMULATION_RENDER_STATE.position_source)
    {
      primary_distance = 0;
      primary_record   = (VehicleRuntimeFrameRecord *) ((const SimulationSource *)
                                                          g_SIMULATION_RENDER_STATE.position_source)
                           ->runtime_record;
    }

  node = storage_->list.head_sentinel->next;
  while(node->next != NULL)
    {
      record = (VehicleRuntimeFrameRecord *) node;
      if((record != active_record) &&
         ((record->runtime_flags & VEHICLE_RUNTIME_RECORD_ACTIVE_FLAG) != 0))
        {
          z_distance = record->audio_position_z - g_SIMULATION_RENDER_STATE.current_position.z;
          if(z_distance < 0)
            z_distance = -z_distance;
          x_distance = record->audio_position_x - g_SIMULATION_RENDER_STATE.current_position.x;
          if(x_distance < 0)
            x_distance = -x_distance;
          if(x_distance < z_distance)
            distance = z_distance + (x_distance >> VEHICLE_RUNTIME_AUDIO_DISTANCE_MINOR_SHIFT);
          else
            distance = x_distance + (z_distance >> VEHICLE_RUNTIME_AUDIO_DISTANCE_MINOR_SHIFT);

          if((record->status_flags & VEHICLE_RUNTIME_TRAFFIC_AUDIO_FLAG) != 0)
            {
              if((active_record->render_update_callback != (void *) update_player_vehicle_state) &&
                 (record->render_update_callback != (void *) update_player_vehicle_state))
                {
                  track_gap = record->track_position - active_record->track_position;
                  if(track_gap < 0)
                    track_gap = -track_gap;
                  g_VEHICLE_PROXIMITY_ALERT_RATE_INDEX =
                    (VEHICLE_RUNTIME_PROXIMITY_TRACK_RANGE - track_gap) /
                    (1 << VEHICLE_RUNTIME_PROXIMITY_RATE_SHIFT);
                  if(g_VEHICLE_PROXIMITY_ALERT_RATE_INDEX < 0)
                    g_VEHICLE_PROXIMITY_ALERT_RATE_INDEX = 0;
                  else if(g_VEHICLE_PROXIMITY_ALERT_RATE_INDEX >
                          VEHICLE_RUNTIME_PROXIMITY_RATE_MAXIMUM)
                    g_VEHICLE_PROXIMITY_ALERT_RATE_INDEX = VEHICLE_RUNTIME_PROXIMITY_RATE_MAXIMUM;
                  proximity_event = 1;
                }
              if((g_VEHICLE_PROXIMITY_ALERT_RATE_INDEX != 0) &&
                 (record->track_position <= active_record->track_position))
                {
                  g_VEHICLE_PROXIMITY_ALERT_RATE_INDEX /=
                    VEHICLE_RUNTIME_PROXIMITY_REAR_RATE_DIVISOR;
                }
              if(((record->status_flags & VEHICLE_RUNTIME_COLLISION_STATUS_FLAGS) != 0) &&
                 (distance < VEHICLE_RUNTIME_COLLISION_DISTANCE))
                {
                  g_TRAFFIC_VEHICLE_SOUND_COUNTDOWN = 0;
                  collision_record                  = record;
                }
            }

          if((g_STARTUP_CONFIGURATION.sound_effects_enabled != 0) &&
             (g_SECONDARY_ROAD_RENDER_CCB == NULL) &&
             (distance < VEHICLE_RUNTIME_AUDIO_SELECTION_DISTANCE))
            {
              old_primary_distance   = primary_distance;
              old_secondary_distance = secondary_distance;
              old_primary_record     = primary_record;
              old_secondary_record   = secondary_record;
              if(primary_distance < secondary_distance)
                {
                  if(distance < secondary_distance)
                    {
                      secondary_distance = distance;
                      secondary_record   = record;
                      if(record != g_SECONDARY_VEHICLE_AUDIO_RECORD)
                        {
                          primary_distance   = distance;
                          secondary_distance = old_secondary_distance;
                          primary_record     = record;
                          secondary_record   = old_secondary_record;
                          if(record != g_PRIMARY_VEHICLE_AUDIO_RECORD)
                            {
                              record->audio_render_mode = VEHICLE_RUNTIME_AUDIO_MODE_RESET;
                              primary_distance          = old_primary_distance;
                              secondary_distance        = distance;
                              primary_record            = old_primary_record;
                              secondary_record          = record;
                            }
                        }
                    }
                }
              else if(distance < primary_distance)
                {
                  secondary_distance = distance;
                  secondary_record   = record;
                  if(record != g_SECONDARY_VEHICLE_AUDIO_RECORD)
                    {
                      primary_distance   = distance;
                      secondary_distance = old_secondary_distance;
                      primary_record     = record;
                      secondary_record   = old_secondary_record;
                      if(record != g_PRIMARY_VEHICLE_AUDIO_RECORD)
                        {
                          record->audio_render_mode = VEHICLE_RUNTIME_AUDIO_MODE_RESET;
                        }
                    }
                }
            }
        }
      node = node->next;
    }

  if(proximity_event == 0)
    {
      g_VEHICLE_PROXIMITY_ALERT_RATE_INDEX = 0;
      g_VEHICLE_PROXIMITY_ALERT_PHASE      = 0;
    }
  else if((G_DRIVING_WARNING_TIMER_FLAGS & VEHICLE_RUNTIME_WARNING_TIMER_PHASE_FLAG) != 0)
    {
      g_VEHICLE_PROXIMITY_ALERT_PHASE = 1 - g_VEHICLE_PROXIMITY_ALERT_PHASE;
    }

  if((g_STARTUP_CONFIGURATION.sound_effects_enabled == 0) || (g_SECONDARY_ROAD_RENDER_CCB != NULL))
    {
      return;
    }

  g_SECONDARY_VEHICLE_SOUND_SUPPRESSED = 0;
  if(collision_record == NULL)
    {
      g_VEHICLE_COLLISION_SOUND_ACTIVE = 0;
    }
  else
    {
      calculate_spatial_sound_parameters((const Vector3 *) &collision_record->audio_position_x,
                                         &pan,
                                         &distance);
      if(distance < VEHICLE_RUNTIME_COLLISION_GAIN_DISTANCE)
        {
          amplitude = ((VEHICLE_RUNTIME_COLLISION_GAIN_DISTANCE - distance) *
                       VEHICLE_RUNTIME_COLLISION_GAIN_MAXIMUM) >>
                      VEHICLE_RUNTIME_COLLISION_GAIN_SHIFT;
        }
      else
        {
          amplitude = 0;
        }

      frequency    = VEHICLE_RUNTIME_COLLISION_FREQUENCY;
      motion_scale = calculate_vehicle_audio_motion_scale(collision_record);
      if(motion_scale != 0)
        {
          track_gap = g_VEHICLE_COLLISION_TRACK_POSITION - collision_record->track_position;
          if(track_gap < 0)
            track_gap = -track_gap;
          if(track_gap < VEHICLE_RUNTIME_COLLISION_TRACK_GAP)
            {
              frequency = motion_scale * VEHICLE_RUNTIME_COLLISION_FREQUENCY;
              if(frequency < 0)
                frequency += VEHICLE_RUNTIME_COLLISION_ROUNDING_BIAS;
              frequency >>= VEHICLE_RUNTIME_FIXED8_SHIFT;
              if(((VehicleRuntimeFrameRecord *)
                    g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_SIX_TARGET])
                   ->render_update_callback == (void *) update_vehicle_simulation_transform)
                {
                  g_VEHICLE_COLLISION_CONTACT_FRAMES = 0;
                }
              else
                {
                  g_VEHICLE_COLLISION_CONTACT_FRAMES++;
                  if(g_VEHICLE_COLLISION_CONTACT_FRAMES > VEHICLE_RUNTIME_COLLISION_CONTACT_LIMIT)
                    {
                      clear_vehicle_simulation_contact_flags(1);
                    }
                }
              motion_scale = VEHICLE_RUNTIME_COLLISION_CONTACT_LIMIT / frequency;
              if(motion_scale < g_VEHICLE_COLLISION_CONTACT_FRAMES)
                g_VEHICLE_COLLISION_CONTACT_FRAMES = motion_scale;
              update_dolby_spatial_sound(VEHICLE_RUNTIME_COLLISION_SOUND_PRIMARY,
                                         VEHICLE_RUNTIME_COLLISION_SOUND_CLASS,
                                         frequency - g_VEHICLE_COLLISION_CONTACT_FRAMES *
                                                       VEHICLE_RUNTIME_COLLISION_FREQUENCY_STEP,
                                         amplitude,
                                         pan >> VEHICLE_RUNTIME_FIXED16_SHIFT);
              g_VEHICLE_COLLISION_SOUND_ACTIVE  = 1;
              g_VEHICLE_COLLISION_SOUND_PENDING = 1;
            }
        }

      if(g_VEHICLE_SURFACE_INDEX == 0)
        {
          g_SECONDARY_VEHICLE_SOUND_SUPPRESSED = 0;
        }
      else if(g_VEHICLE_COLLISION_SECONDARY_COUNTDOWN == 0)
        {
          update_dolby_spatial_sound(VEHICLE_RUNTIME_COLLISION_SOUND_SECONDARY,
                                     VEHICLE_RUNTIME_COLLISION_SOUND_CLASS,
                                     frequency - g_VEHICLE_COLLISION_CONTACT_FRAMES *
                                                   VEHICLE_RUNTIME_COLLISION_FREQUENCY_STEP,
                                     amplitude >> 1,
                                     pan >> VEHICLE_RUNTIME_FIXED16_SHIFT);
          g_SECONDARY_VEHICLE_SOUND_SUPPRESSED    = 1;
          g_VEHICLE_COLLISION_SECONDARY_COUNTDOWN = 1;
        }
      else if(g_SECONDARY_VEHICLE_SOUND_SUPPRESSED == 0)
        {
          g_VEHICLE_COLLISION_SECONDARY_COUNTDOWN--;
        }
    }

  g_VEHICLE_COLLISION_TRACK_POSITION =
    ((VehicleRuntimeFrameRecord *)
       g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_SIX_TARGET])
      ->track_position;

  if(primary_record == NULL)
    {
      update_dolby_spatial_sound(VEHICLE_RUNTIME_ENGINE_SOUND_PRIMARY,
                                 VEHICLE_RUNTIME_ENGINE_SOUND_CLASS,
                                 0,
                                 0,
                                 0);
    }
  else
    {
      calculate_spatial_sound_parameters((const Vector3 *) &primary_record->audio_position_x,
                                         &pan,
                                         &distance);
      sound_configuration = primary_record->spatial_sound_configuration;
      if(sound_configuration == 0)
        {
          distance_scale = calculate_vehicle_audio_distance_scale(
                             (const ApproximateDistancePoint *) primary_record) >>
                           VEHICLE_RUNTIME_FIXED16_SHIFT;
          if((primary_record->slot_index == 1) &&
             (distance_scale < VEHICLE_RUNTIME_PLAYER_ENGINE_GAIN_MINIMUM))
            {
              distance_scale = VEHICLE_RUNTIME_PLAYER_ENGINE_GAIN_MINIMUM;
            }
          amplitude = (distance_scale * calculate_sound_distance_gain(distance)) >>
                      VEHICLE_RUNTIME_GAIN_PRODUCT_SHIFT;
          if(amplitude < 0)
            amplitude = 0;
          if(primary_record->slot_index == 1)
            {
              frequency_entry =
                &g_SPATIAL_SOUND_FREQUENCY_CACHE[VEHICLE_RUNTIME_AUDIO_FREQUENCY_TABLE_BASE +
                                                 g_STARTUP_CONFIGURATION.opponent_vehicle_index *
                                                   VEHICLE_RUNTIME_AUDIO_FREQUENCY_TABLE_STRIDE];
              frequency =
                (primary_record->engine_frequency_limit >> VEHICLE_RUNTIME_FIXED16_SHIFT) /
                (primary_record->audio_engine_speed * frequency_entry[0]);
              frequency += frequency_entry[1];
            }
          else
            {
              frequency = VEHICLE_RUNTIME_DEFAULT_ENGINE_FREQUENCY;
            }
          motion_scale = calculate_vehicle_audio_motion_scale(primary_record);
          if(motion_scale != 0)
            {
              frequency = (motion_scale * frequency) >> VEHICLE_RUNTIME_FIXED8_SHIFT;
              if(primary_record->slot_index == 1)
                {
                  update_dolby_spatial_sound(VEHICLE_RUNTIME_ENGINE_SOUND_PRIMARY,
                                             VEHICLE_RUNTIME_PLAYER_ENGINE_SOUND_CLASS,
                                             frequency,
                                             amplitude << VEHICLE_RUNTIME_PRIMARY_AMPLITUDE_SHIFT,
                                             pan >> VEHICLE_RUNTIME_FIXED16_SHIFT);
                }
              else
                {
                  update_dolby_spatial_sound(VEHICLE_RUNTIME_ENGINE_SOUND_PRIMARY,
                                             VEHICLE_RUNTIME_ENGINE_SOUND_CLASS,
                                             frequency,
                                             amplitude << VEHICLE_RUNTIME_PRIMARY_AMPLITUDE_SHIFT,
                                             pan >> VEHICLE_RUNTIME_FIXED16_SHIFT);
                }
            }
        }
      else
        {
          play_vehicle_spatial_sound_attenuated(
            VEHICLE_RUNTIME_ENGINE_SOUND_PRIMARY,
            (int32) sound_configuration >> VEHICLE_RUNTIME_SOUND_CONFIGURATION_SHIFT,
            (int32) (sound_configuration & VEHICLE_RUNTIME_SOUND_VARIANT_MASK),
            primary_record->spatial_sound_gain,
            distance,
            pan);
        }
      g_PRIMARY_VEHICLE_AUDIO_RECORD = primary_record;
    }

  if(secondary_record == NULL)
    {
      update_dolby_spatial_sound(VEHICLE_RUNTIME_ENGINE_SOUND_SECONDARY,
                                 VEHICLE_RUNTIME_ENGINE_SOUND_CLASS,
                                 0,
                                 0,
                                 0);
    }
  else
    {
      calculate_spatial_sound_parameters((const Vector3 *) &secondary_record->audio_position_x,
                                         &pan,
                                         &distance);
      sound_configuration = secondary_record->spatial_sound_configuration;
      if(sound_configuration == 0)
        {
          distance_scale = calculate_vehicle_audio_distance_scale(
                             (const ApproximateDistancePoint *) secondary_record) >>
                           VEHICLE_RUNTIME_FIXED16_SHIFT;
          if((secondary_record->slot_index == 1) &&
             (distance_scale < VEHICLE_RUNTIME_PLAYER_ENGINE_GAIN_MINIMUM))
            {
              distance_scale = VEHICLE_RUNTIME_PLAYER_ENGINE_GAIN_MINIMUM;
            }
          amplitude = ((distance_scale + VEHICLE_RUNTIME_SECONDARY_DISTANCE_BIAS) *
                       calculate_sound_distance_gain(distance)) >>
                      VEHICLE_RUNTIME_GAIN_PRODUCT_SHIFT;
          if(amplitude < 0)
            amplitude = 0;
          if(secondary_record->slot_index == 1)
            {
              frequency_entry =
                &g_SPATIAL_SOUND_FREQUENCY_CACHE[VEHICLE_RUNTIME_AUDIO_FREQUENCY_TABLE_BASE +
                                                 g_STARTUP_CONFIGURATION.opponent_vehicle_index *
                                                   VEHICLE_RUNTIME_AUDIO_FREQUENCY_TABLE_STRIDE];
              frequency =
                (secondary_record->engine_frequency_limit >> VEHICLE_RUNTIME_FIXED16_SHIFT) /
                (secondary_record->audio_engine_speed * frequency_entry[0]);
              frequency += frequency_entry[1];
            }
          else
            {
              frequency = VEHICLE_RUNTIME_DEFAULT_ENGINE_FREQUENCY;
            }
          motion_scale = calculate_vehicle_audio_motion_scale(secondary_record);
          if(motion_scale != 0)
            {
              frequency = (motion_scale * frequency) >> VEHICLE_RUNTIME_FIXED8_SHIFT;
              if(secondary_record->slot_index == 1)
                {
                  update_dolby_spatial_sound(VEHICLE_RUNTIME_ENGINE_SOUND_SECONDARY,
                                             VEHICLE_RUNTIME_PLAYER_ENGINE_SOUND_CLASS,
                                             frequency,
                                             amplitude << VEHICLE_RUNTIME_PRIMARY_AMPLITUDE_SHIFT,
                                             pan >> VEHICLE_RUNTIME_FIXED16_SHIFT);
                }
              else
                {
                  update_dolby_spatial_sound(VEHICLE_RUNTIME_ENGINE_SOUND_SECONDARY,
                                             VEHICLE_RUNTIME_ENGINE_SOUND_CLASS,
                                             frequency,
                                             amplitude << VEHICLE_RUNTIME_SECONDARY_AMPLITUDE_SHIFT,
                                             pan >> VEHICLE_RUNTIME_FIXED16_SHIFT);
                }
            }
        }
      else
        {
          play_vehicle_spatial_sound_attenuated(
            VEHICLE_RUNTIME_ENGINE_SOUND_SECONDARY,
            (int32) sound_configuration >> VEHICLE_RUNTIME_SOUND_CONFIGURATION_SHIFT,
            (int32) (sound_configuration & VEHICLE_RUNTIME_SOUND_VARIANT_MASK),
            secondary_record->spatial_sound_gain,
            distance,
            pan);
        }
      g_SECONDARY_VEHICLE_AUDIO_RECORD = secondary_record;
    }
}

static void
prepare_vehicle_runtime_audio(void)
{
  SimulationSource *source;
  int32             pan;
  int32             distance;

  source = (SimulationSource *) ((uint8 *) G_ACTIVE_SIMULATION_SOURCE +
                                 VEHICLE_RUNTIME_CALLBACK_SOURCE_OFFSET);
  if(*(int32 *) g_FRONTEND_RENDER_STATE.projection.source == 0)
    {
      pan      = 0;
      distance = 1;
    }
  else
    {
      calculate_spatial_sound_parameters(&source->position, &pan, &distance);
    }
  update_player_vehicle_runtime_audio(source, pan, distance);
}

void
update_vehicle_runtime_objects(void)
{
  VehicleRuntimeControllerState *controller;
  VehicleRuntimeFrameRecord     *record;
  VehicleRuntimeFrameRecord     *player_record;
  DoublyLinkedNode              *node;
  uint32                         render_mode;
  int32                          anti_lock_brakes_enabled;

  update_replay_runtime();
  render_mode = (uint32) G_FRONTEND_EXIT_REQUESTED;
  controller  = (VehicleRuntimeControllerState *) &g_VEHICLE_RUNTIME_POOL_STATE;

  if((render_mode == VEHICLE_RUNTIME_RENDER_MODE_COMPLETE) ||
     (g_SIMULATION_THREAD_SIGNAL_STATE.runtime_update_blocked != 0))
    {
      if(g_RENDER_THREAD_STATE.frame_complete != 0)
        g_RENDER_THREAD_STATE.frame_complete += 1;
      return;
    }

  if((render_mode == VEHICLE_RUNTIME_RENDER_MODE_BLOCKED) && (G_SIMULATION_UPDATE_MODE == 0) &&
     (G_SIMULATION_FRAME_BUDGET < 1))
    {
      return;
    }

  if((g_SIMULATION_THREAD_SIGNAL_STATE.render_flags & VEHICLE_RUNTIME_RENDER_FLAG_CALLBACKS) != 0)
    {
      node = controller->active_record_list->list.head_sentinel->next;
      while(node->next != NULL)
        {
          record = (VehicleRuntimeFrameRecord *) node;
          if(((record->runtime_flags & VEHICLE_RUNTIME_RECORD_ACTIVE_FLAG) != 0) &&
             (record->state_update_callback != NULL))
            {
              ((VehicleRuntimeRecordCallback) ((uint32) record->state_update_callback &
                                               VEHICLE_RUNTIME_CALLBACK_ADDRESS_MASK))(
                (uint8 *) record + VEHICLE_RUNTIME_CALLBACK_SOURCE_OFFSET);
            }
          node = node->next;
        }
    }

  if((g_SIMULATION_THREAD_SIGNAL_STATE.render_flags & VEHICLE_RUNTIME_RENDER_FLAG_UPDATE) != 0)
    {
      prepare_vehicle_runtime_render();
      player_record = (VehicleRuntimeFrameRecord *) G_ACTIVE_SIMULATION_SOURCE;
      if(player_record->track_position < VEHICLE_RUNTIME_PLAYER_MOTION_LIMIT)
        {
          player_record->warning_pending      = 1;
          player_record->player_render_scale  = VEHICLE_RUNTIME_GAME_STATE_SCALE;
          player_record->special_render_value = 0;
          if(((player_record->track_position < VEHICLE_RUNTIME_PLAYER_TRACK_LIMIT) ||
              (player_record->lateral_position < VEHICLE_RUNTIME_PLAYER_LATERAL_LIMIT)) &&
             (G_FRONTEND_EXIT_REQUESTED == 1))
            {
              g_GAME_RUNTIME_STATE.flags_14 = VEHICLE_RUNTIME_GAME_STATE_FLAG;
              g_GAME_RUNTIME_STATE.flags_18 = VEHICLE_RUNTIME_GAME_STATE_FLAG;
            }
        }

      node = controller->active_record_list->list.head_sentinel->next;
      while(node->next != NULL)
        {
          record = (VehicleRuntimeFrameRecord *) node;
          if(((record->runtime_flags & VEHICLE_RUNTIME_RECORD_ACTIVE_FLAG) != 0) &&
             (record->render_update_callback != NULL))
            {
              if((record != (VehicleRuntimeFrameRecord *) G_ACTIVE_VEHICLE_RUNTIME_RECORD) ||
                 (G_GAME_FRAME_COUNTER == 0) ||
                 ((g_GAME_RUNTIME_STATE.race_start_frame > 0) &&
                  (g_GAME_RUNTIME_STATE.race_start_frame < G_GAME_FRAME_COUNTER)))
                {
                  ((VehicleRuntimeRecordCallback) ((uint32) record->render_update_callback &
                                                   VEHICLE_RUNTIME_CALLBACK_ADDRESS_MASK))(
                    (uint8 *) record + VEHICLE_RUNTIME_CALLBACK_SOURCE_OFFSET);
                }
              update_vehicle_runtime_track_offset(
                (SimulationSource *) ((uint8 *) record + VEHICLE_RUNTIME_CALLBACK_SOURCE_OFFSET));
            }
          node = node->next;
        }

      sort_list_nodes_by_two_keys(&controller->active_record_list->list,
                                  VEHICLE_RUNTIME_SORT_KEY_OFFSET,
                                  VEHICLE_RUNTIME_SORT_RECORD_BYTES);
      update_vehicle_runtime_interactions(controller->active_record_list);
      sort_list_nodes_by_two_keys(&controller->active_record_list->list,
                                  VEHICLE_RUNTIME_SORT_KEY_OFFSET,
                                  VEHICLE_RUNTIME_SORT_RECORD_BYTES);
      update_simulation_state(&g_SIMULATION_RENDER_STATE);

      if((render_mode == VEHICLE_RUNTIME_RENDER_MODE_AUDIO_DISABLED) ||
         (g_STARTUP_CONFIGURATION.anti_lock_brakes_enabled == 0) ||
         (controller->sound_update_complete != 0))
        {
          if(controller->audio_channels_stopped == 0)
            {
              stop_all_game_audio_channels();
              controller->audio_channels_stopped = 1;
            }
        }
      else
        {
          refresh_vehicle_runtime_audio();
          if(controller->render_update_pending < VEHICLE_RUNTIME_AUDIO_UPDATE_LIMIT)
            {
              if(player_record->warning_mode == -1)
                prepare_vehicle_runtime_audio();

              anti_lock_brakes_enabled = 0;
              if(player_record->vehicle_specification != NULL)
                anti_lock_brakes_enabled = g_ANTI_LOCK_BRAKES_ENABLED;
              if((player_record->vehicle_specification == NULL) || (anti_lock_brakes_enabled == 0))
                {
                  if(((player_record->audio_state_flags & VEHICLE_RUNTIME_AUDIO_STATE_TIMER_FLAG) ==
                      0) ||
                     (g_TRACTION_CONTROL_ENABLED == 0))
                    {
                      controller->render_update_pending += 1;
                      controller->audio_channels_stopped = 0;
                      goto refresh_audio_positions;
                    }
                }
              update_active_vehicle_spatial_audio_channels();
            }
          else
            {
              controller->render_update_pending = 0;
              prepare_vehicle_runtime_audio();
              update_active_vehicle_spatial_audio_channels();
              update_vehicle_runtime_audio_positions(controller->active_record_list);
            }
          controller->render_update_pending += 1;
          controller->audio_channels_stopped = 0;
        }

    refresh_audio_positions:
      if((render_mode != VEHICLE_RUNTIME_RENDER_MODE_AUDIO_DISABLED) &&
         ((g_SIMULATION_THREAD_SIGNAL_STATE.render_flags & VEHICLE_RUNTIME_RENDER_FLAG_CALLBACKS) !=
          0))
        {
          update_vehicle_runtime_audio_positions(controller->active_record_list);
        }

      if(g_RENDER_THREAD_STATE.frame_complete == 0)
        {
          controller->render_update_ready = 1;
        }
      else
        {
          controller->render_object_count = 0;
          node = controller->active_record_list->list.head_sentinel->next;
          while(node->next != NULL)
            {
              record = (VehicleRuntimeFrameRecord *) node;
              if(((record->runtime_flags & VEHICLE_RUNTIME_RECORD_ACTIVE_FLAG) != 0) &&
                 (record->state_update_callback != NULL))
                {
                  g_VEHICLE_ANIMATION_STATES[controller->render_object_count] =
                    (VehicleSimulationState *) record;
                  controller->render_object_count += 1;
                  capture_vehicle_render_snapshot((VehicleRenderSnapshotState *) record);
                }
              node = node->next;
            }
          capture_simulation_render_state(&g_SIMULATION_RENDER_STATE);
          if(controller->sound_update_complete != 0)
            {
              controller->road_render_state     = 1;
              controller->sound_update_complete = 0;
            }
          g_RENDER_THREAD_STATE.frame_complete += 1;
          controller->render_update_ready = 0;
        }
    }

  if((render_mode == VEHICLE_RUNTIME_RENDER_MODE_BLOCKED) &&
     (G_SIMULATION_UPDATE_MODE != VEHICLE_RUNTIME_RENDER_MODE_COMPLETE))
    {
      return;
    }

  if(((g_SIMULATION_THREAD_SIGNAL_STATE.render_flags & VEHICLE_RUNTIME_RENDER_FLAG_UPDATE) == 0) &&
     (g_RENDER_THREAD_STATE.frame_complete != 0) && (controller->render_update_ready == 1))
    {
      controller->render_object_count = 0;
      node                            = controller->active_record_list->list.head_sentinel->next;
      while(node->next != NULL)
        {
          record = (VehicleRuntimeFrameRecord *) node;
          if(((record->runtime_flags & VEHICLE_RUNTIME_RECORD_ACTIVE_FLAG) != 0) &&
             (record->state_update_callback != NULL))
            {
              g_VEHICLE_ANIMATION_STATES[controller->render_object_count] =
                (VehicleSimulationState *) record;
              controller->render_object_count += 1;
              capture_vehicle_render_snapshot((VehicleRenderSnapshotState *) record);
            }
          node = node->next;
        }
      capture_simulation_render_state(&g_SIMULATION_RENDER_STATE);
      if(controller->sound_update_complete != 0)
        {
          controller->road_render_state     = 1;
          controller->sound_update_complete = 0;
        }
      g_RENDER_THREAD_STATE.frame_complete += 1;
      controller->render_update_ready = 0;
    }
  else
    {
      while(controller->sound_update_complete != 0)
        {
          if(g_RENDER_THREAD_STATE.frame_complete != 0)
            {
              controller->render_object_count = 0;
              node = controller->active_record_list->list.head_sentinel->next;
              while(node->next != NULL)
                {
                  record = (VehicleRuntimeFrameRecord *) node;
                  if(((record->runtime_flags & VEHICLE_RUNTIME_RECORD_ACTIVE_FLAG) != 0) &&
                     (record->state_update_callback != NULL))
                    {
                      g_VEHICLE_ANIMATION_STATES[controller->render_object_count] =
                        (VehicleSimulationState *) record;
                      controller->render_object_count += 1;
                      capture_vehicle_render_snapshot((VehicleRenderSnapshotState *) record);
                    }
                  node = node->next;
                }
              capture_simulation_render_state(&g_SIMULATION_RENDER_STATE);
              g_RENDER_THREAD_STATE.frame_complete += 1;
              controller->road_render_state     = 1;
              controller->render_update_ready   = 0;
              controller->sound_update_complete = 0;
            }
          signal_render_thread();
          (void) delay_game_vbls(VEHICLE_RUNTIME_WAIT_COUNT);
        }
    }

  while(controller->road_render_state != 0)
    {
      if(g_RENDER_THREAD_STATE.frame_complete != 0)
        g_RENDER_THREAD_STATE.frame_complete += 1;
      signal_render_thread();
      (void) delay_game_vbls(VEHICLE_RUNTIME_WAIT_COUNT);
    }

  if(g_VEHICLE_RUNTIME_FRAME_DELAY_COUNTER > VEHICLE_RUNTIME_WAIT_LIMIT)
    g_VEHICLE_RUNTIME_FRAME_DELAY_COUNTER = 0;
}
