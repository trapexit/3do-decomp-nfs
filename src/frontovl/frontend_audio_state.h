#pragma once

#include "sdk_veneers.h"

typedef struct FrontendMusicState
{
  uint8 reserved_before_instrument[0x9C];
  Item  instrument;
} FrontendMusicState;

typedef struct SpeechStreamState
{
  uint8  reserved_before_stop_signal[0xD0];
  uint32 stop_signal;
  uint8  reserved_before_task[0x20];
  Item   task;
} SpeechStreamState;

extern int32               g_FRONTEND_MUSIC_BUFFER_BYTE_COUNT;
extern int32               g_FRONTEND_SPEECH_BUFFER_BYTE_COUNT;
extern Item                g_FRONTEND_OUTPUT_INSTRUMENT;
extern Item                g_FRONTEND_OUTPUT_TEMPLATE;
extern int32               g_FRONTEND_PLAYBACK_ENABLED;
extern int32               g_FRONTEND_AUDIO_RESOURCE_REFERENCE_COUNT;
extern FrontendMusicState *g_FRONTEND_MUSIC_STATE;
extern SpeechStreamState  *g_FRONTEND_SPEECH_STREAM;
