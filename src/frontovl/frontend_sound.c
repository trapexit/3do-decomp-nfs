#include "frontend_sound.h"

#include "dsp_memory.h"
#include "frontend_audio_state.h"
#include "item_cleanup.h"
#include "race_state.h"
#include "runtime_audio_template.h"
#include "runtime_memory.h"
#include "sdk_veneers.h"

#include "audio.h"
#include "filefunctions.h"

#define AUDIO_FILE_DATA_FLAGS 0x20
#define AUDIO_FILE_HEADER_SIZE 8
#define FRONTEND_SOUND_BANK_PATH "DriveData/aiff/FESound.AIFFfam"
#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

COMPILE_TIME_SIZE_ASSERT(item_size, sizeof(Item) == 4);

COMPILE_TIME_SIZE_ASSERT(frontend_sound_voice_size, sizeof(FrontendSoundVoice) == 28);

enum
{
  FRONTEND_SOUND_VOICE     = 0,
  FRONTEND_SOUND_RATE      = 0x4000,
  FRONTEND_SOUND_AMPLITUDE = 0x7FFF,
  FRONTEND_SOUND_PRIORITY  = 0x40,
  CONFIRMATION_SOUND_INDEX = 8,
  NAVIGATION_SOUND_INDEX   = 9,
  ERROR_SOUND_INDEX        = 10
};

#define FIRST_GENERAL_FRONTEND_SOUND_INDEX 0
#define LAST_GENERAL_FRONTEND_SOUND_INDEX 7
#define FRONTEND_SOUND_VOICE_COUNT 3
#define FRONTEND_SOUND_FULL_GAIN 0x1FF
#define FRONTEND_MIXER_VOICE_INDEX 0
#define FRONTEND_STREAM_VOICE_INDEX 1
#define FRONTEND_EFFECTS_VOICE_INDEX 2
#define FRONTEND_STEREO_GAIN 0x7FFF
#define FRONTEND_MONO_GAIN 0x3FFF
#define FRONTEND_AUDIO_DIRECTORY_BYTE_COUNT 80
#define FRONTEND_AUDIO_KNOB_NAME_BYTE_COUNT 24
#define FRONTEND_AUDIO_INSTRUMENT_PRIORITY 100
#define FRONTEND_AUDIO_DSP_DIRECTORY "drivedata/dsp"
#define FRONTEND_AUDIO_MIXER_PATH "mixer4x2.dsp"
#define FRONTEND_AUDIO_SAMPLER_TEMPLATE_PATH "sampler.dsp"
#define FRONTEND_AUDIO_STREAM_PATH "halfmonosample.dsp"
#define FRONTEND_AUDIO_EFFECTS_PATH "dcsqxdstereo.dsp"
#define FRONTEND_AUDIO_INPUT_FORMAT "Input%ld"
#define FRONTEND_AUDIO_LEFT_GAIN_FORMAT "LeftGain%ld"
#define FRONTEND_AUDIO_RIGHT_GAIN_FORMAT "RightGain%ld"
#define FRONTEND_AUDIO_OUTPUT_NAME "Output"
#define FRONTEND_AUDIO_LEFT_OUTPUT_NAME "LeftOutput"
#define FRONTEND_AUDIO_RIGHT_OUTPUT_NAME "RightOutput"
#define FRONTEND_AUDIO_FREQUENCY_KNOB_NAME "Frequency"
#define FRONTEND_AUDIO_AMPLITUDE_KNOB_NAME "Amplitude"
#define FRONTEND_PAN_CENTER 0x40
#define FRONTEND_PAN_MAXIMUM 0x7F
#define FRONTEND_PAN_GAIN_SHIFT 9
#define FRONTEND_PLAYBACK_TAG_COUNT 3
#define FRONTEND_RATE_TAG_INDEX 0
#define FRONTEND_AMPLITUDE_TAG_INDEX 1
#define FRONTEND_END_TAG_INDEX 2
Item g_FRONTEND_MASTER_KNOBS[2] = { 0 };

typedef char FrontendMasterKnobsSizeCheck[sizeof(g_FRONTEND_MASTER_KNOBS) == 8 ? 1 : -1];

void
initialize_frontend_sound_voices(void)
{
  FrontendSoundVoice *voice;
  int32               voice_index;

  for(voice_index = 0; voice_index < FRONTEND_SOUND_VOICE_COUNT; ++voice_index)
    {
      voice                  = &g_FRONTEND_SOUND_VOICES[voice_index];
      voice->instrument      = 0;
      voice->attachment      = 0;
      voice->frequency_knob  = 0;
      voice->amplitude_knob  = 0;
      voice->left_gain_knob  = 0;
      voice->right_gain_knob = 0;
      voice->sample          = 0;
    }
}

void
reset_frontend_sound_voice_controls(int32 voice_index_)
{
  FrontendSoundVoice *voice;

  voice = &g_FRONTEND_SOUND_VOICES[voice_index_];
  TweakRawKnob(voice->amplitude_knob, 0);
  TweakRawKnob(voice->frequency_knob, 0);
  TweakRawKnob(voice->left_gain_knob, FRONTEND_SOUND_FULL_GAIN);
  TweakRawKnob(voice->right_gain_knob, FRONTEND_SOUND_FULL_GAIN);
}

void
release_frontend_audio_resources(void)
{
  FrontendSoundVoice *voice;
  int32               voice_index;

  --g_FRONTEND_AUDIO_RESOURCE_REFERENCE_COUNT;
  if(g_FRONTEND_AUDIO_RESOURCE_REFERENCE_COUNT != 0)
    {
      return;
    }

  for(voice_index = 0; voice_index < FRONTEND_SOUND_VOICE_COUNT; ++voice_index)
    {
      stop_frontend_sound_voice(voice_index);
    }

  voice = &g_FRONTEND_SOUND_VOICES[FRONTEND_MIXER_VOICE_INDEX];
  delete_resource_item(voice->frequency_knob);
  delete_resource_item(voice->amplitude_knob);
  delete_resource_item(voice->left_gain_knob);
  delete_resource_item(voice->right_gain_knob);
  if(voice->attachment > 0)
    {
      DetachSample(voice->attachment);
    }
  if(voice->instrument > 0)
    {
      delete_audio_item(voice->instrument);
    }

  UnloadInsTemplate(g_FRONTEND_OUTPUT_TEMPLATE);

  voice = &g_FRONTEND_SOUND_VOICES[FRONTEND_STREAM_VOICE_INDEX];
  delete_resource_item(voice->amplitude_knob);
  delete_resource_item(voice->left_gain_knob);
  delete_resource_item(voice->right_gain_knob);
  UnloadInstrument(voice->instrument);

  voice = &g_FRONTEND_SOUND_VOICES[FRONTEND_EFFECTS_VOICE_INDEX];
  delete_resource_item(voice->amplitude_knob);
  delete_resource_item(voice->left_gain_knob);
  delete_resource_item(voice->right_gain_knob);
  delete_resource_item(g_FRONTEND_MASTER_KNOBS[0]);
  delete_resource_item(g_FRONTEND_MASTER_KNOBS[1]);
  UnloadInstrument(voice->instrument);

  StopInstrument(g_FRONTEND_OUTPUT_INSTRUMENT, NULL);
  UnloadInstrument(g_FRONTEND_OUTPUT_INSTRUMENT);
}

void
configure_stereo_frontend_audio(void)
{
  FrontendSoundVoice *voice;

  voice = &g_FRONTEND_SOUND_VOICES[FRONTEND_EFFECTS_VOICE_INDEX];
  TweakRawKnob(g_FRONTEND_MASTER_KNOBS[0], 0);
  TweakRawKnob(g_FRONTEND_MASTER_KNOBS[1], 0);
  TweakRawKnob(voice->left_gain_knob, FRONTEND_STEREO_GAIN);
  TweakRawKnob(voice->right_gain_knob, FRONTEND_STEREO_GAIN);
}

void
configure_mono_frontend_audio(void)
{
  FrontendSoundVoice *voice;

  voice = &g_FRONTEND_SOUND_VOICES[FRONTEND_EFFECTS_VOICE_INDEX];
  TweakRawKnob(g_FRONTEND_MASTER_KNOBS[0], FRONTEND_MONO_GAIN);
  TweakRawKnob(g_FRONTEND_MASTER_KNOBS[1], FRONTEND_MONO_GAIN);
  TweakRawKnob(voice->left_gain_knob, FRONTEND_MONO_GAIN);
  TweakRawKnob(voice->right_gain_knob, FRONTEND_MONO_GAIN);
}

int32
initialize_frontend_audio_resources(void)
{
  char                original_directory[FRONTEND_AUDIO_DIRECTORY_BYTE_COUNT];
  char                knob_name[FRONTEND_AUDIO_KNOB_NAME_BYTE_COUNT];
  FrontendSoundVoice *voice;
  Item               *master_knobs;
  int32               voice_index;
  Err                 result;

  master_knobs = g_FRONTEND_MASTER_KNOBS;
  ++g_FRONTEND_AUDIO_RESOURCE_REFERENCE_COUNT;
  if(g_FRONTEND_AUDIO_RESOURCE_REFERENCE_COUNT != 1)
    {
      return g_FRONTEND_AUDIO_RESOURCE_REFERENCE_COUNT;
    }

  GetDirectory(original_directory, FRONTEND_AUDIO_DIRECTORY_BYTE_COUNT);
  ChangeDirectory(FRONTEND_AUDIO_DSP_DIRECTORY);
  initialize_frontend_sound_voices();

  g_FRONTEND_OUTPUT_INSTRUMENT = LoadInstrument(FRONTEND_AUDIO_MIXER_PATH, 0, 0);
  if(g_FRONTEND_OUTPUT_INSTRUMENT < 0)
    {
      goto fail;
    }
  g_FRONTEND_OUTPUT_TEMPLATE =
    LOAD_INSTRUMENT_TEMPLATE_VENEER(FRONTEND_AUDIO_SAMPLER_TEMPLATE_PATH, 0);
  if(g_FRONTEND_OUTPUT_TEMPLATE < 0)
    {
      goto fail;
    }

  voice = &g_FRONTEND_SOUND_VOICES[FRONTEND_MIXER_VOICE_INDEX];
  voice->instrument =
    AllocInstrument(g_FRONTEND_OUTPUT_TEMPLATE, FRONTEND_AUDIO_INSTRUMENT_PRIORITY);
  if(voice->instrument < 0)
    {
      goto fail;
    }

  voice             = &g_FRONTEND_SOUND_VOICES[FRONTEND_STREAM_VOICE_INDEX];
  voice->instrument = LoadInstrument(FRONTEND_AUDIO_STREAM_PATH, 0, 0);
  if(voice->instrument < 0)
    {
      goto fail;
    }

  voice             = &g_FRONTEND_SOUND_VOICES[FRONTEND_EFFECTS_VOICE_INDEX];
  voice->instrument = LoadInstrument(FRONTEND_AUDIO_EFFECTS_PATH, 0, 0);
  if(voice->instrument < 0)
    {
      goto fail;
    }

  for(voice_index = 0; voice_index < 1; ++voice_index)
    {
      voice = &g_FRONTEND_SOUND_VOICES[voice_index];
      runtime_format_string(knob_name, FRONTEND_AUDIO_INPUT_FORMAT, voice_index);
      result = ConnectInstruments(voice->instrument,
                                  FRONTEND_AUDIO_OUTPUT_NAME,
                                  g_FRONTEND_OUTPUT_INSTRUMENT,
                                  knob_name);
      if(result < 0)
        {
          goto fail;
        }
      voice->frequency_knob = GrabKnob(voice->instrument, FRONTEND_AUDIO_FREQUENCY_KNOB_NAME);
      if(voice->frequency_knob < 0)
        {
          goto fail;
        }
      voice->amplitude_knob = GrabKnob(voice->instrument, FRONTEND_AUDIO_AMPLITUDE_KNOB_NAME);
      if(voice->amplitude_knob < 0)
        {
          goto fail;
        }
      runtime_format_string(knob_name, FRONTEND_AUDIO_LEFT_GAIN_FORMAT, voice_index);
      voice->left_gain_knob = GrabKnob(g_FRONTEND_OUTPUT_INSTRUMENT, knob_name);
      if(voice->left_gain_knob < 0)
        {
          goto fail;
        }
      runtime_format_string(knob_name, FRONTEND_AUDIO_RIGHT_GAIN_FORMAT, voice_index);
      voice->right_gain_knob = GrabKnob(g_FRONTEND_OUTPUT_INSTRUMENT, knob_name);
      if(voice->right_gain_knob < 0)
        {
          goto fail;
        }
      reset_frontend_sound_voice_controls(voice_index);
    }

  voice = &g_FRONTEND_SOUND_VOICES[FRONTEND_STREAM_VOICE_INDEX];
  runtime_format_string(knob_name, FRONTEND_AUDIO_INPUT_FORMAT, FRONTEND_STREAM_VOICE_INDEX);
  result = ConnectInstruments(voice->instrument,
                              FRONTEND_AUDIO_OUTPUT_NAME,
                              g_FRONTEND_OUTPUT_INSTRUMENT,
                              knob_name);
  if(result < 0)
    {
      goto fail;
    }
  voice->amplitude_knob = GrabKnob(voice->instrument, FRONTEND_AUDIO_AMPLITUDE_KNOB_NAME);
  if(voice->amplitude_knob < 0)
    {
      goto fail;
    }
  runtime_format_string(knob_name, FRONTEND_AUDIO_LEFT_GAIN_FORMAT, FRONTEND_STREAM_VOICE_INDEX);
  voice->left_gain_knob = GrabKnob(g_FRONTEND_OUTPUT_INSTRUMENT, knob_name);
  if(voice->left_gain_knob < 0)
    {
      goto fail;
    }
  runtime_format_string(knob_name, FRONTEND_AUDIO_RIGHT_GAIN_FORMAT, FRONTEND_STREAM_VOICE_INDEX);
  voice->right_gain_knob = GrabKnob(g_FRONTEND_OUTPUT_INSTRUMENT, knob_name);
  if(voice->right_gain_knob < 0)
    {
      goto fail;
    }
  TweakRawKnob(voice->amplitude_knob, 0);
  TweakRawKnob(voice->left_gain_knob, FRONTEND_STEREO_GAIN);
  TweakRawKnob(voice->right_gain_knob, FRONTEND_STEREO_GAIN);

  voice = &g_FRONTEND_SOUND_VOICES[FRONTEND_EFFECTS_VOICE_INDEX];
  runtime_format_string(knob_name, FRONTEND_AUDIO_INPUT_FORMAT, FRONTEND_EFFECTS_VOICE_INDEX);
  result = ConnectInstruments(voice->instrument,
                              FRONTEND_AUDIO_LEFT_OUTPUT_NAME,
                              g_FRONTEND_OUTPUT_INSTRUMENT,
                              knob_name);
  if(result < 0)
    {
      goto fail;
    }
  runtime_format_string(knob_name, FRONTEND_AUDIO_INPUT_FORMAT, FRONTEND_EFFECTS_VOICE_INDEX + 1);
  result = ConnectInstruments(voice->instrument,
                              FRONTEND_AUDIO_RIGHT_OUTPUT_NAME,
                              g_FRONTEND_OUTPUT_INSTRUMENT,
                              knob_name);
  if(result < 0)
    {
      goto fail;
    }
  voice->amplitude_knob = GrabKnob(voice->instrument, FRONTEND_AUDIO_AMPLITUDE_KNOB_NAME);
  if(voice->amplitude_knob < 0)
    {
      goto fail;
    }
  runtime_format_string(knob_name, FRONTEND_AUDIO_LEFT_GAIN_FORMAT, FRONTEND_EFFECTS_VOICE_INDEX);
  voice->left_gain_knob = GrabKnob(g_FRONTEND_OUTPUT_INSTRUMENT, knob_name);
  if(voice->left_gain_knob < 0)
    {
      goto fail;
    }
  runtime_format_string(knob_name,
                        FRONTEND_AUDIO_RIGHT_GAIN_FORMAT,
                        FRONTEND_EFFECTS_VOICE_INDEX + 1);
  voice->right_gain_knob = GrabKnob(g_FRONTEND_OUTPUT_INSTRUMENT, knob_name);
  if(voice->right_gain_knob < 0)
    {
      goto fail;
    }
  runtime_format_string(knob_name, FRONTEND_AUDIO_RIGHT_GAIN_FORMAT, FRONTEND_EFFECTS_VOICE_INDEX);
  master_knobs[0] = GrabKnob(g_FRONTEND_OUTPUT_INSTRUMENT, knob_name);
  runtime_format_string(knob_name,
                        FRONTEND_AUDIO_LEFT_GAIN_FORMAT,
                        FRONTEND_EFFECTS_VOICE_INDEX + 1);
  master_knobs[1] = GrabKnob(g_FRONTEND_OUTPUT_INSTRUMENT, knob_name);

  if(RACE_CONFIGURATION->stereo_enabled != 0)
    {
      configure_stereo_frontend_audio();
    }
  else
    {
      configure_mono_frontend_audio();
    }
  TweakRawKnob(voice->amplitude_knob, 0);
  TweakRawKnob(voice->left_gain_knob, FRONTEND_STEREO_GAIN);
  TweakRawKnob(voice->right_gain_knob, FRONTEND_STEREO_GAIN);

  result = StartInstrument(g_FRONTEND_OUTPUT_INSTRUMENT, NULL);
  if(result < 0)
    {
      goto fail;
    }
  ChangeDirectory(original_directory);
  return 1;

fail:
  ChangeDirectory(original_directory);
  release_frontend_audio_resources();
  return 0;
}

Item
stop_frontend_sound_voice(int32 voice_index_)
{
  FrontendSoundVoice *voice;

  voice = &g_FRONTEND_SOUND_VOICES[voice_index_];
  PauseInstrument(voice->instrument);
  StopInstrument(voice->instrument, NULL);
  return voice->instrument;
}

int32
attach_frontend_sound_sample(int32 voice_index_, Item sample_)
{
  FrontendSoundVoice *voice;

  voice = &g_FRONTEND_SOUND_VOICES[voice_index_];
  if(voice->attachment > 0)
    {
      stop_frontend_sound_voice(voice_index_);
      if(DetachSample(voice->attachment) < 0)
        {
          return 0;
        }
    }

  voice->attachment = AttachSample(voice->instrument, sample_, NULL);
  if(voice->attachment < 0)
    {
      return 0;
    }
  return 1;
}

void
set_frontend_sound_pan(int32 voice_index_, int32 pan_)
{
  FrontendSoundVoice *voice;
  int32               left_gain;
  int32               right_gain;

  left_gain  = FRONTEND_STEREO_GAIN;
  right_gain = FRONTEND_STEREO_GAIN;
  if(pan_ < FRONTEND_PAN_CENTER)
    {
      right_gain = pan_ << FRONTEND_PAN_GAIN_SHIFT;
    }
  else
    {
      left_gain = (FRONTEND_PAN_MAXIMUM - pan_) << FRONTEND_PAN_GAIN_SHIFT;
    }

  voice = &g_FRONTEND_SOUND_VOICES[voice_index_];
  TweakRawKnob(voice->left_gain_knob, left_gain);
  TweakRawKnob(voice->right_gain_knob, right_gain);
}

void
play_frontend_sample(int32 voice_index_, Item sample_, int32 rate_, int32 amplitude_, int32 pan_)
{
  TagArg              tags[FRONTEND_PLAYBACK_TAG_COUNT];
  FrontendSoundVoice *voice;

  if(attach_frontend_sound_sample(voice_index_, sample_) == 0)
    {
      return;
    }
  set_frontend_sound_pan(voice_index_, pan_);

  tags[FRONTEND_RATE_TAG_INDEX].ta_Tag      = AF_TAG_RATE;
  tags[FRONTEND_RATE_TAG_INDEX].ta_Arg      = (void *) rate_;
  tags[FRONTEND_AMPLITUDE_TAG_INDEX].ta_Tag = AF_TAG_AMPLITUDE;
  tags[FRONTEND_AMPLITUDE_TAG_INDEX].ta_Arg = (void *) (amplitude_ >> 1);
  tags[FRONTEND_END_TAG_INDEX].ta_Tag       = TAG_END;
  tags[FRONTEND_END_TAG_INDEX].ta_Arg       = NULL;

  voice = &g_FRONTEND_SOUND_VOICES[voice_index_];
  StartInstrument(voice->instrument, tags);
}

int32 g_FRONTEND_SOUND_BANK_REFERENCE_COUNT = 0;
Item  g_FRONTEND_SOUND_SAMPLES[11]          = { 0 };

Item
load_frontend_sample(const char *path_)
{
  uint8 *file_data;
  Item   sample;

  file_data = (uint8 *) load_runtime_file_data(path_, AUDIO_FILE_DATA_FLAGS);
  if(file_data == NULL)
    {
      return 0;
    }

  sample = DefineSampleHere(file_data,
                            ((uint32 *) file_data)[1] + AUDIO_FILE_HEADER_SIZE,
                            allocate_dsp_memory,
                            free_dsp_memory);
  if(sample < 0)
    {
      sample = 0;
    }
  free_runtime_memory(file_data);
  return sample;
}

void
load_frontend_sound_bank(void)
{
  uint8 **sample_data;
  uint8  *first_sample;
  uint8  *previous_sample;
  uint8  *current_sample;
  uint32  file_size;
  int32   index;

  if(g_FRONTEND_SOUND_BANK_REFERENCE_COUNT++ != 0)
    {
      return;
    }

  sample_data = (uint8 **) load_runtime_pointer_array(FRONTEND_SOUND_BANK_PATH);
  if(sample_data == NULL)
    {
      return;
    }

  file_size       = get_runtime_file_size(FRONTEND_SOUND_BANK_PATH);
  first_sample    = (uint8 *) get_runtime_pointer_array_element(sample_data, 0);
  previous_sample = first_sample;
  for(index = 1; index < FRONTEND_SOUND_SAMPLE_COUNT; index++)
    {
      current_sample = (uint8 *) get_runtime_pointer_array_element(sample_data, index);
      g_FRONTEND_SOUND_SAMPLES[index - 1] = DefineSampleHere(previous_sample,
                                                             current_sample - previous_sample,
                                                             allocate_dsp_memory,
                                                             free_dsp_memory);
      previous_sample                     = current_sample;
    }
  g_FRONTEND_SOUND_SAMPLES[FRONTEND_SOUND_SAMPLE_COUNT - 1] =
    DefineSampleHere(previous_sample,
                     first_sample + file_size - previous_sample,
                     allocate_dsp_memory,
                     free_dsp_memory);
  free_runtime_memory(sample_data);
}

void
unload_frontend_sound_bank(void)
{
  int32 index;

  g_FRONTEND_SOUND_BANK_REFERENCE_COUNT--;
  if(g_FRONTEND_SOUND_BANK_REFERENCE_COUNT != 0)
    {
      return;
    }

  for(index = 0; index < FRONTEND_SOUND_SAMPLE_COUNT; index++)
    {
      if(g_FRONTEND_SOUND_SAMPLES[index] > 0)
        {
          UnloadSample(g_FRONTEND_SOUND_SAMPLES[index]);
        }
    }
}

void
play_confirmation_sound(void)
{
  Item sample;

  sample = g_FRONTEND_SOUND_SAMPLES[CONFIRMATION_SOUND_INDEX];
  if(sample > 0)
    {
      play_frontend_sample(FRONTEND_SOUND_VOICE,
                           sample,
                           FRONTEND_SOUND_RATE,
                           FRONTEND_SOUND_AMPLITUDE,
                           FRONTEND_SOUND_PRIORITY);
    }
}

void
play_frontend_sound(int32 sound_index_)
{
  Item sample;

  if((sound_index_ < FIRST_GENERAL_FRONTEND_SOUND_INDEX) ||
     (sound_index_ > LAST_GENERAL_FRONTEND_SOUND_INDEX))
    {
      return;
    }

  sample = g_FRONTEND_SOUND_SAMPLES[sound_index_];
  if(sample > 0)
    {
      play_frontend_sample(FRONTEND_SOUND_VOICE,
                           sample,
                           FRONTEND_SOUND_RATE,
                           FRONTEND_SOUND_AMPLITUDE,
                           FRONTEND_SOUND_PRIORITY);
    }
}

void
play_navigation_sound(void)
{
  Item sample;

  sample = g_FRONTEND_SOUND_SAMPLES[NAVIGATION_SOUND_INDEX];
  if(sample > 0)
    {
      play_frontend_sample(FRONTEND_SOUND_VOICE,
                           sample,
                           FRONTEND_SOUND_RATE,
                           FRONTEND_SOUND_AMPLITUDE,
                           FRONTEND_SOUND_PRIORITY);
    }
}

void
play_error_sound(void)
{
  Item sample;

  sample = g_FRONTEND_SOUND_SAMPLES[ERROR_SOUND_INDEX];
  if(sample > 0)
    {
      play_frontend_sample(FRONTEND_SOUND_VOICE,
                           sample,
                           FRONTEND_SOUND_RATE,
                           FRONTEND_SOUND_AMPLITUDE,
                           FRONTEND_SOUND_PRIORITY);
    }
}

void
stop_frontend_sound(void)
{
  Item attachment;

  attachment = g_FRONTEND_SOUND_VOICES[FRONTEND_SOUND_VOICE].attachment;
  if(attachment <= 0)
    {
      return;
    }

  stop_frontend_sound_voice(FRONTEND_SOUND_VOICE);
  DetachSample(g_FRONTEND_SOUND_VOICES[FRONTEND_SOUND_VOICE].attachment);
  g_FRONTEND_SOUND_VOICES[FRONTEND_SOUND_VOICE].attachment = 0;
}
