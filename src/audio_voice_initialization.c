#include "audio_voice.h"
#include "sound_effect_resources.h"

#include "filefunctions.h"
#include "operror.h"

#include "stdio.h"

#define DOLBY_SOUND_CHANNEL_COUNT 4
#define AUDIO_DIRECTORY_CHECK_BYTE_COUNT 256

int32
initialize_audio_voice_system_checked(void)
{
  char current_directory[AUDIO_DIRECTORY_CHECK_BYTE_COUNT];
  Err  result;

  result = GetDirectory(current_directory, AUDIO_DIRECTORY_CHECK_BYTE_COUNT);
  if(result < 0)
    PrintfSysErr(result);

  return initialize_audio_voice_system();
}

int32
initialize_audio_voice_mixer(void)
{
  int32 result;

  initialize_audio_voice_system();
  result = initialize_dolby_sound_pool(DOLBY_SOUND_CHANNEL_COUNT);
  if(result < 0)
    return fflush(stdout);

  return result;
}
