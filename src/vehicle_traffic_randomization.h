#pragma once

#include "simulation_snapshot.h"

#include "types.h"

#define VEHICLE_TRAFFIC_RANDOM_TABLE_COUNT 4
#define VEHICLE_TRAFFIC_SPEED_SCALE_COUNT 21
#define VEHICLE_STEERING_RANDOM_MODE_COUNT 3

typedef struct VehicleSpawnConfiguration
{
  int32 target_traffic_count;
  int32 maximum_track_position_adjustment;
  int32 minimum_spawn_spacing;
  int32 initial_spawn_delay;
  int32 spawn_delay_variance;
  int32 maximum_spawn_spacing;
  int32 minimum_traffic_speed;
  int32 track_position_margin;
  int32 placement_attempt_count;
  int32 minimum_refresh_delay;
  int32 maximum_refresh_delay;
  int32 lane_search_limit;
  int32 activation_range;
  int32 maximum_active_vehicle_count;
  int32 refresh_speeds[VEHICLE_TRAFFIC_RANDOM_TABLE_COUNT];
  int32 speed_average;
  int32 steering_offsets[VEHICLE_TRAFFIC_RANDOM_TABLE_COUNT];
  int32 track_speed_scales[VEHICLE_TRAFFIC_SPEED_SCALE_COUNT];
} VehicleSpawnConfiguration;

typedef struct VehicleSpawnPositionState
{
  int32 active_vehicle_count;
  int32 track_heading_change;
  int32 track_curvature;
  int32 track_gap_index;
} VehicleSpawnPositionState;

typedef struct VehicleSpawnRandomState
{
  uint32 low_word;
  uint32 product;
  uint32 multiplier;
} VehicleSpawnRandomState;

extern VehicleSpawnConfiguration g_VEHICLE_SPAWN_CONFIGURATION;
extern VehicleSpawnPositionState g_VEHICLE_SPAWN_POSITION_STATE;
extern VehicleSpawnRandomState   g_VEHICLE_SPAWN_RANDOM_STATE;
extern int32 g_VEHICLE_STEERING_RANDOM_THRESHOLDS[VEHICLE_STEERING_RANDOM_MODE_COUNT];

void
load_vehicle_spawn_configuration(const char *path_);
void
initialize_vehicle_spawn_configuration(VehicleSpawnConfiguration *configuration_, int32 mode_);
int32
randomize_vehicle_track_speed(SimulationSource *source_, int32 speed_);
void
randomize_vehicle_refresh_speed(SimulationSource *source_);
void
randomize_vehicle_steering_offset(SimulationSource *source_);
void
initialize_vehicle_steering_offset(SimulationSource *source_);
