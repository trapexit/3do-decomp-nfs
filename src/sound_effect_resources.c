#include "sound_effect_resources.h"

#include "memory.h"
#include "sound_effect_slots.h"
#include "sound_effect_state.h"
#include "string_utilities.h"

#include "audio.h"
#include "debug.h"
#include "filefunctions.h"

#include "stdio.h"

#define DOLBY_SOUND_ALLOCATION_NAME "dolby"
#define DOLBY_LOCATION_MAXIMUM 0xFF
#define DOLBY_LOCATION_PERIOD 0x100
#define DOLBY_PAN_PERIOD 0x200
#define DOLBY_QUADRANT_SIZE 0x40
#define DOLBY_HALF_TURN 0x80
#define DOLBY_THREE_QUARTER_TURN 0xC0
#define SOUND_EFFECT_DIRECTORY_BYTE_COUNT 80
#define INITIALIZED_VEHICLE_SOUND_SAMPLE_COUNT 24
#define SOUND_EFFECT_DSP_SUBDIRECTORY "/DriveData/dsp/"
#define DOLBY_INSTRUMENT_PATH_BYTE_COUNT 256
#define DOLBY_SAMPLER_DSP_NAME "varmono16.dsp"
#define DOLBY_OUTPUT_DSP_NAME "directout.dsp"
#define DOLBY_MULTIPLY_DSP_NAME "multiply.dsp"
#define DOLBY_OUTPUT_PORT_NAME "Output"
#define DOLBY_MULTIPLY_INPUT_NAME "InputB"
#define DOLBY_LEFT_OUTPUT_INPUT_NAME "InputLeft"
#define DOLBY_RIGHT_OUTPUT_INPUT_NAME "InputRight"
#define DOLBY_GAIN_INPUT_NAME "InputA"
#define DOLBY_FREQUENCY_KNOB_NAME "Frequency"
#define DOLBY_AMPLITUDE_KNOB_NAME "Amplitude"
#define DOLBY_CHANNEL_INITIALIZATION_ERROR "Problem in LoadDolbyInstrument - load aborted.\n"
#define DOLBY_DEFAULT_GAIN 0x7FFF
#define DOLBY_DEFAULT_FREQUENCY 0x8000

DolbySoundChannel *g_DOLBY_SOUND_CHANNELS;
int32              g_DOLBY_SOUND_CHANNEL_COUNT;
char               g_DOLBY_DSP_DIRECTORY[DOLBY_DSP_DIRECTORY_BYTE_COUNT];
int32              g_SOUND_EFFECT_RESOURCE_STATE = 1;

int32
complete_sound_effect_resource_initialization(void)
{
  return 1;
}

void
set_dolby_dsp_directory(const char *directory_)
{
  copy_string(g_DOLBY_DSP_DIRECTORY, directory_);
  initialize_dolby_gain_table();
}

void
initialize_sound_effect_resources(void)
{
  char  original_directory[SOUND_EFFECT_DIRECTORY_BYTE_COUNT];
  char  dsp_directory[SOUND_EFFECT_DIRECTORY_BYTE_COUNT];
  int32 sample_index;

  g_SOUND_EFFECT_RESOURCE_STATE = 1;
  for(sample_index = 0; sample_index < INITIALIZED_VEHICLE_SOUND_SAMPLE_COUNT; sample_index++)
    g_VEHICLE_SOUND_SAMPLES[sample_index] = 0;

  initialize_primary_sound_effect_slots();
  initialize_secondary_sound_effect_slots();
  initialize_tertiary_sound_effect_slots();
  GetDirectory(original_directory, SOUND_EFFECT_DIRECTORY_BYTE_COUNT);
  GetDirectory(dsp_directory, SOUND_EFFECT_DIRECTORY_BYTE_COUNT);
  append_string(dsp_directory, SOUND_EFFECT_DSP_SUBDIRECTORY);
  set_dolby_dsp_directory(dsp_directory);
  complete_sound_effect_resource_initialization();
  ChangeDirectory(original_directory);
}

int32
initialize_dolby_sound_channel(DolbySoundChannel *channel_)
{
  char  instrument_path[DOLBY_INSTRUMENT_PATH_BYTE_COUNT];
  int32 path_index;
  int32 result;

  result = -1;
  for(path_index = 0; path_index < DOLBY_INSTRUMENT_PATH_BYTE_COUNT; path_index++)
    instrument_path[path_index] = '\0';

  copy_string(instrument_path, g_DOLBY_DSP_DIRECTORY);
  append_string(instrument_path, DOLBY_SAMPLER_DSP_NAME);
  channel_->sampler_instrument = LoadInstrument(instrument_path, 0, 0);
  if(channel_->sampler_instrument < 0)
    goto fail;

  copy_string(instrument_path, g_DOLBY_DSP_DIRECTORY);
  append_string(instrument_path, DOLBY_OUTPUT_DSP_NAME);
  channel_->output_instrument = LoadInstrument(instrument_path, 0, 0);
  if(channel_->output_instrument < 0)
    goto fail;

  copy_string(instrument_path, g_DOLBY_DSP_DIRECTORY);
  append_string(instrument_path, DOLBY_MULTIPLY_DSP_NAME);
  channel_->left_multiply_instrument = LoadInstrument(instrument_path, 0, 0);
  if(channel_->left_multiply_instrument < 0)
    goto fail;

  copy_string(instrument_path, g_DOLBY_DSP_DIRECTORY);
  append_string(instrument_path, DOLBY_MULTIPLY_DSP_NAME);
  channel_->right_multiply_instrument = LoadInstrument(instrument_path, 0, 0);
  if(channel_->right_multiply_instrument < 0)
    goto fail;

  result = ConnectInstruments(channel_->sampler_instrument,
                              DOLBY_OUTPUT_PORT_NAME,
                              channel_->left_multiply_instrument,
                              DOLBY_MULTIPLY_INPUT_NAME);
  if(result < 0)
    goto fail;
  result = ConnectInstruments(channel_->sampler_instrument,
                              DOLBY_OUTPUT_PORT_NAME,
                              channel_->right_multiply_instrument,
                              DOLBY_MULTIPLY_INPUT_NAME);
  if(result < 0)
    goto fail;
  result = ConnectInstruments(channel_->left_multiply_instrument,
                              DOLBY_OUTPUT_PORT_NAME,
                              channel_->output_instrument,
                              DOLBY_LEFT_OUTPUT_INPUT_NAME);
  if(result < 0)
    goto fail;
  result = ConnectInstruments(channel_->right_multiply_instrument,
                              DOLBY_OUTPUT_PORT_NAME,
                              channel_->output_instrument,
                              DOLBY_RIGHT_OUTPUT_INPUT_NAME);
  if(result < 0)
    goto fail;

  channel_->left_gain_knob = GrabKnob(channel_->left_multiply_instrument, DOLBY_GAIN_INPUT_NAME);
  if(channel_->left_gain_knob < 0)
    {
      result = channel_->left_gain_knob;
      goto fail;
    }
  channel_->right_gain_knob = GrabKnob(channel_->right_multiply_instrument, DOLBY_GAIN_INPUT_NAME);
  if(channel_->right_gain_knob < 0)
    {
      result = channel_->right_gain_knob;
      goto fail;
    }
  channel_->frequency_knob = GrabKnob(channel_->sampler_instrument, DOLBY_FREQUENCY_KNOB_NAME);
  if(channel_->frequency_knob < 0)
    {
      result = channel_->frequency_knob;
      goto fail;
    }
  channel_->amplitude_knob = GrabKnob(channel_->sampler_instrument, DOLBY_AMPLITUDE_KNOB_NAME);
  if(channel_->amplitude_knob < 0)
    {
      result = channel_->amplitude_knob;
      goto fail;
    }

  result = TweakKnob(channel_->left_gain_knob, DOLBY_DEFAULT_GAIN);
  if(result < 0)
    goto fail;
  result = TweakKnob(channel_->right_gain_knob, DOLBY_DEFAULT_GAIN);
  if(result < 0)
    goto fail;
  result = TweakKnob(channel_->frequency_knob, DOLBY_DEFAULT_FREQUENCY);
  if(result < 0)
    goto fail;
  result = TweakKnob(channel_->amplitude_knob, DOLBY_DEFAULT_GAIN);
  if(result < 0)
    goto fail;

  StartInstrument(channel_->left_multiply_instrument, NULL);
  StartInstrument(channel_->right_multiply_instrument, NULL);
  StartInstrument(channel_->output_instrument, NULL);
  return 0;

fail:
  kprintf(DOLBY_CHANNEL_INITIALIZATION_ERROR);
  fflush(stdout);
  shutdown_dolby_sound_channel(channel_);
  if(result == 0)
    return -1;
  return result;
}

void
clear_dolby_sound_channel(DolbySoundChannel *channel_)
{
  channel_->sample                    = 0;
  channel_->attachment                = 0;
  channel_->sampler_instrument        = 0;
  channel_->output_instrument         = 0;
  channel_->left_multiply_instrument  = 0;
  channel_->right_multiply_instrument = 0;
  channel_->amplitude_knob            = 0;
  channel_->frequency_knob            = 0;
  channel_->left_gain_knob            = 0;
  channel_->right_gain_knob           = 0;
  channel_->previous_location         = 0;
  channel_->pan_table_index           = 0;
}

void
shutdown_dolby_sound_channel(DolbySoundChannel *channel_)
{
  ReleaseKnob(channel_->left_gain_knob);
  ReleaseKnob(channel_->right_gain_knob);
  ReleaseKnob(channel_->amplitude_knob);
  ReleaseKnob(channel_->frequency_knob);
  UnloadInstrument(channel_->sampler_instrument);
  UnloadInstrument(channel_->output_instrument);
  UnloadInstrument(channel_->left_multiply_instrument);
  UnloadInstrument(channel_->right_multiply_instrument);
}

void
shutdown_sound_effect_resources(void)
{
  DolbySoundChannel *channel;
  int32              channel_index;

  for(channel_index = 0; channel_index < g_DOLBY_SOUND_CHANNEL_COUNT; channel_index++)
    {
      channel = &g_DOLBY_SOUND_CHANNELS[channel_index];
      StopInstrument(channel->sampler_instrument, NULL);
      StopInstrument(channel->output_instrument, NULL);
      StopInstrument(channel->left_multiply_instrument, NULL);
      StopInstrument(channel->right_multiply_instrument, NULL);
      if(channel->attachment > 0)
        DetachSample(channel->attachment);
      shutdown_dolby_sound_channel(channel);
    }

  free_memory(g_DOLBY_SOUND_CHANNELS);
  g_DOLBY_SOUND_CHANNELS      = NULL;
  g_DOLBY_SOUND_CHANNEL_COUNT = 0;
}

int32
stop_dolby_sound_channel(int32 channel_index_)
{
  DolbySoundChannel *channel;

  if((channel_index_ < 0) || (channel_index_ > g_DOLBY_SOUND_CHANNEL_COUNT))
    return -1;

  channel = &g_DOLBY_SOUND_CHANNELS[channel_index_];
  PauseInstrument(channel->sampler_instrument);
  StopInstrument(channel->sampler_instrument, NULL);
  return 0;
}

void
calculate_dolby_channel_gains(StereoGain *gain_, int32 channel_index_, int32 location_)
{
  DolbySoundChannel *channel;
  int32              pan_index;
  int32              relative_location;

  channel = &g_DOLBY_SOUND_CHANNELS[channel_index_];
  if(location_ > DOLBY_LOCATION_MAXIMUM)
    location_ -= DOLBY_LOCATION_PERIOD;

  relative_location = location_ - channel->previous_location;
  if(relative_location > DOLBY_HALF_TURN)
    relative_location -= DOLBY_LOCATION_PERIOD;
  if(relative_location < -DOLBY_HALF_TURN)
    relative_location += DOLBY_LOCATION_PERIOD;

  pan_index = channel->pan_table_index + relative_location;
  if(pan_index < 0)
    pan_index += DOLBY_PAN_PERIOD;
  if(pan_index >= DOLBY_PAN_PERIOD)
    pan_index -= DOLBY_PAN_PERIOD;

  if(g_STEREO_PANNING_ENABLED == 0)
    {
      if((location_ >= 0) && (location_ <= DOLBY_QUADRANT_SIZE))
        pan_index = location_;
      if((location_ >= DOLBY_QUADRANT_SIZE) && (location_ <= DOLBY_HALF_TURN))
        pan_index = DOLBY_HALF_TURN - location_;
      if((location_ > DOLBY_HALF_TURN) && (location_ <= DOLBY_THREE_QUARTER_TURN))
        pan_index = (DOLBY_PAN_PERIOD + DOLBY_HALF_TURN) - location_;
      if(location_ >= DOLBY_THREE_QUARTER_TURN)
        pan_index = location_ + DOLBY_LOCATION_PERIOD;
    }

  channel->previous_location = location_;
  channel->pan_table_index   = pan_index;
  *gain_                     = g_DOLBY_GAIN_TABLE[pan_index];
}

int32
set_dolby_sound_channel_parameters(int32 channel_index_,
                                   int32 frequency_,
                                   int32 amplitude_,
                                   int32 location_)
{
  DolbySoundChannel *channel;
  StereoGain         gain;
  int32              result;

  if((location_ < 0) || (location_ > DOLBY_LOCATION_MAXIMUM))
    {
      kprintf("location parm out of bounds for TweakDolby.\n");
      fflush(stdout);
      if(location_ < 0)
        {
          location_ = -location_;
          location_ &= DOLBY_LOCATION_MAXIMUM;
          location_ = -location_;
        }
      else
        location_ &= DOLBY_LOCATION_MAXIMUM;
    }

  if((channel_index_ < 0) || (channel_index_ > g_DOLBY_SOUND_CHANNEL_COUNT))
    return -1;

  channel = &g_DOLBY_SOUND_CHANNELS[channel_index_];
  calculate_dolby_channel_gains(&gain, channel_index_, location_);
  result = TweakKnob(channel->left_gain_knob, gain.left);
  if(result >= 0)
    result = TweakKnob(channel->right_gain_knob, gain.right);
  if(result >= 0)
    result = TweakKnob(channel->frequency_knob, frequency_);
  if(result >= 0)
    result = TweakKnob(channel->amplitude_knob, amplitude_);

  return result;
}

int32
play_dolby_sound_channel(int32 channel_index_,
                         Item  sample_,
                         int32 frequency_,
                         int32 amplitude_,
                         int32 location_)
{
  DolbySoundChannel *channel;
  int32              result;

  if((channel_index_ < 0) || (channel_index_ > g_DOLBY_SOUND_CHANNEL_COUNT))
    return -1;

  channel = &g_DOLBY_SOUND_CHANNELS[channel_index_];
  if(channel->attachment != 0)
    {
      DetachSample(channel->attachment);
      channel->attachment = 0;
    }

  channel->attachment = AttachSample(channel->sampler_instrument, sample_, NULL);
  result = set_dolby_sound_channel_parameters(channel_index_, frequency_, amplitude_, location_);
  if(result < 0)
    return result;

  StartInstrument(channel->sampler_instrument, NULL);
  return 0;
}

int32
initialize_dolby_sound_pool(int32 channel_count_)
{
  DolbySoundChannel *channel;
  int32              channel_index;
  int32              result;

  g_DOLBY_SOUND_CHANNELS =
    (DolbySoundChannel *) allocate_named_memory(DOLBY_SOUND_ALLOCATION_NAME,
                                                channel_count_ * sizeof(DolbySoundChannel),
                                                0);
  if(g_DOLBY_SOUND_CHANNELS == NULL)
    return -1;

  for(channel_index = 0; channel_index < channel_count_; channel_index++)
    clear_dolby_sound_channel(&g_DOLBY_SOUND_CHANNELS[channel_index]);

  for(channel_index = 0; channel_index < channel_count_; channel_index++)
    {
      channel = &g_DOLBY_SOUND_CHANNELS[channel_index];
      result  = initialize_dolby_sound_channel(channel);
      if(result != 0)
        return result;

      g_DOLBY_SOUND_CHANNEL_COUNT++;
    }

  return 0;
}
