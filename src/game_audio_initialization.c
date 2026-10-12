#include "game_audio_initialization.h"

#include "audio_voice.h"
#include "sound_effect_state.h"

void
initialize_game_audio_resources(void)
{
  load_vehicle_sound_samples();
  initialize_audio_voice_instruments();
  enable_stereo_panning();
}
