#include "vehicle_traffic_randomization.h"

#include "disk_stream_cleanup.h"
#include "text_line_reader.h"
#include "vehicle_simulation_state.h"

#include "operamath.h"

#include "stddef.h"

#define VEHICLE_STATUS_PLAYER_FLAG 0x00000004
#define VEHICLE_RANDOM_LOW_WORD_MASK 0x0000FFFF
#define VEHICLE_RANDOM_VALUE_MASK 0x00FFFFFF
#define VEHICLE_RANDOM_VALUE_SHIFT 8
#define VEHICLE_RANDOM_TABLE_INDEX_MASK 3
#define VEHICLE_TRACK_SPEED_VARIANCE 0x000D6666
#define VEHICLE_MINIMUM_RANDOMIZED_SPEED 0x0008E666
#define VEHICLE_SPAWN_MODE_LIGHT 0
#define VEHICLE_SPAWN_MODE_MEDIUM 1
#define VEHICLE_SPAWN_MODE_HEAVY 2
#define VEHICLE_SPAWN_REFRESH_FRAMES_PER_SECOND 60
#define VEHICLE_SPAWN_SPEED_AVERAGE_SCALE 0x471C
#define VEHICLE_SPAWN_LIGHT_TRAFFIC_COUNT 6
#define VEHICLE_SPAWN_MEDIUM_TRAFFIC_COUNT 12
#define VEHICLE_SPAWN_HEAVY_TRAFFIC_COUNT 20
#define VEHICLE_SPAWN_LIGHT_TRACK_ADJUSTMENT 0x30000
#define VEHICLE_SPAWN_MEDIUM_TRACK_ADJUSTMENT 0x3CCCC
#define VEHICLE_SPAWN_HEAVY_TRACK_ADJUSTMENT 0x48000
#define VEHICLE_SPAWN_LIGHT_MINIMUM_SPACING 30
#define VEHICLE_SPAWN_MEDIUM_MINIMUM_SPACING 20
#define VEHICLE_SPAWN_HEAVY_MINIMUM_SPACING 12
#define VEHICLE_SPAWN_HEAVY_DELAY_VARIANCE 20
#define VEHICLE_SPAWN_LIGHT_MAXIMUM_SPACING 0x230000
#define VEHICLE_SPAWN_STANDARD_MAXIMUM_SPACING 0x2B0000
#define VEHICLE_SPAWN_LIGHT_MINIMUM_SPEED 0x50000
#define VEHICLE_SPAWN_MEDIUM_MINIMUM_SPEED 0xA0000
#define VEHICLE_SPAWN_HEAVY_MINIMUM_SPEED 0x140000
#define VEHICLE_SPAWN_TRACK_POSITION_MARGIN 400
#define VEHICLE_SPAWN_PLACEMENT_ATTEMPTS 2
#define VEHICLE_SPAWN_MINIMUM_REFRESH_SECONDS 1
#define VEHICLE_SPAWN_MAXIMUM_REFRESH_SECONDS 2
#define VEHICLE_SPAWN_LANE_SEARCH_LIMIT 4
#define VEHICLE_SPAWN_LIGHT_ACTIVATION_RANGE 500
#define VEHICLE_SPAWN_MEDIUM_ACTIVATION_RANGE 300
#define VEHICLE_SPAWN_HEAVY_ACTIVATION_RANGE 170
#define VEHICLE_SPAWN_MAXIMUM_ACTIVE_COUNT 4
#define VEHICLE_SPAWN_LIGHT_SPEED_AVERAGE 0x640000
#define VEHICLE_SPAWN_MEDIUM_SPEED_AVERAGE 0x7D0000
#define VEHICLE_SPAWN_HEAVY_SPEED_AVERAGE 0x8C0000
#define VEHICLE_FIXED_NEGATIVE_ONE -0x10000
#define VEHICLE_FIXED_NEGATIVE_POINT_SEVEN -0x0B333
#define VEHICLE_FIXED_NEGATIVE_HALF -0x08000
#define VEHICLE_FIXED_POINT_ONE 0x01999
#define VEHICLE_FIXED_POINT_TWO 0x03333
#define VEHICLE_FIXED_POINT_THREE 0x04CCC
#define VEHICLE_FIXED_POINT_FOUR 0x06666
#define VEHICLE_FIXED_HALF 0x08000
#define VEHICLE_FIXED_POINT_FIVE_FIVE 0x08CCC
#define VEHICLE_FIXED_POINT_SIX 0x09999
#define VEHICLE_FIXED_POINT_SIX_FIVE 0x0A666
#define VEHICLE_FIXED_POINT_SEVEN 0x0B333
#define VEHICLE_FIXED_POINT_SEVEN_FIVE 0x0C000
#define VEHICLE_FIXED_POINT_EIGHT 0x0CCCC
#define VEHICLE_FIXED_POINT_EIGHT_FIVE 0x0D999
#define VEHICLE_FIXED_POINT_NINE 0x0E666
#define VEHICLE_FIXED_POINT_NINE_FIVE 0x0F333
#define VEHICLE_FIXED_ALMOST_ONE 0x0FFBE
#define VEHICLE_FIXED_ONE 0x10000
#define VEHICLE_FIXED_ONE_POINT_ZERO_FIVE 0x10CCC
#define VEHICLE_FIXED_ONE_POINT_ONE 0x11999
#define VEHICLE_FIXED_ONE_POINT_FIVE 0x18000
#define VEHICLE_FIXED_TWO 0x20000
#define VEHICLE_STEERING_RANDOM_THRESHOLD 40
#define VEHICLE_DEFAULT_TRAFFIC_STEERING_OFFSET (-0x58000)
#define VEHICLE_SPAWN_CONFIGURATION_ACTIVE_MINIMUM 0
#define VEHICLE_SPAWN_CONFIGURATION_ACTIVE_MAXIMUM 4

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

VehicleSpawnConfiguration g_VEHICLE_SPAWN_CONFIGURATION;
VehicleSpawnPositionState g_VEHICLE_SPAWN_POSITION_STATE;
VehicleSpawnRandomState   g_VEHICLE_SPAWN_RANDOM_STATE;
int32 g_VEHICLE_STEERING_RANDOM_THRESHOLDS[VEHICLE_STEERING_RANDOM_MODE_COUNT] = {
  VEHICLE_STEERING_RANDOM_THRESHOLD,
  VEHICLE_STEERING_RANDOM_THRESHOLD,
  VEHICLE_STEERING_RANDOM_THRESHOLD
};
uint8 g_VEHICLE_TRAFFIC_RANDOM_RESERVED_STATE[0x0C];

COMPILE_TIME_SIZE_ASSERT(vehicle_spawn_configuration_size,
                         sizeof(VehicleSpawnConfiguration) == 0xB0);
COMPILE_TIME_SIZE_ASSERT(vehicle_spawn_position_state_size,
                         sizeof(VehicleSpawnPositionState) == 0x10);
COMPILE_TIME_SIZE_ASSERT(vehicle_spawn_random_state_size, sizeof(VehicleSpawnRandomState) == 0x0C);

void
load_vehicle_spawn_configuration(const char *path_)
{
  Stream *stream;
  int32   index;

  stream = open_disk_stream_default((char *) path_);
  (void) read_noncomment_integer_text_line(stream);
  g_VEHICLE_SPAWN_CONFIGURATION.target_traffic_count = read_noncomment_integer_text_line(stream);
  g_VEHICLE_SPAWN_CONFIGURATION.maximum_track_position_adjustment =
    read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_SPAWN_CONFIGURATION.minimum_spawn_spacing   = read_noncomment_integer_text_line(stream);
  g_VEHICLE_SPAWN_CONFIGURATION.initial_spawn_delay     = read_noncomment_integer_text_line(stream);
  g_VEHICLE_SPAWN_CONFIGURATION.spawn_delay_variance    = read_noncomment_integer_text_line(stream);
  g_VEHICLE_SPAWN_CONFIGURATION.maximum_spawn_spacing   = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_SPAWN_CONFIGURATION.minimum_traffic_speed   = read_noncomment_fixed16_text_line(stream);
  g_VEHICLE_SPAWN_CONFIGURATION.track_position_margin   = read_noncomment_integer_text_line(stream);
  g_VEHICLE_SPAWN_CONFIGURATION.placement_attempt_count = read_noncomment_integer_text_line(stream);
  g_VEHICLE_SPAWN_CONFIGURATION.minimum_refresh_delay =
    read_noncomment_integer_text_line(stream) * VEHICLE_SPAWN_REFRESH_FRAMES_PER_SECOND;
  g_VEHICLE_SPAWN_CONFIGURATION.maximum_refresh_delay =
    read_noncomment_integer_text_line(stream) * VEHICLE_SPAWN_REFRESH_FRAMES_PER_SECOND;
  g_VEHICLE_SPAWN_CONFIGURATION.lane_search_limit = read_noncomment_integer_text_line(stream);
  g_VEHICLE_SPAWN_CONFIGURATION.activation_range  = read_noncomment_integer_text_line(stream);
  g_VEHICLE_SPAWN_CONFIGURATION.maximum_active_vehicle_count =
    read_noncomment_integer_text_line(stream);
  if(g_VEHICLE_SPAWN_CONFIGURATION.maximum_active_vehicle_count <
     VEHICLE_SPAWN_CONFIGURATION_ACTIVE_MINIMUM)
    {
      g_VEHICLE_SPAWN_CONFIGURATION.maximum_active_vehicle_count =
        VEHICLE_SPAWN_CONFIGURATION_ACTIVE_MINIMUM;
    }
  else if(g_VEHICLE_SPAWN_CONFIGURATION.maximum_active_vehicle_count >
          VEHICLE_SPAWN_CONFIGURATION_ACTIVE_MAXIMUM)
    {
      g_VEHICLE_SPAWN_CONFIGURATION.maximum_active_vehicle_count =
        VEHICLE_SPAWN_CONFIGURATION_ACTIVE_MAXIMUM;
    }

  for(index = 0; index < VEHICLE_TRAFFIC_RANDOM_TABLE_COUNT; ++index)
    {
      g_VEHICLE_SPAWN_CONFIGURATION.refresh_speeds[index] =
        read_noncomment_fixed16_text_line(stream);
    }
  g_VEHICLE_SPAWN_CONFIGURATION.speed_average =
    MulSF16(read_noncomment_fixed16_text_line(stream), VEHICLE_SPAWN_SPEED_AVERAGE_SCALE);
  for(index = 0; index < VEHICLE_TRAFFIC_RANDOM_TABLE_COUNT; ++index)
    {
      g_VEHICLE_SPAWN_CONFIGURATION.steering_offsets[index] =
        read_noncomment_fixed16_text_line(stream);
    }
  for(index = 0; index < VEHICLE_TRAFFIC_SPEED_SCALE_COUNT; ++index)
    {
      g_VEHICLE_SPAWN_CONFIGURATION.track_speed_scales[index] =
        read_noncomment_fixed16_text_line(stream);
    }

  (void) close_disk_stream_and_return_zero(stream);
}

void
initialize_vehicle_spawn_configuration(VehicleSpawnConfiguration *configuration_, int32 mode_)
{
  int32 *refresh_speed;
  int32 *steering_offset;
  int32 *track_speed_scale;

  refresh_speed     = configuration_->refresh_speeds;
  steering_offset   = configuration_->steering_offsets;
  track_speed_scale = configuration_->track_speed_scales;
  if(mode_ == VEHICLE_SPAWN_MODE_LIGHT)
    {
      configuration_->target_traffic_count              = VEHICLE_SPAWN_LIGHT_TRAFFIC_COUNT;
      configuration_->maximum_track_position_adjustment = VEHICLE_SPAWN_LIGHT_TRACK_ADJUSTMENT;
      configuration_->minimum_spawn_spacing             = VEHICLE_SPAWN_LIGHT_MINIMUM_SPACING;
      configuration_->initial_spawn_delay               = 0;
      configuration_->spawn_delay_variance              = 0;
      configuration_->maximum_spawn_spacing             = VEHICLE_SPAWN_LIGHT_MAXIMUM_SPACING;
      configuration_->minimum_traffic_speed             = VEHICLE_SPAWN_LIGHT_MINIMUM_SPEED;
      configuration_->track_position_margin             = VEHICLE_SPAWN_TRACK_POSITION_MARGIN;
      configuration_->placement_attempt_count           = VEHICLE_SPAWN_PLACEMENT_ATTEMPTS;
      configuration_->minimum_refresh_delay             = VEHICLE_SPAWN_MINIMUM_REFRESH_SECONDS;
      configuration_->maximum_refresh_delay             = VEHICLE_SPAWN_MAXIMUM_REFRESH_SECONDS;
      configuration_->lane_search_limit                 = VEHICLE_SPAWN_LANE_SEARCH_LIMIT;
      configuration_->activation_range                  = VEHICLE_SPAWN_LIGHT_ACTIVATION_RANGE;
      configuration_->maximum_active_vehicle_count      = VEHICLE_SPAWN_MAXIMUM_ACTIVE_COUNT;
      *refresh_speed++                                  = VEHICLE_FIXED_ONE;
      *refresh_speed++                                  = VEHICLE_FIXED_POINT_NINE;
      *refresh_speed++                                  = VEHICLE_FIXED_POINT_NINE;
      *refresh_speed                                    = VEHICLE_FIXED_ONE;
      configuration_->speed_average                     = VEHICLE_SPAWN_LIGHT_SPEED_AVERAGE;
      *steering_offset++                                = VEHICLE_FIXED_HALF;
      *steering_offset++                                = VEHICLE_FIXED_NEGATIVE_HALF;
      *steering_offset++                                = VEHICLE_FIXED_POINT_TWO;
      *steering_offset                                  = VEHICLE_FIXED_POINT_THREE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE_POINT_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE_POINT_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_NINE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_NINE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_EIGHT_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_EIGHT_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_EIGHT;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_EIGHT;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_EIGHT_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_EIGHT;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_SEVEN;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_SIX;
      *track_speed_scale++                              = VEHICLE_FIXED_HALF;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_THREE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_THREE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_TWO;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_ONE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_ONE;
      *track_speed_scale                                = VEHICLE_FIXED_POINT_ONE;
    }
  else if(mode_ == VEHICLE_SPAWN_MODE_MEDIUM)
    {
      configuration_->target_traffic_count              = VEHICLE_SPAWN_MEDIUM_TRAFFIC_COUNT;
      configuration_->maximum_track_position_adjustment = VEHICLE_SPAWN_MEDIUM_TRACK_ADJUSTMENT;
      configuration_->minimum_spawn_spacing             = VEHICLE_SPAWN_MEDIUM_MINIMUM_SPACING;
      configuration_->initial_spawn_delay               = 0;
      configuration_->spawn_delay_variance              = 0;
      configuration_->maximum_spawn_spacing             = VEHICLE_SPAWN_STANDARD_MAXIMUM_SPACING;
      configuration_->minimum_traffic_speed             = VEHICLE_SPAWN_MEDIUM_MINIMUM_SPEED;
      configuration_->track_position_margin             = VEHICLE_SPAWN_TRACK_POSITION_MARGIN;
      configuration_->placement_attempt_count           = VEHICLE_SPAWN_PLACEMENT_ATTEMPTS;
      configuration_->minimum_refresh_delay             = VEHICLE_SPAWN_MINIMUM_REFRESH_SECONDS;
      configuration_->maximum_refresh_delay             = VEHICLE_SPAWN_MAXIMUM_REFRESH_SECONDS;
      configuration_->lane_search_limit                 = VEHICLE_SPAWN_LANE_SEARCH_LIMIT;
      configuration_->activation_range                  = VEHICLE_SPAWN_MEDIUM_ACTIVATION_RANGE;
      configuration_->maximum_active_vehicle_count      = VEHICLE_SPAWN_MAXIMUM_ACTIVE_COUNT;
      *refresh_speed++                                  = VEHICLE_FIXED_ONE;
      *refresh_speed++                                  = VEHICLE_FIXED_POINT_NINE;
      *refresh_speed++                                  = VEHICLE_FIXED_POINT_EIGHT;
      *refresh_speed                                    = VEHICLE_FIXED_POINT_SEVEN;
      configuration_->speed_average                     = VEHICLE_SPAWN_MEDIUM_SPEED_AVERAGE;
      *steering_offset++                                = VEHICLE_FIXED_POINT_SEVEN;
      *steering_offset++                                = VEHICLE_FIXED_NEGATIVE_POINT_SEVEN;
      *steering_offset++                                = VEHICLE_FIXED_HALF;
      *steering_offset                                  = 0;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE_POINT_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE_POINT_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE_POINT_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE_POINT_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE_POINT_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_NINE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_EIGHT_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_EIGHT_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_NINE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_NINE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_EIGHT_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_EIGHT;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_SEVEN;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_SIX;
      *track_speed_scale++                              = VEHICLE_FIXED_HALF;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_FOUR;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_FOUR;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_THREE;
      *track_speed_scale                                = VEHICLE_FIXED_POINT_THREE;
    }
  else if(mode_ == VEHICLE_SPAWN_MODE_HEAVY)
    {
      configuration_->target_traffic_count              = VEHICLE_SPAWN_HEAVY_TRAFFIC_COUNT;
      configuration_->maximum_track_position_adjustment = VEHICLE_SPAWN_HEAVY_TRACK_ADJUSTMENT;
      configuration_->minimum_spawn_spacing             = VEHICLE_SPAWN_HEAVY_MINIMUM_SPACING;
      configuration_->initial_spawn_delay               = 0;
      configuration_->spawn_delay_variance              = VEHICLE_SPAWN_HEAVY_DELAY_VARIANCE;
      configuration_->maximum_spawn_spacing             = VEHICLE_SPAWN_STANDARD_MAXIMUM_SPACING;
      configuration_->minimum_traffic_speed             = VEHICLE_SPAWN_HEAVY_MINIMUM_SPEED;
      configuration_->track_position_margin             = VEHICLE_SPAWN_TRACK_POSITION_MARGIN;
      configuration_->placement_attempt_count           = VEHICLE_SPAWN_PLACEMENT_ATTEMPTS;
      configuration_->minimum_refresh_delay             = VEHICLE_SPAWN_MINIMUM_REFRESH_SECONDS;
      configuration_->maximum_refresh_delay             = VEHICLE_SPAWN_MAXIMUM_REFRESH_SECONDS;
      configuration_->lane_search_limit                 = VEHICLE_SPAWN_LANE_SEARCH_LIMIT;
      configuration_->activation_range                  = VEHICLE_SPAWN_HEAVY_ACTIVATION_RANGE;
      configuration_->maximum_active_vehicle_count      = VEHICLE_SPAWN_MAXIMUM_ACTIVE_COUNT;
      *refresh_speed++                                  = VEHICLE_FIXED_ONE;
      *refresh_speed++                                  = VEHICLE_FIXED_POINT_NINE;
      *refresh_speed++                                  = VEHICLE_FIXED_POINT_SEVEN;
      *refresh_speed                                    = VEHICLE_FIXED_POINT_SIX;
      configuration_->speed_average                     = VEHICLE_SPAWN_HEAVY_SPEED_AVERAGE;
      *steering_offset++                                = VEHICLE_FIXED_ONE;
      *steering_offset++                                = VEHICLE_FIXED_NEGATIVE_ONE;
      *steering_offset++                                = VEHICLE_FIXED_HALF;
      *steering_offset                                  = VEHICLE_FIXED_POINT_EIGHT;
      *track_speed_scale++                              = VEHICLE_FIXED_TWO;
      *track_speed_scale++                              = VEHICLE_FIXED_TWO;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE_POINT_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE_POINT_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE_POINT_ONE;
      *track_speed_scale++                              = VEHICLE_FIXED_ONE_POINT_ZERO_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_ALMOST_ONE;
      *track_speed_scale++                              = VEHICLE_FIXED_ALMOST_ONE;
      *track_speed_scale++                              = VEHICLE_FIXED_ALMOST_ONE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_NINE_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_NINE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_SEVEN_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_SIX_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_SIX_FIVE;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_SIX;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_SIX;
      *track_speed_scale++                              = VEHICLE_FIXED_POINT_FIVE_FIVE;
      *track_speed_scale                                = VEHICLE_FIXED_HALF;
    }

  configuration_->minimum_refresh_delay *= VEHICLE_SPAWN_REFRESH_FRAMES_PER_SECOND;
  configuration_->maximum_refresh_delay *= VEHICLE_SPAWN_REFRESH_FRAMES_PER_SECOND;
  configuration_->speed_average =
    MulSF16(configuration_->speed_average, VEHICLE_SPAWN_SPEED_AVERAGE_SCALE);
}

int32
randomize_vehicle_track_speed(SimulationSource *source_, int32 speed_)
{
  VehicleSimulationRefreshState *refresh_state;
  int32                          randomized_speed;
  int32                          minimum_speed;

  refresh_state    = (VehicleSimulationRefreshState *) source_->refresh_state;
  randomized_speed = MulSF16(speed_, refresh_state->randomized_speed);
  if(speed_ <= 0)
    return randomized_speed;

  minimum_speed = (speed_ - VEHICLE_TRACK_SPEED_VARIANCE);
  if(randomized_speed < minimum_speed)
    randomized_speed = minimum_speed;
  if(randomized_speed < VEHICLE_MINIMUM_RANDOMIZED_SPEED)
    randomized_speed = VEHICLE_MINIMUM_RANDOMIZED_SPEED;
  return randomized_speed;
}

void
randomize_vehicle_refresh_speed(SimulationSource *source_)
{
  VehicleSimulationRefreshState *refresh_state;
  uint32                         product;
  uint32                         table_index;

  product = (g_VEHICLE_SPAWN_RANDOM_STATE.low_word * g_VEHICLE_SPAWN_RANDOM_STATE.multiplier);
  g_VEHICLE_SPAWN_RANDOM_STATE.product  = product;
  g_VEHICLE_SPAWN_RANDOM_STATE.low_word = (product & VEHICLE_RANDOM_LOW_WORD_MASK);
  table_index = ((product & VEHICLE_RANDOM_VALUE_MASK) >> VEHICLE_RANDOM_VALUE_SHIFT);
  table_index &= VEHICLE_RANDOM_TABLE_INDEX_MASK;

  refresh_state                   = (VehicleSimulationRefreshState *) source_->refresh_state;
  refresh_state->randomized_speed = g_VEHICLE_SPAWN_CONFIGURATION.refresh_speeds[table_index];
}

void
randomize_vehicle_steering_offset(SimulationSource *source_)
{
  uint32 product;
  uint32 table_index;

  if((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) != 0)
    {
      source_->direction.z = 0;
      return;
    }

  product = (g_VEHICLE_SPAWN_RANDOM_STATE.low_word * g_VEHICLE_SPAWN_RANDOM_STATE.multiplier);
  g_VEHICLE_SPAWN_RANDOM_STATE.product  = product;
  g_VEHICLE_SPAWN_RANDOM_STATE.low_word = (product & VEHICLE_RANDOM_LOW_WORD_MASK);
  table_index = ((product & VEHICLE_RANDOM_VALUE_MASK) >> VEHICLE_RANDOM_VALUE_SHIFT);
  table_index &= VEHICLE_RANDOM_TABLE_INDEX_MASK;
  source_->direction.z = g_VEHICLE_SPAWN_CONFIGURATION.steering_offsets[table_index];
}

void
initialize_vehicle_steering_offset(SimulationSource *source_)
{
  if((source_->status_flags & VEHICLE_STATUS_PLAYER_FLAG) != 0)
    source_->direction.z = 0;
  else
    source_->direction.z = VEHICLE_DEFAULT_TRAFFIC_STEERING_OFFSET;
}
