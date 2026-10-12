#pragma once

#include "sdk_veneers.h"

#include "types.h"

#define FRONTEND_SOUND_SAMPLE_COUNT 11

typedef void *(*AudioSampleAllocateFunction)(uint32 byte_count_);
typedef void  (*AudioSampleFreeFunction)(void *memory_);

typedef struct FrontendSoundVoice
{
  Item instrument;
  Item attachment;
  Item frequency_knob;
  Item amplitude_knob;
  Item left_gain_knob;
  Item right_gain_knob;
  Item sample;
} FrontendSoundVoice;

extern int32              g_FRONTEND_SOUND_BANK_REFERENCE_COUNT;
extern Item               g_FRONTEND_SOUND_SAMPLES[FRONTEND_SOUND_SAMPLE_COUNT];
extern FrontendSoundVoice g_FRONTEND_SOUND_VOICES[3];

void
play_frontend_sample(int32 voice_index_, Item sample_, int32 rate_, int32 amplitude_, int32 pan_);

Item
load_frontend_sample(const char *path_);
void
load_frontend_sound_bank(void);
void
unload_frontend_sound_bank(void);
void
play_frontend_sound(int32 sound_index_);
void
play_confirmation_sound(void);
void
play_navigation_sound(void);
void
play_error_sound(void);
void
stop_frontend_sound(void);
void
initialize_frontend_sound_voices(void);
void
reset_frontend_sound_voice_controls(int32 voice_index_);
void
release_frontend_audio_resources(void);
void
configure_stereo_frontend_audio(void);
void
configure_mono_frontend_audio(void);
int32
initialize_frontend_audio_resources(void);
Item
stop_frontend_sound_voice(int32 voice_index_);
int32
attach_frontend_sound_sample(int32 voice_index_, Item sample_);
void
set_frontend_sound_pan(int32 voice_index_, int32 pan_);
