#pragma once

#include "types.h"

#define DOLBY_GAIN_TABLE_ENTRY_COUNT 512
#define DOLBY_DSP_DIRECTORY_BYTE_COUNT 256

typedef struct DolbySoundChannel
{
  Item  sample;
  Item  attachment;
  Item  sampler_instrument;
  Item  output_instrument;
  Item  left_multiply_instrument;
  Item  right_multiply_instrument;
  Item  amplitude_knob;
  Item  frequency_knob;
  Item  left_gain_knob;
  Item  right_gain_knob;
  int32 previous_location;
  int32 pan_table_index;
} DolbySoundChannel;

typedef struct StereoGain
{
  int32 left;
  int32 right;
} StereoGain;

extern DolbySoundChannel *g_DOLBY_SOUND_CHANNELS;
extern int32              g_DOLBY_SOUND_CHANNEL_COUNT;
extern StereoGain         g_DOLBY_GAIN_TABLE[DOLBY_GAIN_TABLE_ENTRY_COUNT];
extern char               g_DOLBY_DSP_DIRECTORY[DOLBY_DSP_DIRECTORY_BYTE_COUNT];
extern int32              g_SOUND_EFFECT_RESOURCE_STATE;

void
initialize_dolby_gain_table(void);
void
set_dolby_dsp_directory(const char *directory_);
int32
complete_sound_effect_resource_initialization(void);
void
clear_dolby_sound_channel(DolbySoundChannel *channel_);
int32
initialize_dolby_sound_channel(DolbySoundChannel *channel_);
void
shutdown_dolby_sound_channel(DolbySoundChannel *channel_);
int32
initialize_dolby_sound_pool(int32 channel_count_);
int32
stop_dolby_sound_channel(int32 channel_index_);
void
calculate_dolby_channel_gains(StereoGain *gain_, int32 channel_index_, int32 location_);
int32
set_dolby_sound_channel_parameters(int32 channel_index_,
                                   int32 frequency_,
                                   int32 amplitude_,
                                   int32 location_);
int32
play_dolby_sound_channel(int32 channel_index_,
                         Item  sample_,
                         int32 frequency_,
                         int32 amplitude_,
                         int32 location_);
void
initialize_sound_effect_resources(void);
void
shutdown_sound_effect_resources(void);
