#include "game_audio_shutdown.h"

#include "audio_voice.h"
#include "sound_effect_resources.h"

#include "mem.h"

void
shutdown_game_audio_resources(void)
{
  shutdown_audio_voices();
  shutdown_sound_effect_resources();
  ScavengeMem();
}
