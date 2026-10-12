#pragma once

#include "types.h"

#define VEHICLE_DRIVE_STATISTICS_VEHICLE_COUNT 3
#define VEHICLE_DRIVE_STATISTICS_RECORD_BYTES 0xB0
#define VEHICLE_DRIVE_STATISTICS_TOTAL_BYTES 0x24C
#define VEHICLE_DRIVE_STATISTICS_WORD_BYTES 4
#define VEHICLE_DRIVE_STATISTICS_SUMMARY_TRAILING_WORDS 6
#define VEHICLE_DRIVE_STATISTICS_SUMMARY_BYTES                                                     \
  (VEHICLE_DRIVE_STATISTICS_TOTAL_BYTES -                                                          \
   (VEHICLE_DRIVE_STATISTICS_VEHICLE_COUNT * VEHICLE_DRIVE_STATISTICS_RECORD_BYTES))
#define VEHICLE_DRIVER_SKILL_TABLE_BYTES 0x700

typedef struct VehicleDriveStatisticsRecord
{
  uint32 reserved_000;
  uint32 primary_weight;
  uint32 shared_value;
  int32  primary_maximum_candidate;
  uint32 primary_ratio;
  uint32 reserved_014[4];
  uint32 tracked_racer_pass_count;
  uint32 first_difference;
  uint32 tracked_racer_off_road_pass_count;
  uint32 other_racer_pass_count;
  uint32 second_difference;
  uint32 other_racer_off_road_pass_count;
  uint32 reserved_03_c[6];
  uint32 first_sample_total;
  uint32 first_sample_count;
  uint32 second_sample_total;
  uint32 second_sample_count;
  uint32 first_sample_average;
  uint32 second_sample_average;
  uint32 reserved_06_c[2];
  uint32 traffic_proximity_trigger_count;
  uint32 reserved_078[3];
  uint32 resolved_event_count;
  uint32 expired_event_count;
  uint32 pending_event_count;
  uint32 unresolved_event_count;
  uint32 reserved_094[2];
  uint32 secondary_weight;
  uint32 secondary_value;
  int32  secondary_maximum_candidate;
  uint32 secondary_ratio;
  uint32 tracked_racer_encounter_count;
} VehicleDriveStatisticsRecord;

typedef struct VehicleDriveStatisticsSummary
{
  uint32 tracked_racer_ahead;
  uint32 primary_weight_total;
  uint32 primary_value_total;
  uint32 primary_ratio;
  int32  primary_maximum;
  uint32 secondary_weight_total;
  uint32 secondary_value_total;
  uint32 secondary_ratio;
  int32  secondary_maximum;
  uint32 collision_event_time;
  uint32 recovery_count;
  int32  recovery_adjustment;
  int32  lane_search_limit;
  uint32 event_track_segment;
  uint32 reserved_38;
} VehicleDriveStatisticsSummary;

typedef struct VehicleDriveStatistics
{
  VehicleDriveStatisticsRecord  vehicle_records[VEHICLE_DRIVE_STATISTICS_VEHICLE_COUNT];
  VehicleDriveStatisticsSummary summary;
} VehicleDriveStatistics;

extern int32                  g_EVENT_INDEX;
extern int32                  g_TRACK_INDEX;
extern int32                  g_VEHICLE_STATISTICS_MODE_MAXIMUMS[9];
extern VehicleDriveStatistics g_VEHICLE_DRIVE_STATISTICS;
extern VehicleDriveStatistics g_VEHICLE_DRIVE_STATISTICS_BACKUP;
extern uint8                  g_VEHICLE_DRIVER_SKILL_TABLE[VEHICLE_DRIVER_SKILL_TABLE_BYTES];

void
initialize_vehicle_drive_statistics(void);
void
load_vehicle_profile_time_limit(const char *path_);
void
update_vehicle_drive_statistics_derived_values(VehicleDriveStatisticsRecord *record_);
void
recalculate_vehicle_drive_statistics_summary(void);
void
save_vehicle_drive_statistics(void);
void
restore_vehicle_drive_statistics(void);
