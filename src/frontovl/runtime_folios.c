#include "runtime_folios.h"

#include "audio_stream_resources.h"
#include "frontend_audio_control.h"
#include "frontend_sound.h"
#include "sdk_veneers.h"

#include "audio.h"
#include "graphics.h"
#include "operamath.h"

void
initialize_runtime_folios(void)
{
  (void) OpenGraphicsFolio();
  (void) OpenMathFolio();
  (void) OpenAudioFolio();
}

void
initialize_frontend_audio(void)
{
  initialize_frontend_audio_resources();
  load_frontend_sound_bank();
  initialize_frontend_audio_stream();
}

void
start_frontend_music(const char *path_)
{
  initialize_frontend_audio_stream();
  configure_frontend_audio_stream(path_);
  resume_frontend_audio_stream();
}

void
shutdown_frontend_audio(void)
{
  stop_frontend_audio_stream();
  release_frontend_audio_resources();
  unload_frontend_sound_bank();
}

void
initialize_speech_stream(void)
{
  initialize_speech_stream_state();
}

void
stop_speech_stream(void)
{
  stop_speech_stream_state();
}
