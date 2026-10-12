#include "game_audio_system.h"

#include "audio_voice.h"
#include "game_audio_initialization.h"
#include "game_audio_shutdown.h"
#include "sound_effect_resources.h"
#include "sound_effect_slots.h"
#include "sound_effect_state.h"

#include "audio.h"
#include "mem.h"

#define GAME_AUDIO_VOICE_COUNT 8

void
stop_all_game_audio_channels(void)
{
  int32 channel_index;
  int32 voice_index;

  for(voice_index = 0; voice_index < GAME_AUDIO_VOICE_COUNT; voice_index++)
    ramp_down_audio_voice_amplitude(voice_index);

  for(channel_index = 0; channel_index < TERTIARY_SOUND_EFFECT_SLOT_COUNT; channel_index++)
    {
      (void) stop_dolby_sound_channel(channel_index);
      g_TERTIARY_SOUND_EFFECT_SLOTS[channel_index].active_voice = 0;
    }
}

void
initialize_game_audio_system(void)
{
  ScavengeMem();
  (void) OpenAudioFolio();
  ScavengeMem();
  initialize_sound_effect_resources();
  ScavengeMem();
  initialize_audio_voice_mixer();
  ScavengeMem();
  initialize_game_audio_resources();
  ScavengeMem();
}

void
shutdown_game_audio_system(void)
{
  shutdown_game_audio_resources();
  unload_vehicle_sound_samples();
  CloseAudioFolio();
  ScavengeMem();
}
