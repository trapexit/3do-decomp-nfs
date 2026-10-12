#include "vehicle_drive_initialization.h"

#include "formatted_output.h"
#include "control_input.h"
#include "game_bootstrap.h"
#include "game_data_path.h"
#include "vehicle_control_input.h"
#include "vehicle_drive_statistics.h"
#include "vehicle_dynamics_state.h"
#include "vehicle_pair_collision.h"
#include "vehicle_parameter_loader.h"
#include "vehicle_simulation_state.h"
#include "vehicle_traffic_lane.h"
#include "vehicle_traffic_randomization.h"

#include "audio.h"

#define VEHICLE_INITIAL_FORWARD_TRAFFIC_COUNT 4
#define VEHICLE_TRACK_RUNTIME_DEFAULT_SPEED 0x5999
#define VEHICLE_TRACK_RANDOM_INITIAL_LOW_WORD 0xF874AF01
#define VEHICLE_CONTROL_INPUT_SEQUENCE_INITIAL_FRAME_LIMIT 80000
#define VEHICLE_STATISTICS_CURRENT_VEHICLE_SLOT 8
#define VEHICLE_PARAMETER_PATH_BYTES 100
#define VEHICLE_PARAMETER_PATH_FORMAT "%s%s%s.%d"
#define VEHICLE_PARAMETER_DIRECTORY "DriveData/"
#define VEHICLE_SPECIAL_RESOURCE_PREFIX 'p'

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

VehicleTrafficDirectionCounts g_VEHICLE_TRAFFIC_DIRECTION_COUNTS;
int32                         g_VEHICLE_SPAWN_CONFIGURATION_MODE;

COMPILE_TIME_SIZE_ASSERT(vehicle_drive_mode_selector_size,
                         sizeof(VehicleTrafficDirectionCounts) == 8);

void
initialize_vehicle_drive_state(void)
{
  uint32 audio_time;
  char  *special_resource_name;
  char   parameter_path[VEHICLE_PARAMETER_PATH_BYTES];

  audio_time = GetAudioTime();
  if((audio_time & 1) == 0)
    audio_time++;

  /* g_TRACK_RUNTIME_DATA is the loaded track table, not the retail sidecar block. */
  g_VEHICLE_SPAWN_RANDOM_STATE.multiplier           = (uint32) audio_time;
  g_VEHICLE_LAST_TRACK_FEATURE_SEGMENT              = 0;
  g_VEHICLE_SPAWN_POSITION_STATE.active_vehicle_count = 0;
  g_VEHICLE_SNAPSHOT_EVENT_STATE                    = 0;
  g_VEHICLE_COLLISION_STATISTICS_SUPPRESSED         = 0;
  g_VEHICLE_COLLISION_SCORE                         = 0;
  g_VEHICLE_TRAFFIC_DIRECTION_COUNTS.reverse_count = 0;
  g_VEHICLE_TRAFFIC_DIRECTION_COUNTS.forward_count = VEHICLE_INITIAL_FORWARD_TRAFFIC_COUNT;
  g_VEHICLE_SPAWN_CONFIGURATION_MODE               = g_STARTUP_CONFIGURATION.skill_level;
  g_VEHICLE_STATISTICS_MODE_MAXIMUMS[VEHICLE_STATISTICS_CURRENT_VEHICLE_SLOT] = g_EVENT_INDEX;
  g_CONTROL_INPUT_SEQUENCE_FRAME_LIMIT = VEHICLE_CONTROL_INPUT_SEQUENCE_INITIAL_FRAME_LIMIT;
  g_VEHICLE_TRAFFIC_COORDINATION_DISABLED                                     = 0;
  g_VEHICLE_TRAFFIC_LANE_CONSTRAINTS_ENABLED                                  = 1;
  g_VEHICLE_COLLISION_UPDATE_PENDING                                           = 0;
  g_VEHICLE_RESISTANCE_REDUCTION_ENABLED                                       = 0;
  g_VEHICLE_SPAWN_RANDOM_STATE.low_word = VEHICLE_TRACK_RANDOM_INITIAL_LOW_WORD;
  g_VEHICLE_GEAR_SHIFT_STATISTICS =
    (VehicleGearShiftStatistics *) &g_VEHICLE_DRIVE_STATISTICS.vehicle_records[g_EVENT_INDEX];

  initialize_vehicle_drive_statistics();
  special_resource_name = (char *) &g_VEHICLE_ACCELERATE_BUTTON_MASK;
  sprintf(parameter_path,
          VEHICLE_PARAMETER_PATH_FORMAT,
          g_GAME_DATA_PATH,
          VEHICLE_PARAMETER_DIRECTORY,
          special_resource_name,
          g_VEHICLE_SPAWN_CONFIGURATION_MODE);
  initialize_vehicle_spawn_configuration(&g_VEHICLE_SPAWN_CONFIGURATION,
                                         g_VEHICLE_SPAWN_CONFIGURATION_MODE);
  g_VEHICLE_DRIVE_STATISTICS.summary.lane_search_limit =
    g_VEHICLE_SPAWN_CONFIGURATION.lane_search_limit;
  if(special_resource_name[0] == VEHICLE_SPECIAL_RESOURCE_PREFIX)
    g_VEHICLE_SPAWN_CONFIGURATION.maximum_active_vehicle_count = 0;
  g_PLAYER_VEHICLE_ACCELERATION_RATE = VEHICLE_TRACK_RUNTIME_DEFAULT_SPEED;
}
