#include "audio_voice.h"

#include "sound_effect_state.h"
#include "string_utilities.h"

#include "audio.h"
#include "filefunctions.h"

#include "stdio.h"

#define AUDIO_GAIN_FULL 0x7FFF
#define AUDIO_PAN_CENTER 0x40
#define AUDIO_PAN_MAXIMUM 0x7F
#define AUDIO_PAN_SHIFT 9
#define AUDIO_RAMP_AMPLITUDE_HIGH 0x2000
#define AUDIO_RAMP_AMPLITUDE_MID 0x1000
#define AUDIO_RAMP_AMPLITUDE_LOW 0x0800
#define AUDIO_VOICE_FREQUENCY 0x2000
#define AUDIO_VOICE_PAN_RIGHT 0x7F
#define AUDIO_VOICE_FRONT_COUNT 4
#define AUDIO_VOICE_SAMPLE_OFFSET 3
#define AUDIO_VOICE_ENGINE_LEFT 4
#define AUDIO_VOICE_ENGINE_RIGHT 5
#define AUDIO_VOICE_ENGINE_SAMPLE 19
#define AUDIO_DIRECTORY_BYTE_COUNT 80
#define AUDIO_KNOB_NAME_BYTE_COUNT 24
#define AUDIO_INSTRUMENT_PRIORITY 100
#define AUDIO_DSP_DIRECTORY_PATH "/DriveData/dsp/"
#define AUDIO_MIXER_INSTRUMENT_NAME "mixer8x2.dsp"
#define AUDIO_MONO_TEMPLATE_NAME "varmono16.dsp"
#define AUDIO_SAMPLER_TEMPLATE_NAME "sampler.dsp"
#define AUDIO_MIXER_INPUT_NAME_FORMAT "Input%d"
#define AUDIO_MIXER_OUTPUT_NAME "Output"
#define AUDIO_FREQUENCY_KNOB_NAME "Frequency"
#define AUDIO_AMPLITUDE_KNOB_NAME "Amplitude"
#define AUDIO_LEFT_GAIN_KNOB_NAME_FORMAT "LeftGain%d"
#define AUDIO_RIGHT_GAIN_KNOB_NAME_FORMAT "RightGain%d"
#define AUDIO_START_TAG_END_INDEX 2
#define AUDIO_START_TAG_COUNT (AUDIO_START_TAG_END_INDEX + 1)

AudioVoice      g_AUDIO_VOICES[AUDIO_VOICE_COUNT];
AudioVoiceMixer g_AUDIO_VOICE_MIXER;

/* The original ordered acquisitions share one partial-initialization rollback. */
int32
initialize_audio_voice_system(void)
{
  char  current_directory[AUDIO_DIRECTORY_BYTE_COUNT];
  char  dsp_directory[AUDIO_DIRECTORY_BYTE_COUNT];
  char  knob_name[AUDIO_KNOB_NAME_BYTE_COUNT];
  Err   result;
  int32 voice_index;

  GetDirectory(current_directory, AUDIO_DIRECTORY_BYTE_COUNT);
  copy_string(dsp_directory, current_directory);
  append_string(dsp_directory, AUDIO_DSP_DIRECTORY_PATH);
  clear_audio_voices();

  g_AUDIO_VOICE_MIXER.instrument = LoadInstrument(AUDIO_MIXER_INSTRUMENT_NAME, 0, 0);
  if(g_AUDIO_VOICE_MIXER.instrument == 0)
    goto initialization_failed;

  g_AUDIO_VOICE_MIXER.left_template = LoadInsTemplate(AUDIO_MONO_TEMPLATE_NAME, 0);
  if(g_AUDIO_VOICE_MIXER.left_template == 0)
    goto initialization_failed;

  for(voice_index = 0; voice_index < AUDIO_VOICE_ENGINE_RIGHT + 1; voice_index++)
    {
      g_AUDIO_VOICES[voice_index].instrument =
        AllocInstrument(g_AUDIO_VOICE_MIXER.left_template, AUDIO_INSTRUMENT_PRIORITY);
      if(g_AUDIO_VOICES[voice_index].instrument == 0)
        goto initialization_failed;
    }

  g_AUDIO_VOICE_MIXER.right_template = LoadInsTemplate(AUDIO_SAMPLER_TEMPLATE_NAME, 0);
  if(g_AUDIO_VOICE_MIXER.right_template == 0)
    goto initialization_failed;

  for(voice_index = AUDIO_VOICE_ENGINE_RIGHT + 1; voice_index < AUDIO_VOICE_COUNT; voice_index++)
    {
      g_AUDIO_VOICES[voice_index].instrument =
        AllocInstrument(g_AUDIO_VOICE_MIXER.right_template, AUDIO_INSTRUMENT_PRIORITY);
      if(g_AUDIO_VOICES[voice_index].instrument == 0)
        goto initialization_failed;
    }

  for(voice_index = 0; voice_index < AUDIO_VOICE_COUNT; voice_index++)
    {
      sprintf(knob_name, AUDIO_MIXER_INPUT_NAME_FORMAT, voice_index);
      result = ConnectInstruments(g_AUDIO_VOICES[voice_index].instrument,
                                  AUDIO_MIXER_OUTPUT_NAME,
                                  g_AUDIO_VOICE_MIXER.instrument,
                                  knob_name);
      if(result < 0)
        goto initialization_failed;

      g_AUDIO_VOICES[voice_index].frequency_knob =
        GrabKnob(g_AUDIO_VOICES[voice_index].instrument, AUDIO_FREQUENCY_KNOB_NAME);
      if(g_AUDIO_VOICES[voice_index].frequency_knob < 0)
        goto initialization_failed;

      g_AUDIO_VOICES[voice_index].amplitude_knob =
        GrabKnob(g_AUDIO_VOICES[voice_index].instrument, AUDIO_AMPLITUDE_KNOB_NAME);
      if(g_AUDIO_VOICES[voice_index].amplitude_knob < 0)
        goto initialization_failed;

      sprintf(knob_name, AUDIO_LEFT_GAIN_KNOB_NAME_FORMAT, voice_index);
      g_AUDIO_VOICES[voice_index].left_gain_knob =
        GrabKnob(g_AUDIO_VOICE_MIXER.instrument, knob_name);
      if(g_AUDIO_VOICES[voice_index].left_gain_knob < 0)
        goto initialization_failed;

      sprintf(knob_name, AUDIO_RIGHT_GAIN_KNOB_NAME_FORMAT, voice_index);
      g_AUDIO_VOICES[voice_index].right_gain_knob =
        GrabKnob(g_AUDIO_VOICE_MIXER.instrument, knob_name);
      if(g_AUDIO_VOICES[voice_index].right_gain_knob < 0)
        goto initialization_failed;

      initialize_audio_voice_knobs(voice_index);
    }

  result = StartInstrument(g_AUDIO_VOICE_MIXER.instrument, NULL);
  if(result < 0)
    goto initialization_failed;

  ChangeDirectory(current_directory);
  return 1;

initialization_failed:
  ChangeDirectory(current_directory);
  shutdown_audio_voices();
  return 0;
}

void
initialize_audio_voice_instruments(void)
{
  int32 voice_index;

  fflush(stdout);
  for(voice_index = 0; voice_index < AUDIO_VOICE_FRONT_COUNT; voice_index++)
    {
      fflush(stdout);
      initialize_audio_voice(voice_index,
                             g_VEHICLE_SOUND_SAMPLES[voice_index + AUDIO_VOICE_SAMPLE_OFFSET],
                             AUDIO_VOICE_FREQUENCY,
                             0,
                             (voice_index & 1) * AUDIO_VOICE_PAN_RIGHT);
    }
  initialize_audio_voice(AUDIO_VOICE_ENGINE_LEFT,
                         g_VEHICLE_SOUND_SAMPLES[AUDIO_VOICE_ENGINE_SAMPLE],
                         AUDIO_VOICE_FREQUENCY,
                         0,
                         0);
  initialize_audio_voice(AUDIO_VOICE_ENGINE_RIGHT,
                         g_VEHICLE_SOUND_SAMPLES[AUDIO_VOICE_ENGINE_SAMPLE],
                         AUDIO_VOICE_FREQUENCY,
                         0,
                         AUDIO_VOICE_PAN_RIGHT);
}

void
clear_audio_voices(void)
{
  AudioVoice *voice;
  int32       voice_index;

  for(voice_index = 0; voice_index < AUDIO_VOICE_COUNT; voice_index++)
    {
      voice                  = &g_AUDIO_VOICES[voice_index];
      voice->instrument      = 0;
      voice->attachment      = 0;
      voice->amplitude_knob  = 0;
      voice->frequency_knob  = 0;
      voice->left_gain_knob  = 0;
      voice->right_gain_knob = 0;
      voice->state           = 0;
    }
}

void
shutdown_audio_voices(void)
{
  AudioVoice *voice;
  int32       voice_index;

  for(voice_index = 0; voice_index < AUDIO_VOICE_COUNT; voice_index++)
    {
      voice = &g_AUDIO_VOICES[voice_index];
      if(voice->instrument > 0)
        {
          ramp_down_audio_voice_amplitude(voice_index);
          StopInstrument(voice->instrument, NULL);
        }

      ReleaseKnob(voice->amplitude_knob);
      ReleaseKnob(voice->frequency_knob);
      ReleaseKnob(voice->left_gain_knob);
      ReleaseKnob(voice->right_gain_knob);
      if(voice->attachment > 0)
        DetachSample(voice->attachment);
      if(voice->instrument > 0)
        FreeInstrument(voice->instrument);
    }

  UnloadInsTemplate(g_AUDIO_VOICE_MIXER.left_template);
  UnloadInsTemplate(g_AUDIO_VOICE_MIXER.right_template);
  StopInstrument(g_AUDIO_VOICE_MIXER.instrument, NULL);
  UnloadInstrument(g_AUDIO_VOICE_MIXER.instrument);
}

void
initialize_audio_voice_knobs(int32 voice_index_)
{
  AudioVoice *voice;

  voice = &g_AUDIO_VOICES[voice_index_];
  TweakRawKnob(voice->frequency_knob, 0);
  TweakRawKnob(voice->amplitude_knob, 0);
  TweakRawKnob(voice->left_gain_knob, AUDIO_GAIN_FULL);
  TweakRawKnob(voice->right_gain_knob, AUDIO_GAIN_FULL);
}

void
ramp_down_audio_voice_amplitude(int32 voice_index_)
{
  TweakRawKnob(g_AUDIO_VOICES[voice_index_].amplitude_knob, AUDIO_RAMP_AMPLITUDE_HIGH);
  TweakRawKnob(g_AUDIO_VOICES[voice_index_].amplitude_knob, AUDIO_RAMP_AMPLITUDE_MID);
  TweakRawKnob(g_AUDIO_VOICES[voice_index_].amplitude_knob, AUDIO_RAMP_AMPLITUDE_LOW);
  TweakRawKnob(g_AUDIO_VOICES[voice_index_].amplitude_knob, 0);
}

void
stop_audio_voice(int32 voice_index_)
{
  PauseInstrument(g_AUDIO_VOICES[voice_index_].instrument);
  StopInstrument(g_AUDIO_VOICES[voice_index_].instrument, NULL);
}

int32
attach_audio_voice_sample(int32 voice_index_, int32 sample_)
{
  AudioVoice *voice;
  Err         result;

  voice = &g_AUDIO_VOICES[voice_index_];
  if(voice->attachment > 0)
    {
      stop_audio_voice(voice_index_);
      result = DetachSample(voice->attachment);
      if(result < 0)
        return 0;

      voice->attachment = 0;
    }

  voice->attachment = AttachSample(voice->instrument, sample_, NULL);
  if(voice->attachment >= 0)
    return 1;

  return 0;
}

void
set_audio_voice_frequency(int32 voice_index_, int32 frequency_)
{
  TweakRawKnob(g_AUDIO_VOICES[voice_index_].frequency_knob, frequency_);
}

void
set_audio_voice_amplitude(int32 voice_index_, int32 amplitude_)
{
  TweakRawKnob(g_AUDIO_VOICES[voice_index_].amplitude_knob, amplitude_ >> 1);
}

void
set_audio_voice_pan(int32 voice_index_, int32 pan_)
{
  int32 left_gain;
  int32 right_gain;

  left_gain  = AUDIO_GAIN_FULL;
  right_gain = AUDIO_GAIN_FULL;
  if(pan_ < AUDIO_PAN_CENTER)
    right_gain = (pan_ << AUDIO_PAN_SHIFT);
  else
    left_gain = ((AUDIO_PAN_MAXIMUM - pan_) << AUDIO_PAN_SHIFT);

  TweakRawKnob(g_AUDIO_VOICES[voice_index_].left_gain_knob, left_gain);
  TweakRawKnob(g_AUDIO_VOICES[voice_index_].right_gain_knob, right_gain);
}

int32
initialize_audio_voice(int32 voice_index_,
                       int32 sample_,
                       int32 frequency_,
                       int32 amplitude_,
                       int32 pan_)
{
  TagArg start_tags[AUDIO_START_TAG_COUNT];

  if(attach_audio_voice_sample(voice_index_, sample_) == 0)
    return 0;

  set_audio_voice_pan(voice_index_, pan_);
  /* Frequency values are raw DSP rates, not 16.16 Hertz. */
  start_tags[0].ta_Tag                         = AF_TAG_RATE;
  start_tags[0].ta_Arg                         = (void *) frequency_;
  start_tags[1].ta_Tag                         = AF_TAG_AMPLITUDE;
  start_tags[1].ta_Arg                         = (void *) (amplitude_ >> 1);
  start_tags[AUDIO_START_TAG_END_INDEX].ta_Tag = TAG_END;
  start_tags[AUDIO_START_TAG_END_INDEX].ta_Arg = NULL;

  return StartInstrument(g_AUDIO_VOICES[voice_index_].instrument, start_tags);
}
