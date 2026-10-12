#pragma once

#include "base_types.h"

#define AUDIO_VOICE_COUNT 8

typedef struct AudioVoice
{
  int32 instrument;
  int32 attachment;
  int32 frequency_knob;
  int32 amplitude_knob;
  int32 left_gain_knob;
  int32 right_gain_knob;
  int32 state;
} AudioVoice;

typedef struct AudioVoiceMixer
{
  int32 instrument;
  int32 left_template;
  int32 right_template;
} AudioVoiceMixer;

extern AudioVoice      g_AUDIO_VOICES[AUDIO_VOICE_COUNT];
extern AudioVoiceMixer g_AUDIO_VOICE_MIXER;

void
clear_audio_voices(void);
void
shutdown_audio_voices(void);
int32
initialize_audio_voice_system_checked(void);
int32
initialize_audio_voice_mixer(void);
int32
initialize_audio_voice_system(void);
void
initialize_audio_voice_instruments(void);
void
initialize_audio_voice_knobs(int32 voice_index_);
void
ramp_down_audio_voice_amplitude(int32 voice_index_);
void
stop_audio_voice(int32 voice_index_);
int32
attach_audio_voice_sample(int32 voice_index_, int32 sample_);
void
set_audio_voice_frequency(int32 voice_index_, int32 frequency_);
void
set_audio_voice_amplitude(int32 voice_index_, int32 amplitude_);
void
set_audio_voice_pan(int32 voice_index_, int32 pan_);
int32
initialize_audio_voice(int32 voice_index_,
                       int32 sample_,
                       int32 frequency_,
                       int32 amplitude_,
                       int32 pan_);
