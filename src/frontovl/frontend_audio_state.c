#include "frontend_audio_state.h"

#include "frontend_sound.h"

int32               g_FRONTEND_MUSIC_BUFFER_BYTE_COUNT  = 0x56220;
int32               g_FRONTEND_SPEECH_BUFFER_BYTE_COUNT = 0x2B110;
Item                g_FRONTEND_OUTPUT_INSTRUMENT;
Item                g_FRONTEND_OUTPUT_TEMPLATE;
FrontendSoundVoice  g_FRONTEND_SOUND_VOICES[3];
int32               g_FRONTEND_PLAYBACK_ENABLED;
int32               g_FRONTEND_AUDIO_RESOURCE_REFERENCE_COUNT;
FrontendMusicState *g_FRONTEND_MUSIC_STATE;
SpeechStreamState  *g_FRONTEND_SPEECH_STREAM;

typedef char FrontendSoundVoicesSizeCheck[sizeof(g_FRONTEND_SOUND_VOICES) == 84 ? 1 : -1];
