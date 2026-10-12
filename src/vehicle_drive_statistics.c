#include "vehicle_drive_statistics.h"

#include "disk_stream_cleanup.h"
#include "formatted_output.h"
#include "frontend_overlay.h"
#include "frontend_transition.h"
#include "game_bootstrap.h"
#include "string_token_normalization.h"
#include "string_utilities.h"
#include "text_line_reader.h"
#include "vehicle_parameter_loader.h"
#include "vehicle_simulation_state.h"

#include "operamath.h"

#include "stddef.h"
#include "stdlib.h"
#include "string.h"

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]
#define VEHICLE_DRIVE_STATISTICS_RATIO_INPUT_SHIFT 12
#define VEHICLE_DRIVE_STATISTICS_RATIO_OUTPUT_SHIFT 4
#define VEHICLE_DRIVE_STATISTICS_PROFILE_MULTIPLIER 6
#define VEHICLE_DRIVE_STATISTICS_FIXED_SHIFT 16
#define VEHICLE_DRIVE_STATISTICS_CALCULATION_SHIFT 9
#define VEHICLE_DRIVE_STATISTICS_MINIMUM_WEIGHT_BONUS 0x3C
#define VEHICLE_DRIVE_STATISTICS_WEIGHT_THRESHOLD 0xF5C2
#define VEHICLE_DRIVER_SKILL_GROUP_BYTES 0x1C0
#define VEHICLE_DRIVER_SKILL_RECORD_BYTES 0x70
#define VEHICLE_DRIVER_SKILL_SECONDARY_WEIGHT_OFFSET 0x14
#define VEHICLE_DRIVER_SKILL_SECONDARY_RATIO_OFFSET 0x18
#define VEHICLE_DRIVER_SKILL_MAXIMUM_OFFSET 0x1C
#define VEHICLE_TUNING_SCALE_OFFSET 0x17C
#define VEHICLE_DRIVE_STATISTICS_COPY_WORD_COUNT 3
#define VEHICLE_DRIVE_STATISTICS_COPY_CHUNK_BYTES                                                  \
  (VEHICLE_DRIVE_STATISTICS_COPY_WORD_COUNT * VEHICLE_DRIVE_STATISTICS_WORD_BYTES)
#define VEHICLE_DRIVE_STATISTICS_COPY_CHUNK_COUNT                                                  \
  (VEHICLE_DRIVE_STATISTICS_TOTAL_BYTES / VEHICLE_DRIVE_STATISTICS_COPY_CHUNK_BYTES)
#define VEHICLE_PROFILE_TEST_INDEX 3
#define VEHICLE_PROFILE_NUMBER_OFFSET 1
#define VEHICLE_PROFILE_TOKEN_BYTES 12
#define VEHICLE_PROFILE_LINE_BYTES 128
#define VEHICLE_PROFILE_LINE_READ_LIMIT 127
#define VEHICLE_PROFILE_FRAMES_PER_SECOND 60
#define VEHICLE_PROFILE_DEFAULT_TIME_SECONDS 10000
#define VEHICLE_PROFILE_NAME_FORMAT "%s%d"
#define VEHICLE_PROFILE_END_TOKEN "end"

typedef struct VehicleDriveStatisticsCopyChunk
{
  uint32 words[VEHICLE_DRIVE_STATISTICS_COPY_WORD_COUNT];
} VehicleDriveStatisticsCopyChunk;

typedef struct VehicleDriverSkillRecord
{
  uint8  reserved_00[VEHICLE_DRIVER_SKILL_SECONDARY_WEIGHT_OFFSET];
  uint32 secondary_weight;
  uint32 secondary_ratio;
  int32  maximum_candidate;
  uint8  reserved_20[VEHICLE_DRIVER_SKILL_RECORD_BYTES - VEHICLE_DRIVER_SKILL_MAXIMUM_OFFSET -
                     sizeof(int32)];
} VehicleDriverSkillRecord;

int32                  g_TRACK_INDEX;
int32                  g_EVENT_INDEX;
VehicleDriveStatistics g_VEHICLE_DRIVE_STATISTICS;
VehicleDriveStatistics g_VEHICLE_DRIVE_STATISTICS_BACKUP;
uint8                  g_VEHICLE_DRIVER_SKILL_TABLE[VEHICLE_DRIVER_SKILL_TABLE_BYTES];
uint8                  g_VEHICLE_DRIVER_SKILL_RESERVED_STATE[0x120];
int32                  g_VEHICLE_STATISTICS_MODE_MAXIMUMS[9] = { 0x00453333, 0x005A8000, 0x004B0000,
                                                                 0x004F8000, 0x00560000, 0x0047EB85,
                                                                 0x004B0000, 0x00468000, 0 };

COMPILE_TIME_SIZE_ASSERT(vehicle_drive_statistics_record_size,
                         sizeof(VehicleDriveStatisticsRecord) ==
                           VEHICLE_DRIVE_STATISTICS_RECORD_BYTES);
COMPILE_TIME_SIZE_ASSERT(vehicle_drive_statistics_int32_size, sizeof(int32) == 4);
COMPILE_TIME_SIZE_ASSERT(vehicle_drive_statistics_summary_size,
                         sizeof(VehicleDriveStatisticsSummary) ==
                           VEHICLE_DRIVE_STATISTICS_SUMMARY_BYTES);
COMPILE_TIME_SIZE_ASSERT(vehicle_drive_statistics_size,
                         sizeof(VehicleDriveStatistics) == VEHICLE_DRIVE_STATISTICS_TOTAL_BYTES);
COMPILE_TIME_SIZE_ASSERT(vehicle_driver_skill_record_size,
                         sizeof(VehicleDriverSkillRecord) == VEHICLE_DRIVER_SKILL_RECORD_BYTES);
COMPILE_TIME_SIZE_ASSERT(vehicle_statistics_mode_maximums_size,
                         sizeof(g_VEHICLE_STATISTICS_MODE_MAXIMUMS) == 0x24);
COMPILE_TIME_SIZE_ASSERT(vehicle_profile_time_limit_offset,
                         offsetof(VehicleGearShiftStatistics, profile_time_limit_frames) == 0x94);

void
load_vehicle_profile_time_limit(const char *path_)
{
  Stream *stream;
  char    line[VEHICLE_PROFILE_LINE_BYTES];
  char    vehicle_token[VEHICLE_PROFILE_TOKEN_BYTES];
  int32   time_seconds;

  if(g_TRACK_INDEX != VEHICLE_PROFILE_TEST_INDEX)
    {
      sprintf(vehicle_token,
              VEHICLE_PROFILE_NAME_FORMAT,
              g_STARTUP_VEHICLE_NAME_PREFIXES[g_TRACK_INDEX],
              g_EVENT_INDEX + VEHICLE_PROFILE_NUMBER_OFFSET);
    }
  else
    {
      copy_string(vehicle_token, g_STARTUP_VEHICLE_NAME_PREFIXES[VEHICLE_PROFILE_TEST_INDEX]);
    }
  normalize_lowercase_token(vehicle_token);

  stream = open_disk_stream_default((char *) path_);
  while(1)
    {
      read_noncomment_text_line(line, VEHICLE_PROFILE_LINE_READ_LIMIT, stream);
      normalize_lowercase_token(line);
      if(compare_strings((const uint8 *) line, (const uint8 *) VEHICLE_PROFILE_END_TOKEN) == 0)
        {
          time_seconds = VEHICLE_PROFILE_DEFAULT_TIME_SECONDS;
          break;
        }
      if(compare_strings((const uint8 *) line, (const uint8 *) vehicle_token) == 0)
        {
          read_noncomment_text_line(line, VEHICLE_PROFILE_LINE_READ_LIMIT, stream);
          time_seconds = atoi(line);
          break;
        }
    }

  g_VEHICLE_GEAR_SHIFT_STATISTICS->profile_time_limit_frames =
    time_seconds * VEHICLE_PROFILE_FRAMES_PER_SECOND;
  (void) close_disk_stream_and_return_zero(stream);
}

void
initialize_vehicle_drive_statistics(void)
{
  if(g_EVENT_INDEX != 0)
    return;

  memset(&g_VEHICLE_DRIVE_STATISTICS, 0, sizeof(g_VEHICLE_DRIVE_STATISTICS));
}

void
update_vehicle_drive_statistics_derived_values(VehicleDriveStatisticsRecord *record_)
{
  const VehicleDriverSkillRecord *driver_skill;
  const int32                    *vehicle_tuning_scales;
  uint32                          profile_scale;
  int32                           normalized_value;
  uint32                          calculation_weight;
  int32                           maximum;

  if(record_->first_sample_count != 0)
    record_->first_sample_average =
      DivUF16(record_->first_sample_total,
              record_->first_sample_count << VEHICLE_DRIVE_STATISTICS_FIXED_SHIFT);
  else
    record_->first_sample_average = 0;

  if(record_->second_sample_count != 0)
    record_->second_sample_average =
      DivUF16(record_->second_sample_total,
              record_->second_sample_count << VEHICLE_DRIVE_STATISTICS_FIXED_SHIFT);
  else
    record_->second_sample_average = 0;

  record_->first_difference =
    record_->tracked_racer_pass_count - record_->tracked_racer_off_road_pass_count;
  record_->second_difference =
    record_->other_racer_pass_count - record_->other_racer_off_road_pass_count;

  profile_scale =
    g_GAME_RUNTIME_STATE.track_segment_count * VEHICLE_DRIVE_STATISTICS_PROFILE_MULTIPLIER;
  if((record_->secondary_weight == 0) &&
     (g_OPPONENT_VEHICLE_INDEX != VEHICLE_SIMULATION_DISABLED_MODE))
    {
      normalized_value =
        DivUF16(record_->secondary_value, profile_scale << VEHICLE_DRIVE_STATISTICS_FIXED_SHIFT);
      record_->secondary_weight =
        DivUF16(record_->primary_weight << VEHICLE_DRIVE_STATISTICS_FIXED_SHIFT,
                normalized_value) >>
        VEHICLE_DRIVE_STATISTICS_FIXED_SHIFT;
      if(record_->secondary_weight < record_->primary_weight)
        record_->secondary_weight =
          record_->primary_weight + VEHICLE_DRIVE_STATISTICS_MINIMUM_WEIGHT_BONUS;
      record_->secondary_value = profile_scale << VEHICLE_DRIVE_STATISTICS_FIXED_SHIFT;
    }

  if(g_OPPONENT_VEHICLE_INDEX == VEHICLE_SIMULATION_DISABLED_MODE)
    {
      driver_skill =
        (const VehicleDriverSkillRecord *) (g_VEHICLE_DRIVER_SKILL_TABLE +
                                            (g_TRACK_INDEX * VEHICLE_DRIVER_SKILL_GROUP_BYTES) +
                                            (g_EVENT_INDEX * VEHICLE_DRIVER_SKILL_RECORD_BYTES));
      record_->secondary_weight            = driver_skill->secondary_weight;
      record_->secondary_ratio             = driver_skill->secondary_ratio;
      record_->secondary_maximum_candidate = driver_skill->maximum_candidate;
      record_->secondary_value             = record_->shared_value;
    }

  calculation_weight = record_->primary_weight;
  normalized_value =
    DivUF16(record_->shared_value, profile_scale << VEHICLE_DRIVE_STATISTICS_FIXED_SHIFT);
  if(normalized_value < VEHICLE_DRIVE_STATISTICS_WEIGHT_THRESHOLD)
    calculation_weight =
      DivUF16(record_->primary_weight << VEHICLE_DRIVE_STATISTICS_FIXED_SHIFT, normalized_value) >>
      VEHICLE_DRIVE_STATISTICS_FIXED_SHIFT;

  vehicle_tuning_scales =
    (const int32 *) ((const uint8 *)
                       g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_FIVE_TARGET] +
                     VEHICLE_TUNING_SCALE_OFFSET);
  maximum = MulSF16(record_->secondary_maximum_candidate,
                    vehicle_tuning_scales[g_STARTUP_CONFIGURATION.skill_level]);
  record_->secondary_maximum_candidate =
    DivSF16((record_->primary_maximum_candidate >> VEHICLE_DRIVE_STATISTICS_FIXED_SHIFT) *
              (calculation_weight << VEHICLE_DRIVE_STATISTICS_CALCULATION_SHIFT),
            record_->secondary_weight << VEHICLE_DRIVE_STATISTICS_CALCULATION_SHIFT);
  if(record_->secondary_maximum_candidate > maximum)
    record_->secondary_maximum_candidate = maximum;
  if(record_->secondary_maximum_candidate >
     g_VEHICLE_STATISTICS_MODE_MAXIMUMS[g_OPPONENT_VEHICLE_INDEX])
    record_->secondary_maximum_candidate =
      g_VEHICLE_STATISTICS_MODE_MAXIMUMS[g_OPPONENT_VEHICLE_INDEX];

  record_->primary_ratio =
    DivUF16(record_->shared_value,
            record_->primary_weight << VEHICLE_DRIVE_STATISTICS_RATIO_INPUT_SHIFT) >>
    VEHICLE_DRIVE_STATISTICS_RATIO_OUTPUT_SHIFT;
  if(g_OPPONENT_VEHICLE_INDEX != VEHICLE_SIMULATION_DISABLED_MODE)
    record_->secondary_ratio =
      DivUF16(record_->secondary_value,
              record_->secondary_weight << VEHICLE_DRIVE_STATISTICS_RATIO_INPUT_SHIFT) >>
      VEHICLE_DRIVE_STATISTICS_RATIO_OUTPUT_SHIFT;

  recalculate_vehicle_drive_statistics_summary();
}

void
recalculate_vehicle_drive_statistics_summary(void)
{
  const VehicleDriveStatisticsRecord *record;
  VehicleDriveStatisticsSummary      *summary;
  int32                               last_record_index;
  int32                               record_index;

  summary                         = &g_VEHICLE_DRIVE_STATISTICS.summary;
  summary->primary_weight_total   = 0;
  summary->primary_maximum        = 0;
  summary->secondary_weight_total = 0;
  summary->secondary_maximum      = 0;
  summary->primary_value_total    = 0;
  summary->secondary_value_total  = 0;
  last_record_index               = g_EVENT_INDEX;
  for(record_index = 0; record_index <= last_record_index; ++record_index)
    {
      record = &g_VEHICLE_DRIVE_STATISTICS.vehicle_records[record_index];
      summary->primary_weight_total += record->primary_weight;
      summary->secondary_weight_total += record->secondary_weight;
      summary->primary_value_total += record->shared_value;
      summary->secondary_value_total += record->shared_value;
      if(record->primary_maximum_candidate > summary->primary_maximum)
        summary->primary_maximum = record->primary_maximum_candidate;
      if(record->secondary_maximum_candidate > summary->secondary_maximum)
        summary->secondary_maximum = record->secondary_maximum_candidate;
    }

  summary->primary_ratio =
    (DivUF16(summary->primary_value_total,
             summary->primary_weight_total << VEHICLE_DRIVE_STATISTICS_RATIO_INPUT_SHIFT) >>
     VEHICLE_DRIVE_STATISTICS_RATIO_OUTPUT_SHIFT);
  summary->secondary_ratio =
    (DivUF16(summary->secondary_value_total,
             summary->secondary_weight_total << VEHICLE_DRIVE_STATISTICS_RATIO_INPUT_SHIFT) >>
     VEHICLE_DRIVE_STATISTICS_RATIO_OUTPUT_SHIFT);
}

void
save_vehicle_drive_statistics(void)
{
  VehicleDriveStatisticsCopyChunk       *destination;
  const VehicleDriveStatisticsCopyChunk *source;
  int32                                  chunk_count;

  destination = (VehicleDriveStatisticsCopyChunk *) &g_VEHICLE_DRIVE_STATISTICS_BACKUP;
  source      = (const VehicleDriveStatisticsCopyChunk *) &g_VEHICLE_DRIVE_STATISTICS;
  chunk_count = VEHICLE_DRIVE_STATISTICS_COPY_CHUNK_COUNT;
  do
    {
      *destination = *source;
      ++destination;
      ++source;
      --chunk_count;
    }
  while(chunk_count != 0);
}

void
restore_vehicle_drive_statistics(void)
{
  VehicleDriveStatisticsCopyChunk       *destination;
  const VehicleDriveStatisticsCopyChunk *source;
  int32                                  chunk_count;

  destination = (VehicleDriveStatisticsCopyChunk *) &g_VEHICLE_DRIVE_STATISTICS;
  source      = (const VehicleDriveStatisticsCopyChunk *) &g_VEHICLE_DRIVE_STATISTICS_BACKUP;
  chunk_count = VEHICLE_DRIVE_STATISTICS_COPY_CHUNK_COUNT;
  do
    {
      *destination = *source;
      ++destination;
      ++source;
      --chunk_count;
    }
  while(chunk_count != 0);
}
