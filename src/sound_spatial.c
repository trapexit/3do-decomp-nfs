#include "sound_spatial.h"

#include "audio_voice.h"
#include "game_bootstrap.h"
#include "integer_division.h"
#include "random_number.h"
#include "simulation_snapshot.h"
#include "simulation_thread_signal.h"
#include "sound_effect_resources.h"
#include "sound_effect_slots.h"
#include "sound_effect_state.h"
#include "vehicle_dynamics_state.h"
#include "vehicle_parameter_loader.h"
#include "vehicle_runtime_pool.h"
#include "vehicle_selection_state.h"
#include "vehicle_simulation_state.h"

#include "debug.h"

#include "stdio.h"

#define SOUND_ANGLE_FIXED_SHIFT 16
#define SOUND_ANGLE_QUARTER_TURN 0x40
#define SOUND_ANGLE_REAR_START 0xC0
#define SOUND_ANGLE_FULL_TURN 0x100

#define SOUND_CLASS_EFFECT 2
#define SOUND_CLASS_VEHICLE 4

#define SOUND_VARIANT_PRIMARY 1
#define SOUND_VARIANT_SECONDARY 7

#define SOUND_INSTRUMENT_EFFECT 17
#define SOUND_INSTRUMENT_PRIMARY 20
#define SOUND_INSTRUMENT_SECONDARY 13

#define SOUND_DISTANCE_LIMIT 1225
#define SOUND_GAIN_MAX 32767
#define SOUND_POSITION_FIXED_SHIFT 8
#define SOUND_POSITION_MINOR_DISTANCE_SHIFT 2
#define SOUND_POSITION_VERTICAL_DISTANCE_SHIFT 8
#define SOUND_POSITION_FAR_VERTICAL_SHIFT 2
#define SOUND_POSITION_NEAR_VERTICAL_SHIFT 10
#define SOUND_POSITION_SQUARE_LIMIT 0x4000
#define SOUND_POSITION_FAR_DISTANCE 0x1001
#define SOUND_POSITION_SQUARE_SHIFT 16
#define SOUND_POSITION_ANGLE_FULL_TURN 0x1000000
#define SOUND_SCALED_GAIN_MULTIPLIER 20
#define SOUND_ATTENUATED_GAIN_SHIFT 5

#define SOUND_GAIN_FIXED_SHIFT 16
#define SOUND_GAIN_LOW_LIMIT 16
#define SOUND_GAIN_MID_LIMIT 22
#define SOUND_GAIN_HIGH_LIMIT 80
#define SOUND_GAIN_DEFAULT_MID 60
#define SOUND_GAIN_DEFAULT_HIGH 110

#define SOUND_CLASS_ENGINE_PRIMARY 2
#define SOUND_CLASS_ENGINE_SECONDARY 4
#define SOUND_CLASS_FIXED_ONE 10
#define SOUND_CLASS_FIXED_TWO 11
#define SOUND_CLASS_FIXED_THREE 12
#define SOUND_CLASS_FIXED_FOUR 13

#define SOUND_VARIANT_LOW 1
#define SOUND_VARIANT_FIXED_ONE 8
#define SOUND_VARIANT_FIXED_TWO 9

#define SOUND_SAMPLE_FIXED_FOUR 1
#define SOUND_SAMPLE_FIXED_THREE 7
#define SOUND_SAMPLE_LOW 9
#define SOUND_SAMPLE_HIGH 10
#define SOUND_SAMPLE_BASE 11
#define SOUND_SAMPLE_SECONDARY 15
#define SOUND_SAMPLE_FIXED_ONE 16
#define SOUND_SAMPLE_FIXED_TWO 18

#define SPATIAL_SOUND_GAIN_SHIFT 16
#define SPATIAL_SOUND_FREQUENCY_FINE_SHIFT 13
#define SPATIAL_SOUND_FREQUENCY_COARSE_SHIFT 14
#define SPATIAL_SOUND_FINE_SCALE_SHIFT 7
#define SPATIAL_SOUND_COARSE_SCALE_SHIFT 8
#define SPATIAL_SOUND_ACTIVE_GAIN_SHIFT 8
#define SPATIAL_SOUND_ENGINE_ID 6
#define SPATIAL_SOUND_ENGINE_SLOT_INDEX 6
#define SPATIAL_SOUND_FIRST_TERTIARY_ID 100
#define SPATIAL_SOUND_CACHE_ID_OFFSET 99
#define SPATIAL_SOUND_INSTRUMENT_FINE 17
#define SPATIAL_SOUND_INSTRUMENT_COARSE 20
#define SPATIAL_SOUND_DEFAULT_FREQUENCY 0x2000
#define SPATIAL_SOUND_LOW_SPEED_LIMIT 20
#define SPATIAL_SOUND_FREQUENCY_CACHE_COUNT 21
#define SPATIAL_SOUND_CHANNEL_100 100
#define SPATIAL_SOUND_CHANNEL_101 101
#define SPATIAL_SOUND_LAST_DOLBY_SLOT 3
#define SPATIAL_SOUND_ACTIVE 1
#define SPATIAL_SOUND_AMPLITUDE_SMOOTHING 2
#define SPATIAL_SOUND_ALTERNATING_VOICE_FIRST 6
#define SPATIAL_SOUND_ALTERNATING_VOICE_SECOND 7
#define SPATIAL_SOUND_DISABLED_FRONTEND_MODE 3
#define SPATIAL_SOUND_RANDOM_DIRECTION_COUNT 2
#define SPATIAL_SOUND_RANDOM_FREQUENCY_BASE 0x2000
#define SPATIAL_SOUND_GAIN_MAXIMUM 0x80000
#define SPATIAL_SOUND_AMPLITUDE_PRODUCT_SHIFT 4
#define SPATIAL_SOUND_AMPLITUDE_DIVISOR 0x800
#define SPATIAL_SOUND_AMPLITUDE_BIAS 0x1194
#define SPATIAL_SOUND_AMPLITUDE_MAXIMUM 0x7FFF
#define SPATIAL_SOUND_FREQUENCY_EMPTY 0
#define SPATIAL_SOUND_FREQUENCY_2000 0x07D0
#define SPATIAL_SOUND_FREQUENCY_2800 0x0AF0
#define SPATIAL_SOUND_FREQUENCY_3000 0x0BB8
#define SPATIAL_SOUND_FREQUENCY_3500 0x0DAC
#define SPATIAL_SOUND_FREQUENCY_3800 0x0ED8
#define SPATIAL_SOUND_FREQUENCY_9500 0x251C
#define SPATIAL_SOUND_FREQUENCY_10000 0x2710
#define SPATIAL_SOUND_FREQUENCY_11000 0x2AF8
#define SPATIAL_SOUND_FREQUENCY_12000 0x2EE0

#define VEHICLE_PROXIMITY_ALERT_RATE_COUNT 7
#define VEHICLE_PROXIMITY_ALERT_DELAY_0 1
#define VEHICLE_PROXIMITY_ALERT_DELAY_1 20
#define VEHICLE_PROXIMITY_ALERT_DELAY_2 15
#define VEHICLE_PROXIMITY_ALERT_DELAY_3 10
#define VEHICLE_PROXIMITY_ALERT_DELAY_4 5
#define VEHICLE_PROXIMITY_ALERT_DELAY_5 3
#define VEHICLE_PROXIMITY_ALERT_DELAY_6 1
#define VEHICLE_PROXIMITY_ALERT_VOICE 7
#define VEHICLE_PROXIMITY_ALERT_SAMPLE 14
#define VEHICLE_PROXIMITY_ALERT_FREQUENCY 0x4000
#define VEHICLE_PROXIMITY_ALERT_AMPLITUDE 0x1FFF
#define VEHICLE_PROXIMITY_ALERT_PAN 0x40
#define VEHICLE_AUDIO_SPEED_FIXED_SHIFT 16
#define VEHICLE_AUDIO_SURFACE_FREQUENCY_SHIFT 9
#define VEHICLE_AUDIO_GAIN_MAXIMUM 0x7FFF
#define VEHICLE_AUDIO_GAIN_LIMIT 0x8000
#define VEHICLE_AUDIO_DEFAULT_FREQUENCY 0x2000
#define VEHICLE_AUDIO_LOW_FREQUENCY 0x1000
#define VEHICLE_AUDIO_HIGH_FREQUENCY 0x4000
#define VEHICLE_AUDIO_OSCILLATION_LOW 0x2001
#define VEHICLE_AUDIO_OSCILLATION_SCALE 0x80
#define VEHICLE_AUDIO_SPECIAL_GAIN_SCALE 4
#define VEHICLE_AUDIO_ALTERNATE_GAIN_SCALE 8
#define VEHICLE_AUDIO_SURFACE_VOICE_LEFT 4
#define VEHICLE_AUDIO_SURFACE_VOICE_RIGHT 5
#define VEHICLE_AUDIO_SURFACE_FREQUENCY_OFFSET 0x0F
#define VEHICLE_AUDIO_ENGINE_VOICE_FIRST 0
#define VEHICLE_AUDIO_ENGINE_VOICE_SECOND 1
#define VEHICLE_AUDIO_ENGINE_VOICE_THIRD 2
#define VEHICLE_AUDIO_ENGINE_VOICE_FOURTH 3
#define VEHICLE_AUDIO_FREQUENCY_TABLE_BASE 5
#define VEHICLE_AUDIO_FREQUENCY_TABLE_STRIDE 2
#define VEHICLE_AUDIO_PAN_DECREASE_SHIFT 3
#define VEHICLE_AUDIO_PAN_INCREASE_SHIFT 1
#define VEHICLE_AUDIO_PAN_ACCELERATION_SHIFT 6
#define VEHICLE_AUDIO_PAN_MAXIMUM 0xFF
#define VEHICLE_AUDIO_DISTANCE_GAIN_SHIFT 8
#define VEHICLE_AUDIO_GAIN_PRODUCT_SHIFT 7
#define VEHICLE_AUDIO_PRIMARY_SOUND_ID 6
#define VEHICLE_AUDIO_SECONDARY_SOUND_ID 103
#define VEHICLE_AUDIO_DEFAULT_SOUND_CLASS 4
#define VEHICLE_AUDIO_PRIMARY_VARIANT 1
#define VEHICLE_AUDIO_SECONDARY_VARIANT 7
#define VEHICLE_AUDIO_HEADING_SCALE -6
#define VEHICLE_AUDIO_ANGLE_FULL_TURN 0x1000000
#define VEHICLE_AUDIO_ANGLE_HALF_TURN 0x800000
#define TRAFFIC_VEHICLE_SOUND_BLOCKED_FLAGS 0x000C0000
#define TRAFFIC_VEHICLE_SOUND_MINIMUM_FRAMES 5
#define TRAFFIC_VEHICLE_SOUND_RANDOM_FRAMES 15
#define TRAFFIC_VEHICLE_SOUND_INSTRUMENT_SPECIAL 22
#define TRAFFIC_VEHICLE_SOUND_INSTRUMENT_STANDARD 18
#define TRAFFIC_VEHICLE_SOUND_CHANNEL 0x66
#define TRAFFIC_VEHICLE_SOUND_STOP_INSTRUMENT 0x12
#define TRAFFIC_VEHICLE_SOUND_GAIN_SHIFT 8
#define TRAFFIC_VEHICLE_SOUND_PAN_SHIFT 16

#define SOUND_SPATIAL_SIZE_ASSERT(name_, expression_)                                              \
  typedef char sound_spatial_size_assert_##name_[(expression_) ? 1 : -1]

#define SPATIAL_SOUND_TUNING_BASE_FREQUENCY 0x05DC
#define SPATIAL_SOUND_TUNING_FINE_FREQUENCY 0x13B8
#define SPATIAL_SOUND_TUNING_COARSE_FREQUENCY 0x1F40
#define SPATIAL_SOUND_TUNING_ALTERNATE_FINE 0x1F70
#define SPATIAL_SOUND_TUNING_ALTERNATE_COARSE 0x2328
#define SPATIAL_SOUND_TUNING_COARSE_SHIFT 8
#define SPATIAL_SOUND_TUNING_ACTIVE_SCALE 1
#define SPATIAL_SOUND_TUNING_ACTIVE_SHIFT 13

typedef struct SpatialSoundTuning
{
  int32 base_frequency;
  int32 fine_frequency;
  int32 coarse_frequency;
  int32 alternate_fine_frequency;
  int32 alternate_coarse_frequency;
  int32 coarse_shift;
  int32 active_frequency_scale;
  int32 active_frequency_shift;
} SpatialSoundTuning;

int32 g_SPATIAL_SOUND_FREQUENCY_CACHE[SPATIAL_SOUND_FREQUENCY_CACHE_COUNT] = {
  SPATIAL_SOUND_FREQUENCY_EMPTY, SPATIAL_SOUND_FREQUENCY_EMPTY, SPATIAL_SOUND_FREQUENCY_EMPTY,
  SPATIAL_SOUND_FREQUENCY_EMPTY, SPATIAL_SOUND_FREQUENCY_EMPTY, SPATIAL_SOUND_FREQUENCY_11000,
  SPATIAL_SOUND_FREQUENCY_2000,  SPATIAL_SOUND_FREQUENCY_11000, SPATIAL_SOUND_FREQUENCY_3800,
  SPATIAL_SOUND_FREQUENCY_9500,  SPATIAL_SOUND_FREQUENCY_3500,  SPATIAL_SOUND_FREQUENCY_12000,
  SPATIAL_SOUND_FREQUENCY_2000,  SPATIAL_SOUND_FREQUENCY_12000, SPATIAL_SOUND_FREQUENCY_2000,
  SPATIAL_SOUND_FREQUENCY_10000, SPATIAL_SOUND_FREQUENCY_3000,  SPATIAL_SOUND_FREQUENCY_12000,
  SPATIAL_SOUND_FREQUENCY_2800,  SPATIAL_SOUND_FREQUENCY_11000, SPATIAL_SOUND_FREQUENCY_2800
};
SpatialSoundTuning g_SPATIAL_SOUND_TUNING = {
  SPATIAL_SOUND_TUNING_BASE_FREQUENCY,   SPATIAL_SOUND_TUNING_FINE_FREQUENCY,
  SPATIAL_SOUND_TUNING_COARSE_FREQUENCY, SPATIAL_SOUND_TUNING_ALTERNATE_FINE,
  SPATIAL_SOUND_TUNING_ALTERNATE_COARSE, SPATIAL_SOUND_TUNING_COARSE_SHIFT,
  SPATIAL_SOUND_TUNING_ACTIVE_SCALE,     SPATIAL_SOUND_TUNING_ACTIVE_SHIFT
};
int32             g_TRAFFIC_VEHICLE_SOUND_MINIMUM_FRAMES = TRAFFIC_VEHICLE_SOUND_MINIMUM_FRAMES;
int32             g_TRAFFIC_VEHICLE_SOUND_RANDOM_FRAMES  = TRAFFIC_VEHICLE_SOUND_RANDOM_FRAMES;
SimulationSource *g_SPATIAL_SOUND_SOURCE;
int32             g_SPATIAL_SOUND_CHANNEL_100_AMPLITUDE;
int32             g_SPATIAL_SOUND_CHANNEL_101_AMPLITUDE;
int32             g_SPATIAL_SOUND_ALTERNATING_VOICE = SPATIAL_SOUND_ALTERNATING_VOICE_SECOND;
int32             g_TRAFFIC_VEHICLE_SOUND_VARIANT;
int32             g_TRAFFIC_VEHICLE_SOUND_COUNTDOWN;
int32             g_TRAFFIC_VEHICLE_SOUND_SOURCE_INDEX;
const int32       G_TRAFFIC_VEHICLE_SOUND_INSTRUMENTS[TRAFFIC_VEHICLE_SOUND_TABLE_COUNT] = {
  TRAFFIC_VEHICLE_SOUND_INSTRUMENT_SPECIAL,  TRAFFIC_VEHICLE_SOUND_INSTRUMENT_SPECIAL,
  TRAFFIC_VEHICLE_SOUND_INSTRUMENT_STANDARD, TRAFFIC_VEHICLE_SOUND_INSTRUMENT_STANDARD,
  TRAFFIC_VEHICLE_SOUND_INSTRUMENT_STANDARD, TRAFFIC_VEHICLE_SOUND_INSTRUMENT_STANDARD,
  TRAFFIC_VEHICLE_SOUND_INSTRUMENT_STANDARD, TRAFFIC_VEHICLE_SOUND_INSTRUMENT_STANDARD,
  TRAFFIC_VEHICLE_SOUND_INSTRUMENT_STANDARD, TRAFFIC_VEHICLE_SOUND_INSTRUMENT_STANDARD
};
const int32 G_TRAFFIC_VEHICLE_SOUND_AMPLITUDES[TRAFFIC_VEHICLE_SOUND_TABLE_COUNT] = {
  0x00002000, 0x00001600, 0x00001900, 0x00001C00, 0x00002000,
  0x00002300, 0x00002600, 0x00002A00, 0x00002C00, 0x00002F00
};
int32 g_VEHICLE_AUDIO_GAIN_TABLE[256] = {
  0x00000000, 0x000001ED, 0x000003CA, 0x00000598, 0x00000757, 0x00000908, 0x00000AAD, 0x00000C45,
  0x00000DD3, 0x00000F55, 0x000010CD, 0x0000123C, 0x000013A1, 0x000014FD, 0x00001652, 0x0000179E,
  0x000018E2, 0x00001A20, 0x00001B56, 0x00001C86, 0x00001DB0, 0x00001ED3, 0x00001FF1, 0x00002109,
  0x0000221B, 0x00002329, 0x00002431, 0x00002535, 0x00002633, 0x0000272E, 0x00002824, 0x00002916,
  0x00002A04, 0x00002AEE, 0x00002BD4, 0x00002CB6, 0x00002D95, 0x00002E71, 0x00002F49, 0x0000301E,
  0x000030EF, 0x000031BE, 0x0000328A, 0x00003353, 0x00003419, 0x000034DC, 0x0000359D, 0x0000365B,
  0x00003717, 0x000037D0, 0x00003887, 0x0000393C, 0x000039EE, 0x00003A9E, 0x00003B4C, 0x00003BF8,
  0x00003CA1, 0x00003D49, 0x00003DEF, 0x00003E93, 0x00003F35, 0x00003FD5, 0x00004073, 0x00004110,
  0x000041AB, 0x00004244, 0x000042DC, 0x00004372, 0x00004406, 0x00004499, 0x0000452B, 0x000045BB,
  0x00004649, 0x000046D7, 0x00004762, 0x000047ED, 0x00004876, 0x000048FD, 0x00004984, 0x00004A09,
  0x00004A8D, 0x00004B10, 0x00004B91, 0x00004C12, 0x00004C91, 0x00004D0F, 0x00004D8C, 0x00004E08,
  0x00004E83, 0x00004EFD, 0x00004F76, 0x00004FED, 0x00005064, 0x000050DA, 0x0000514F, 0x000051C2,
  0x00005235, 0x000052A7, 0x00005319, 0x00005389, 0x000053F8, 0x00005466, 0x000054D4, 0x00005541,
  0x000055AD, 0x00005618, 0x00005682, 0x000056EC, 0x00005755, 0x000057BD, 0x00005824, 0x0000588A,
  0x000058F0, 0x00005955, 0x000059B9, 0x00005A1D, 0x00005A80, 0x00005AE2, 0x00005B44, 0x00005BA5,
  0x00005C05, 0x00005C65, 0x00005CC4, 0x00005D22, 0x00005D80, 0x00005DDD, 0x00005E39, 0x00005E95,
  0x00005EF1, 0x00005F4B, 0x00005FA5, 0x00005FFF, 0x00006058, 0x000060B1, 0x00006108, 0x00006160,
  0x000061B7, 0x0000620D, 0x00006263, 0x000062B8, 0x0000630D, 0x00006361, 0x000063B5, 0x00006408,
  0x0000645B, 0x000064AE, 0x000064FF, 0x00006551, 0x000065A2, 0x000065F2, 0x00006642, 0x00006692,
  0x000066E1, 0x00006730, 0x0000677E, 0x000067CC, 0x00006819, 0x00006866, 0x000068B3, 0x000068FF,
  0x0000694B, 0x00006996, 0x000069E1, 0x00006A2B, 0x00006A76, 0x00006ABF, 0x00006B09, 0x00006B52,
  0x00006B9B, 0x00006BE3, 0x00006C2B, 0x00006C72, 0x00006CBA, 0x00006D00, 0x00006D47, 0x00006D8D,
  0x00006DD3, 0x00006E18, 0x00006E5E, 0x00006EA2, 0x00006EE7, 0x00006F2B, 0x00006F6F, 0x00006FB2,
  0x00006FF5, 0x00007038, 0x0000707B, 0x000070BD, 0x000070FF, 0x00007141, 0x00007182, 0x000071C3,
  0x00007204, 0x00007244, 0x00007284, 0x000072C4, 0x00007304, 0x00007343, 0x00007382, 0x000073C1,
  0x000073FF, 0x0000743D, 0x0000747B, 0x000074B9, 0x000074F6, 0x00007534, 0x00007570, 0x000075AD,
  0x000075E9, 0x00007625, 0x00007661, 0x0000769D, 0x000076D8, 0x00007713, 0x0000774E, 0x00007789,
  0x000077C3, 0x000077FD, 0x00007837, 0x00007871, 0x000078AA, 0x000078E3, 0x0000791C, 0x00007955,
  0x0000798E, 0x000079C6, 0x000079FE, 0x00007A36, 0x00007A6D, 0x00007AA5, 0x00007ADC, 0x00007B13,
  0x00007B4A, 0x00007B80, 0x00007BB7, 0x00007BED, 0x00007C23, 0x00007C58, 0x00007C8E, 0x00007CC3,
  0x00007CF8, 0x00007D2D, 0x00007D62, 0x00007D97, 0x00007DCB, 0x00007DFF, 0x00007E33, 0x00007E67,
  0x00007E9B, 0x00007ECE, 0x00007F01, 0x00007F34, 0x00007F67, 0x00007F9A, 0x00007FCC, 0x00007FFF
};
SOUND_SPATIAL_SIZE_ASSERT(vehicle_audio_gain_table_size,
                          sizeof(g_VEHICLE_AUDIO_GAIN_TABLE) == 0x400);
SOUND_SPATIAL_SIZE_ASSERT(traffic_vehicle_sound_instruments_size,
                          sizeof(G_TRAFFIC_VEHICLE_SOUND_INSTRUMENTS) == 0x28);
SOUND_SPATIAL_SIZE_ASSERT(traffic_vehicle_sound_amplitudes_size,
                          sizeof(G_TRAFFIC_VEHICLE_SOUND_AMPLITUDES) == 0x28);
uint8 g_VEHICLE_PROXIMITY_ALERT_ACTIVE;
uint8 g_VEHICLE_PROXIMITY_ALERT_PADDING[3];
int32 g_VEHICLE_PROXIMITY_ALERT_COUNTDOWN;
int32 g_VEHICLE_AUDIO_PAN_INDEX;
int32 g_VEHICLE_SURFACE_AUDIO_FREQUENCY            = VEHICLE_AUDIO_DEFAULT_FREQUENCY;
uint8 g_VEHICLE_SURFACE_AUDIO_FREQUENCY_INCREASING = 1;
uint8 g_VEHICLE_SURFACE_AUDIO_FREQUENCY_PADDING[3];

extern void
update_player_vehicle_state(void *state_);

int32
select_sound_instrument_index(int32 sound_id_, int32 sound_class_, int32 variant_)
{
  (void) sound_id_;

  if(sound_class_ == SOUND_CLASS_VEHICLE)
    {
      if(variant_ == SOUND_VARIANT_PRIMARY)
        return SOUND_INSTRUMENT_PRIMARY;
      if(variant_ == SOUND_VARIANT_SECONDARY)
        return SOUND_INSTRUMENT_SECONDARY;

      return SOUND_INSTRUMENT_PRIMARY;
    }

  if(sound_class_ == SOUND_CLASS_EFFECT)
    return SOUND_INSTRUMENT_EFFECT;

  /* Retail hard-stops on unsupported sound classes. */
  for(;;)
    {
    }
}

int32
calculate_sound_pan(int32 angle_)
{
  int32 angle;

  angle = (angle_ >> SOUND_ANGLE_FIXED_SHIFT);
  if((angle >= 0) && (angle <= SOUND_ANGLE_QUARTER_TURN))
    return SOUND_ANGLE_QUARTER_TURN - angle;
  if((angle >= SOUND_ANGLE_REAR_START) && (angle < SOUND_ANGLE_FULL_TURN))
    {
      return SOUND_ANGLE_FULL_TURN + SOUND_ANGLE_QUARTER_TURN - 1 - angle;
    }

  return angle - SOUND_ANGLE_QUARTER_TURN;
}

int32
calculate_sound_distance_gain(int32 distance_)
{
  int32 scaled_gain;

  if(distance_ >= SOUND_DISTANCE_LIMIT)
    return 0;

  scaled_gain = ((SOUND_DISTANCE_LIMIT - distance_) * SOUND_GAIN_MAX);
  return divide_signed_integer(scaled_gain, SOUND_DISTANCE_LIMIT);
}

void
calculate_spatial_sound_parameters(const Vector3 *position_, int32 *pan_, int32 *distance_)
{
  int32 horizontal_distance;
  int32 minor_distance;
  int32 position_x;
  int32 position_y;
  int32 position_z;

  position_x =
    (position_->x - g_SIMULATION_RENDER_STATE.current_position.x) >> SOUND_POSITION_FIXED_SHIFT;
  position_y = position_->y - g_SIMULATION_RENDER_STATE.current_position.y;
  if(position_y <= 0)
    position_y = -position_y;
  position_y >>= SOUND_POSITION_VERTICAL_DISTANCE_SHIFT;
  position_z =
    (position_->z - g_SIMULATION_RENDER_STATE.current_position.z) >> SOUND_POSITION_FIXED_SHIFT;

  if(g_SIMULATION_RENDER_STATE.current_vector.y < 0)
    {
      g_SIMULATION_RENDER_STATE.current_vector.y += SOUND_POSITION_ANGLE_FULL_TURN;
    }
  *pan_ = g_SIMULATION_RENDER_STATE.current_vector.y - Atan2F16(position_z, position_x);
  if(*pan_ < 0)
    *pan_ += SOUND_POSITION_ANGLE_FULL_TURN;
  if(*pan_ > SOUND_POSITION_ANGLE_FULL_TURN)
    *pan_ -= SOUND_POSITION_ANGLE_FULL_TURN;

  if(position_x <= 0)
    position_x = -position_x;
  if(position_z <= 0)
    position_z = -position_z;
  horizontal_distance = position_z;
  minor_distance      = position_x;
  if(position_z < position_x)
    {
      horizontal_distance = position_x;
      minor_distance      = position_z;
    }
  horizontal_distance += (minor_distance >> SOUND_POSITION_MINOR_DISTANCE_SHIFT);
  if(horizontal_distance < position_y)
    {
      horizontal_distance = position_y + (horizontal_distance >> SOUND_POSITION_FAR_VERTICAL_SHIFT);
    }
  else
    {
      horizontal_distance += (position_y >> SOUND_POSITION_NEAR_VERTICAL_SHIFT);
    }

  if(horizontal_distance < SOUND_POSITION_SQUARE_LIMIT)
    {
      *distance_ = (horizontal_distance * horizontal_distance) >> SOUND_POSITION_SQUARE_SHIFT;
    }
  else
    {
      *distance_ = SOUND_POSITION_FAR_DISTANCE;
    }
}

int32
select_spatial_sound_sample(int32 gain_, int32 sound_class_, int32 sound_variant_)
{
  int32 gain;
  int32 sample;
  int32 low_sample;
  int32 high_sample;
  int32 base_sample;

  gain   = (gain_ >> SOUND_GAIN_FIXED_SHIFT);
  sample = 0;
  if(sound_class_ == SOUND_CLASS_FIXED_FOUR)
    {
      sample = g_VEHICLE_SOUND_SAMPLES[SOUND_SAMPLE_FIXED_FOUR];
      goto selected;
    }
  if(sound_class_ == SOUND_CLASS_FIXED_ONE)
    {
      /* Retail hard-stops on this otherwise unsupported class. */
      for(;;)
        {
        }
    }
  if(sound_class_ == SOUND_CLASS_FIXED_TWO)
    {
      sample = g_VEHICLE_SOUND_SAMPLES[SOUND_SAMPLE_FIXED_TWO];
      goto selected;
    }
  if(sound_class_ == SOUND_CLASS_FIXED_THREE)
    {
      sample = g_VEHICLE_SOUND_SAMPLES[SOUND_SAMPLE_FIXED_THREE];
      goto selected;
    }

  high_sample = g_VEHICLE_SOUND_SAMPLES[SOUND_SAMPLE_HIGH];
  base_sample = g_VEHICLE_SOUND_SAMPLES[SOUND_SAMPLE_BASE];
  low_sample  = g_VEHICLE_SOUND_SAMPLES[SOUND_SAMPLE_LOW];

  if(sound_class_ == SOUND_CLASS_ENGINE_SECONDARY)
    {
      if(sound_variant_ == SOUND_VARIANT_LOW)
        {
          sample = g_VEHICLE_SOUND_SAMPLES[SOUND_SAMPLE_SECONDARY];
          goto selected;
        }
    }
  else if(sound_class_ == SOUND_CLASS_ENGINE_PRIMARY)
    {
      if(sound_variant_ == SOUND_VARIANT_FIXED_ONE)
        {
          sample = g_VEHICLE_SOUND_SAMPLES[SOUND_SAMPLE_FIXED_ONE];
          goto selected;
        }
      if(sound_variant_ == SOUND_VARIANT_FIXED_TWO)
        {
          sample = base_sample;
          goto selected;
        }
      if(sound_variant_ == SOUND_VARIANT_LOW)
        {
          if(gain > SOUND_GAIN_HIGH_LIMIT)
            sample = high_sample;
          else if(gain > SOUND_GAIN_MID_LIMIT)
            sample = low_sample;
          else if(gain > SOUND_GAIN_LOW_LIMIT)
            sample = g_VEHICLE_SOUND_SAMPLES[SOUND_SAMPLE_FIXED_ONE];
          else
            sample = base_sample;
          goto selected;
        }
    }

  if(gain > SOUND_GAIN_DEFAULT_HIGH)
    sample = high_sample;
  else if(gain > SOUND_GAIN_DEFAULT_MID)
    sample = low_sample;
  else
    sample = base_sample;

selected:
  return sample;
}

int32
calculate_spatial_sound_frequency(int32 sound_id_, int32 instrument_index_, int32 gain_)
{
  int32 fixed_gain;
  int32 fine_scale;
  int32 coarse_scale;
  int32 cache_index;
  int32 frequency;

  fixed_gain   = (gain_ >> SPATIAL_SOUND_GAIN_SHIFT);
  fine_scale   = (fixed_gain << SPATIAL_SOUND_FREQUENCY_FINE_SHIFT);
  coarse_scale = (fixed_gain << SPATIAL_SOUND_FREQUENCY_COARSE_SHIFT);
  if(sound_id_ == SPATIAL_SOUND_ENGINE_ID)
    {
      if(g_SECONDARY_SOUND_EFFECT_SLOTS[SPATIAL_SOUND_ENGINE_SLOT_INDEX].active_voice == 0)
        {
          if(instrument_index_ == SPATIAL_SOUND_INSTRUMENT_FINE)
            {
              frequency = (g_SPATIAL_SOUND_TUNING.fine_frequency +
                           (fine_scale >> SPATIAL_SOUND_FINE_SCALE_SHIFT));
            }
          else if(instrument_index_ == SPATIAL_SOUND_INSTRUMENT_COARSE)
            {
              frequency = (g_SPATIAL_SOUND_TUNING.coarse_frequency +
                           (coarse_scale >> g_SPATIAL_SOUND_TUNING.coarse_shift));
            }
          else
            {
              frequency = SPATIAL_SOUND_DEFAULT_FREQUENCY;
            }
          g_SPATIAL_SOUND_FREQUENCY_CACHE[0] = frequency;
        }
      else if(g_SPATIAL_SOUND_SOURCE->traffic_brake_indicator <= SPATIAL_SOUND_LOW_SPEED_LIMIT)
        {
          frequency = ((g_SPATIAL_SOUND_TUNING.active_frequency_scale *
                        (gain_ >> SPATIAL_SOUND_ACTIVE_GAIN_SHIFT)) >>
                       g_SPATIAL_SOUND_TUNING.active_frequency_shift);
        }
      else
        {
          frequency = g_SPATIAL_SOUND_FREQUENCY_CACHE[0];
        }
    }
  else
    {
      cache_index = (sound_id_ - SPATIAL_SOUND_CACHE_ID_OFFSET);
      if(g_TERTIARY_SOUND_EFFECT_SLOTS[sound_id_ - SPATIAL_SOUND_FIRST_TERTIARY_ID].active_voice ==
         0)
        {
          if(instrument_index_ == SPATIAL_SOUND_INSTRUMENT_COARSE)
            {
              frequency = (g_SPATIAL_SOUND_TUNING.alternate_coarse_frequency +
                           (coarse_scale >> SPATIAL_SOUND_COARSE_SCALE_SHIFT));
            }
          else
            {
              frequency = (g_SPATIAL_SOUND_TUNING.alternate_fine_frequency +
                           (fine_scale >> SPATIAL_SOUND_FINE_SCALE_SHIFT));
            }
          g_SPATIAL_SOUND_FREQUENCY_CACHE[cache_index] = frequency;
        }
      else
        {
          frequency = g_SPATIAL_SOUND_FREQUENCY_CACHE[cache_index];
        }
    }

  return frequency;
}

void
stop_spatial_sound_effect(int32 sound_id_)
{
  int32 slot_index;

  if(sound_id_ < SPATIAL_SOUND_FIRST_TERTIARY_ID)
    {
      stop_audio_voice(sound_id_);
      g_SECONDARY_SOUND_EFFECT_SLOTS[sound_id_].active_voice = 0;
      return;
    }

  slot_index = (sound_id_ - SPATIAL_SOUND_FIRST_TERTIARY_ID);
  stop_dolby_sound_channel(slot_index);
  g_TERTIARY_SOUND_EFFECT_SLOTS[slot_index].sample_index = -1;
  g_TERTIARY_SOUND_EFFECT_SLOTS[slot_index].active_voice = 0;
}

int32
update_dolby_spatial_sound(int32 sound_id_,
                           int32 instrument_index_,
                           int32 sample_priority_,
                           int32 amplitude_,
                           int32 pan_)
{
  SoundEffectSlot *slot;
  int32            slot_index;
  int32            sample;
  int32            result;

  sample = g_VEHICLE_SOUND_SAMPLES[instrument_index_];
  amplitude_ >>= 1;
  if(sound_id_ == SPATIAL_SOUND_CHANNEL_100)
    {
      amplitude_ = (g_SPATIAL_SOUND_CHANNEL_100_AMPLITUDE +
                    ((amplitude_ - g_SPATIAL_SOUND_CHANNEL_100_AMPLITUDE) /
                     SPATIAL_SOUND_AMPLITUDE_SMOOTHING));
      g_SPATIAL_SOUND_CHANNEL_100_AMPLITUDE = amplitude_;
    }
  else if(sound_id_ == SPATIAL_SOUND_CHANNEL_101)
    {
      amplitude_ = (g_SPATIAL_SOUND_CHANNEL_101_AMPLITUDE +
                    ((amplitude_ - g_SPATIAL_SOUND_CHANNEL_101_AMPLITUDE) /
                     SPATIAL_SOUND_AMPLITUDE_SMOOTHING));
      g_SPATIAL_SOUND_CHANNEL_101_AMPLITUDE = amplitude_;
    }

  if(g_STARTUP_CONFIGURATION.stereo_enabled == 0)
    pan_ = 0;

  slot_index = (sound_id_ - SPATIAL_SOUND_FIRST_TERTIARY_ID);
  slot       = &g_TERTIARY_SOUND_EFFECT_SLOTS[slot_index];
  if((slot->sample_index != sample) && (slot->active_voice == SPATIAL_SOUND_ACTIVE))
    {
      stop_dolby_sound_channel(slot_index);
      slot->active_voice = 0;
      if(slot_index > SPATIAL_SOUND_LAST_DOLBY_SLOT)
        Debug();
      return 0;
    }

  if(slot->active_voice == 0)
    {
      slot->sample_index = sample;
      slot->active_voice = SPATIAL_SOUND_ACTIVE;
      result = play_dolby_sound_channel(slot_index, sample, sample_priority_, amplitude_, pan_);
      if(result != 0)
        return fflush(stdout);
      return 0;
    }

  return set_dolby_sound_channel_parameters(slot_index, sample_priority_, amplitude_, pan_);
}

void
update_audio_voice_spatial_sound(int32 sound_id_,
                                 int32 instrument_index_,
                                 int32 frequency_,
                                 int32 amplitude_,
                                 int32 pan_)
{
  SoundEffectSlot *slot;
  int32            sample;

  sample = g_VEHICLE_SOUND_SAMPLES[instrument_index_];
  slot   = &g_SECONDARY_SOUND_EFFECT_SLOTS[sound_id_];
  if((slot->sample_index != sample) && (slot->active_voice == SPATIAL_SOUND_ACTIVE))
    {
      ramp_down_audio_voice_amplitude(sound_id_);
      stop_audio_voice(sound_id_);
      slot->active_voice = 0;
      return;
    }

  if(slot->active_voice == 0)
    {
      slot->sample_index = sample;
      slot->active_voice = SPATIAL_SOUND_ACTIVE;
      initialize_audio_voice(sound_id_, sample, frequency_, amplitude_, pan_);
      return;
    }

  set_audio_voice_amplitude(sound_id_, amplitude_);
  set_audio_voice_frequency(sound_id_, frequency_);
  set_audio_voice_pan(sound_id_, pan_);
}

void
play_anonymous_spatial_sound(int32 sample_, int32 frequency_, int32 amplitude_, int32 pan_)
{
  int32 voice_index;

  if(G_ACTIVE_SIMULATION_SOURCE->simulation_source.update_callback == update_player_vehicle_state)
    {
      voice_index = g_SPATIAL_SOUND_ALTERNATING_VOICE;
      initialize_audio_voice(voice_index, sample_, frequency_, amplitude_, pan_);
      if(voice_index == SPATIAL_SOUND_ALTERNATING_VOICE_SECOND)
        {
          g_SPATIAL_SOUND_ALTERNATING_VOICE = SPATIAL_SOUND_ALTERNATING_VOICE_FIRST;
        }
      else
        {
          g_SPATIAL_SOUND_ALTERNATING_VOICE = SPATIAL_SOUND_ALTERNATING_VOICE_SECOND;
        }
      return;
    }

  initialize_audio_voice(SPATIAL_SOUND_ALTERNATING_VOICE_SECOND,
                         sample_,
                         frequency_,
                         amplitude_,
                         pan_);
}

void
update_vehicle_proximity_alert(void)
{
  const int32 ALERT_DELAYS[VEHICLE_PROXIMITY_ALERT_RATE_COUNT] = {
    VEHICLE_PROXIMITY_ALERT_DELAY_0, VEHICLE_PROXIMITY_ALERT_DELAY_1,
    VEHICLE_PROXIMITY_ALERT_DELAY_2, VEHICLE_PROXIMITY_ALERT_DELAY_3,
    VEHICLE_PROXIMITY_ALERT_DELAY_4, VEHICLE_PROXIMITY_ALERT_DELAY_5,
    VEHICLE_PROXIMITY_ALERT_DELAY_6
  };
  int32 alert_delay;

  alert_delay = ALERT_DELAYS[g_VEHICLE_PROXIMITY_ALERT_RATE_INDEX];
  if(g_VEHICLE_PROXIMITY_ALERT_ACTIVE == 0)
    {
      g_VEHICLE_PROXIMITY_ALERT_COUNTDOWN = alert_delay;
      g_VEHICLE_PROXIMITY_ALERT_ACTIVE    = 1;
    }
  else if(alert_delay < g_VEHICLE_PROXIMITY_ALERT_COUNTDOWN)
    {
      g_VEHICLE_PROXIMITY_ALERT_COUNTDOWN = alert_delay;
    }

  if(g_VEHICLE_PROXIMITY_ALERT_COUNTDOWN == 0)
    {
      initialize_audio_voice(VEHICLE_PROXIMITY_ALERT_VOICE,
                             g_VEHICLE_SOUND_SAMPLES[VEHICLE_PROXIMITY_ALERT_SAMPLE],
                             VEHICLE_PROXIMITY_ALERT_FREQUENCY,
                             VEHICLE_PROXIMITY_ALERT_AMPLITUDE,
                             VEHICLE_PROXIMITY_ALERT_PAN);
      g_VEHICLE_PROXIMITY_ALERT_ACTIVE = 0;
    }
  else
    {
      g_VEHICLE_PROXIMITY_ALERT_COUNTDOWN--;
    }
}

void
update_player_vehicle_runtime_audio(SimulationSource *source_, int32 pan_, int32 distance_)
{
  const VehicleParameterState *vehicle_state;
  const int32 *frequency_entry;
  int32        speed;
  int32        surface_frequency;
  int32        surface_gain;
  int32        frequency;
  int32        pan_difference;
  int32        left_gain;
  int32        right_gain;
  int32        distance_gain;

  g_SPATIAL_SOUND_SOURCE = source_;
  vehicle_state          = (const VehicleParameterState *) source_;
  if(g_STARTUP_CONFIGURATION.sound_effects_enabled == 0)
    return;
  if(g_VEHICLE_PROXIMITY_ALERT_RATE_INDEX != 0)
    update_vehicle_proximity_alert();

  speed        = source_->speed_magnitude >> VEHICLE_AUDIO_SPEED_FIXED_SHIFT;
  surface_gain = speed << VEHICLE_AUDIO_SURFACE_FREQUENCY_SHIFT;
  if(surface_gain < 0)
    surface_gain = 0;

  if(g_VEHICLE_ALTERNATE_SURFACE_BANK_ACTIVE == 0)
    {
      if(g_VEHICLE_SPECIAL_SURFACE_AUDIO_ACTIVE == 0)
        {
          if(g_VEHICLE_SURFACE_INDEX == 0)
            {
              g_VEHICLE_SURFACE_AUDIO_FREQUENCY = VEHICLE_AUDIO_DEFAULT_FREQUENCY;
            }
          else
            {
              surface_gain *= VEHICLE_AUDIO_SPECIAL_GAIN_SCALE;
              if(surface_gain >= VEHICLE_AUDIO_GAIN_LIMIT)
                surface_gain = VEHICLE_AUDIO_GAIN_MAXIMUM;
              g_VEHICLE_SURFACE_AUDIO_FREQUENCY = VEHICLE_AUDIO_LOW_FREQUENCY;
            }
        }
      else
        {
          surface_frequency = speed * VEHICLE_AUDIO_OSCILLATION_SCALE;
          if(g_VEHICLE_SURFACE_AUDIO_FREQUENCY_INCREASING == 0)
            surface_frequency = -surface_frequency;
          g_VEHICLE_SURFACE_AUDIO_FREQUENCY += surface_frequency;
          if(g_VEHICLE_SURFACE_AUDIO_FREQUENCY < VEHICLE_AUDIO_OSCILLATION_LOW)
            {
              g_VEHICLE_SURFACE_AUDIO_FREQUENCY_INCREASING = 1;
            }
          else if(g_VEHICLE_SURFACE_AUDIO_FREQUENCY > VEHICLE_AUDIO_HIGH_FREQUENCY)
            {
              g_VEHICLE_SURFACE_AUDIO_FREQUENCY_INCREASING = 0;
            }
        }
    }
  else
    {
      surface_gain *= VEHICLE_AUDIO_ALTERNATE_GAIN_SCALE;
      if(surface_gain >= VEHICLE_AUDIO_GAIN_LIMIT)
        surface_gain = VEHICLE_AUDIO_GAIN_MAXIMUM;
      g_VEHICLE_SURFACE_AUDIO_FREQUENCY = VEHICLE_AUDIO_HIGH_FREQUENCY;
    }

  set_audio_voice_amplitude(VEHICLE_AUDIO_SURFACE_VOICE_LEFT, surface_gain);
  set_audio_voice_frequency(VEHICLE_AUDIO_SURFACE_VOICE_LEFT,
                            g_VEHICLE_SURFACE_AUDIO_FREQUENCY);
  set_audio_voice_amplitude(VEHICLE_AUDIO_SURFACE_VOICE_RIGHT, surface_gain);
  set_audio_voice_frequency(VEHICLE_AUDIO_SURFACE_VOICE_RIGHT,
                            g_VEHICLE_SURFACE_AUDIO_FREQUENCY +
                              VEHICLE_AUDIO_SURFACE_FREQUENCY_OFFSET);

  frequency_entry = &g_SPATIAL_SOUND_FREQUENCY_CACHE[VEHICLE_AUDIO_FREQUENCY_TABLE_BASE +
                                                     (g_PLAYER_VEHICLE_INDEX *
                                                      VEHICLE_AUDIO_FREQUENCY_TABLE_STRIDE)];
  frequency       = ((vehicle_state->engine_speed * frequency_entry[0]) /
                     vehicle_state->parameters->maximum_engine_speed);
  frequency += frequency_entry[1];

  pan_difference = vehicle_state->primary_control_value - g_VEHICLE_AUDIO_PAN_INDEX;
  if(vehicle_state->primary_control_value < g_VEHICLE_AUDIO_PAN_INDEX)
    {
      if(pan_difference < 0)
        pan_difference += (1 << VEHICLE_AUDIO_PAN_DECREASE_SHIFT) - 1;
      g_VEHICLE_AUDIO_PAN_INDEX += pan_difference >> VEHICLE_AUDIO_PAN_DECREASE_SHIFT;
    }
  else
    {
      g_VEHICLE_AUDIO_PAN_INDEX += pan_difference >> VEHICLE_AUDIO_PAN_INCREASE_SHIFT;
    }

  if((vehicle_state->engine_speed < vehicle_state->target_engine_speed) &&
     (vehicle_state->gear_control_index >= 0))
    {
      g_VEHICLE_AUDIO_PAN_INDEX +=
        (vehicle_state->target_engine_speed - vehicle_state->engine_speed) >>
        VEHICLE_AUDIO_PAN_ACCELERATION_SHIFT;
      if(g_VEHICLE_AUDIO_PAN_INDEX > VEHICLE_AUDIO_PAN_MAXIMUM)
        g_VEHICLE_AUDIO_PAN_INDEX = VEHICLE_AUDIO_PAN_MAXIMUM;
    }

  right_gain    = g_VEHICLE_AUDIO_GAIN_TABLE[g_VEHICLE_AUDIO_PAN_INDEX];
  left_gain     = g_VEHICLE_AUDIO_GAIN_TABLE[VEHICLE_AUDIO_PAN_MAXIMUM - g_VEHICLE_AUDIO_PAN_INDEX];
  distance_gain = calculate_sound_distance_gain(distance_);
  left_gain     = ((distance_gain >> VEHICLE_AUDIO_DISTANCE_GAIN_SHIFT) * left_gain) >>
                  VEHICLE_AUDIO_GAIN_PRODUCT_SHIFT;
  right_gain    = ((distance_gain >> VEHICLE_AUDIO_DISTANCE_GAIN_SHIFT) * right_gain) >>
                  VEHICLE_AUDIO_GAIN_PRODUCT_SHIFT;

  set_audio_voice_amplitude(VEHICLE_AUDIO_ENGINE_VOICE_FIRST, left_gain);
  set_audio_voice_frequency(VEHICLE_AUDIO_ENGINE_VOICE_FIRST, frequency);
  set_audio_voice_amplitude(VEHICLE_AUDIO_ENGINE_VOICE_SECOND, left_gain);
  set_audio_voice_frequency(VEHICLE_AUDIO_ENGINE_VOICE_SECOND, frequency);
  set_audio_voice_amplitude(VEHICLE_AUDIO_ENGINE_VOICE_THIRD, right_gain);
  set_audio_voice_frequency(VEHICLE_AUDIO_ENGINE_VOICE_THIRD, frequency);
  set_audio_voice_amplitude(VEHICLE_AUDIO_ENGINE_VOICE_FOURTH, right_gain);
  set_audio_voice_frequency(VEHICLE_AUDIO_ENGINE_VOICE_FOURTH, frequency);

  (void) pan_;
}

void
update_active_vehicle_spatial_audio_channels(void)
{
  SimulationSource *source;
  int32             pan;
  int32             secondary_pan;
  int32             distance;
  int32             sound_class;
  int32             sound_variant;

  source = (SimulationSource *) G_ACTIVE_SIMULATION_SOURCE;
  if(*(int32 *) g_FRONTEND_RENDER_STATE.projection.source == 0)
    {
      pan = source->spatial_sound_heading * VEHICLE_AUDIO_HEADING_SCALE;
      if(pan < 0)
        pan += VEHICLE_AUDIO_ANGLE_FULL_TURN;
      distance      = 0;
      secondary_pan = VEHICLE_AUDIO_ANGLE_HALF_TURN;
    }
  else
    {
      calculate_spatial_sound_parameters(&source->position, &pan, &distance);
      secondary_pan = pan;
    }

  sound_class = g_ACTIVE_SIMULATION_SOUND_INDEX;
  if(sound_class < 0)
    {
      sound_class = VEHICLE_AUDIO_DEFAULT_SOUND_CLASS;
      if(g_VEHICLE_SURFACE_INDEX < 1)
        sound_variant = VEHICLE_AUDIO_PRIMARY_VARIANT;
      else
        sound_variant = VEHICLE_AUDIO_SECONDARY_VARIANT;
    }
  else
    {
      sound_variant = g_ACTIVE_SIMULATION_SOUND_VARIANT;
    }

  if(source->update_callback != update_player_vehicle_state)
    {
      play_spatial_sound_effect_attenuated(VEHICLE_AUDIO_PRIMARY_SOUND_ID,
                                           sound_class,
                                           sound_variant,
                                           source->primary_spatial_sound_gain,
                                           distance,
                                           pan);
    }
  if(g_SECONDARY_VEHICLE_SOUND_SUPPRESSED == 0)
    {
      play_spatial_sound_effect_attenuated(VEHICLE_AUDIO_SECONDARY_SOUND_ID,
                                           sound_class,
                                           sound_variant,
                                           source->secondary_spatial_sound_gain,
                                           distance,
                                           secondary_pan);
    }
}

void
request_traffic_vehicle_spatial_sound(int32 source_index_, int32 variant_)
{
  SimulationSource *target;

  target =
    (SimulationSource *) g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_SIX_TARGET];
  if((target->status_flags & TRAFFIC_VEHICLE_SOUND_BLOCKED_FLAGS) != 0)
    {
      g_TRAFFIC_VEHICLE_SOUND_COUNTDOWN = 0;
      return;
    }
  if(g_TRAFFIC_VEHICLE_SOUND_COUNTDOWN != 0)
    return;

  g_TRAFFIC_VEHICLE_SOUND_SOURCE_INDEX = source_index_;
  g_TRAFFIC_VEHICLE_SOUND_VARIANT      = variant_;
  g_TRAFFIC_VEHICLE_SOUND_COUNTDOWN =
    scale_by_random_fraction(g_TRAFFIC_VEHICLE_SOUND_RANDOM_FRAMES) +
    g_TRAFFIC_VEHICLE_SOUND_MINIMUM_FRAMES;
}

void
refresh_vehicle_runtime_audio(void)
{
  VehicleSimulationState *record;
  int32                   pan;
  int32                   distance;
  int32                   motion_scale;
  int32                   instrument;
  int32                   amplitude;
  int32                   distance_gain;

  if(g_TRAFFIC_VEHICLE_SOUND_COUNTDOWN != 0)
    {
      record = g_VEHICLE_SIMULATION_STATES[g_TRAFFIC_VEHICLE_SOUND_SOURCE_INDEX];
      calculate_spatial_sound_parameters(&record->simulation_source.position, &pan, &distance);
      motion_scale =
        calculate_vehicle_audio_motion_scale((struct VehicleRuntimeFrameRecord *) record);
      g_TRAFFIC_VEHICLE_SOUND_COUNTDOWN--;
      if(g_TRAFFIC_VEHICLE_SOUND_COUNTDOWN == 0)
        return;

      if(motion_scale != 0)
        {
          instrument = select_traffic_vehicle_sound_instrument();
          amplitude  = select_traffic_vehicle_sound_amplitude() * motion_scale;
          if(amplitude < 0)
            amplitude += (1 << TRAFFIC_VEHICLE_SOUND_GAIN_SHIFT) - 1;
          amplitude >>= TRAFFIC_VEHICLE_SOUND_GAIN_SHIFT;
          distance_gain = calculate_sound_distance_gain(distance);
          update_dolby_spatial_sound(TRAFFIC_VEHICLE_SOUND_CHANNEL,
                                     instrument,
                                     amplitude,
                                     distance_gain,
                                     pan >> TRAFFIC_VEHICLE_SOUND_PAN_SHIFT);
          g_VEHICLE_COLLISION_SOUND_PENDING = 1;
        }
      return;
    }

  if((g_VEHICLE_COLLISION_SOUND_ACTIVE == 0) && (g_VEHICLE_COLLISION_SOUND_PENDING != 0))
    {
      update_dolby_spatial_sound(TRAFFIC_VEHICLE_SOUND_CHANNEL,
                                 TRAFFIC_VEHICLE_SOUND_STOP_INSTRUMENT,
                                 0,
                                 0,
                                 0);
      g_VEHICLE_COLLISION_SOUND_PENDING = 0;
    }
}

int32
select_traffic_vehicle_sound_instrument(void)
{
  return G_TRAFFIC_VEHICLE_SOUND_INSTRUMENTS[g_TRAFFIC_VEHICLE_SOUND_SOURCE_INDEX];
}

int32
select_traffic_vehicle_sound_amplitude(void)
{
  return G_TRAFFIC_VEHICLE_SOUND_AMPLITUDES[g_TRAFFIC_VEHICLE_SOUND_VARIANT];
}

void
play_spatial_sound_effect(int32 sound_id_,
                          int32 sound_class_,
                          int32 sound_variant_,
                          int32 gain_,
                          int32 distance_,
                          int32 pan_)
{
  int32 instrument_index;
  int32 sample;
  int32 frequency;
  int32 distance_gain;
  int32 amplitude;
  int32 random_direction;
  int32 random_frequency;

  if(G_FRONTEND_EXIT_REQUESTED == SPATIAL_SOUND_DISABLED_FRONTEND_MODE)
    return;
  if(g_STARTUP_CONFIGURATION.sound_effects_enabled == 0)
    return;
  if(g_STARTUP_CONFIGURATION.stereo_enabled == 0)
    pan_ = 0;

  pan_          = calculate_sound_pan(pan_);
  distance_gain = calculate_sound_distance_gain(distance_);
  if(sound_id_ < 0)
    {
      sample = select_spatial_sound_sample(gain_, sound_class_, sound_variant_);
      if(sample == 0)
        return;

      random_direction = scale_by_random_fraction(SPATIAL_SOUND_RANDOM_DIRECTION_COUNT);
      random_frequency = scale_by_random_fraction(g_SPATIAL_SOUND_TUNING.base_frequency);
      if(random_direction != 0)
        random_frequency = -random_frequency;
      random_frequency += SPATIAL_SOUND_RANDOM_FREQUENCY_BASE;
      play_anonymous_spatial_sound(sample, random_frequency, distance_gain, pan_);
      return;
    }

  if(gain_ < 1)
    {
      stop_spatial_sound_effect(sound_id_);
      return;
    }

  instrument_index = select_sound_instrument_index(gain_, sound_class_, sound_variant_);
  calculate_spatial_sound_frequency(sound_id_, instrument_index, gain_);
  amplitude = distance_gain;
  if(instrument_index == SPATIAL_SOUND_INSTRUMENT_COARSE)
    {
      if(gain_ > SPATIAL_SOUND_GAIN_MAXIMUM)
        gain_ = SPATIAL_SOUND_GAIN_MAXIMUM;
      amplitude = (((gain_ >> SPATIAL_SOUND_AMPLITUDE_PRODUCT_SHIFT) *
                    (distance_gain >> SPATIAL_SOUND_AMPLITUDE_PRODUCT_SHIFT)) /
                   SPATIAL_SOUND_AMPLITUDE_DIVISOR);
      amplitude += SPATIAL_SOUND_AMPLITUDE_BIAS;
      if(amplitude > SPATIAL_SOUND_AMPLITUDE_MAXIMUM)
        amplitude = SPATIAL_SOUND_AMPLITUDE_MAXIMUM;
    }

  if(g_VEHICLE_SOUND_SAMPLES[instrument_index] == 0)
    return;
  if(sound_id_ >= SPATIAL_SOUND_FIRST_TERTIARY_ID)
    {
      frequency = g_SPATIAL_SOUND_FREQUENCY_CACHE[sound_id_ - SPATIAL_SOUND_CACHE_ID_OFFSET];
      update_dolby_spatial_sound(sound_id_,
                                 instrument_index,
                                 frequency,
                                 amplitude,
                                 pan_ >> SOUND_ANGLE_FIXED_SHIFT);
      return;
    }

  if(is_sound_effect_playable(sound_id_) == 0)
    return;
  frequency = g_SPATIAL_SOUND_FREQUENCY_CACHE[0];
  update_audio_voice_spatial_sound(sound_id_, instrument_index, frequency, amplitude, pan_);
}

void
play_spatial_sound_effect_unscaled(int32 sound_id_,
                                   int32 sound_class_,
                                   int32 sound_variant_,
                                   int32 gain_,
                                   int32 distance_,
                                   int32 pan_)
{
  play_spatial_sound_effect(sound_id_, sound_class_, sound_variant_, gain_, distance_, pan_);
}

void
play_spatial_sound_effect_attenuated(int32 sound_id_,
                                     int32 sound_class_,
                                     int32 sound_variant_,
                                     int32 gain_,
                                     int32 distance_,
                                     int32 pan_)
{
  play_spatial_sound_effect(sound_id_,
                            sound_class_,
                            sound_variant_,
                            gain_ >> SOUND_ATTENUATED_GAIN_SHIFT,
                            distance_,
                            pan_);
}

void
play_vehicle_spatial_sound_attenuated(int32 sound_id_,
                                      int32 sound_class_,
                                      int32 sound_variant_,
                                      int32 gain_,
                                      int32 distance_,
                                      int32 pan_)
{
  play_spatial_sound_effect(sound_id_,
                            sound_class_,
                            sound_variant_,
                            gain_ >> SOUND_ATTENUATED_GAIN_SHIFT,
                            distance_,
                            pan_);
}

void
play_spatial_sound_effect_scaled(int32 sound_id_,
                                 int32 sound_class_,
                                 int32 sound_variant_,
                                 int32 gain_,
                                 int32 distance_,
                                 int32 pan_)
{
  play_spatial_sound_effect(sound_id_,
                            sound_class_,
                            sound_variant_,
                            gain_ * SOUND_SCALED_GAIN_MULTIPLIER,
                            distance_,
                            pan_);
}
