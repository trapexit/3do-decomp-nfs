#include "audio_mode_names.h"

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

const char  G_AUDIO_MODE_NAMES[AUDIO_MODE_NAME_COUNT][AUDIO_MODE_NAME_SIZE] = { "none",
                                                                                "musc",
                                                                                "sfx ",
                                                                                "all " };
const char *g_AUDIO_MODE_NAME_POINTERS[AUDIO_MODE_NAME_COUNT] = { G_AUDIO_MODE_NAMES[0],
                                                                  G_AUDIO_MODE_NAMES[1],
                                                                  G_AUDIO_MODE_NAMES[2],
                                                                  G_AUDIO_MODE_NAMES[3] };

COMPILE_TIME_SIZE_ASSERT(audio_mode_names_size, sizeof(G_AUDIO_MODE_NAMES) == 32);
COMPILE_TIME_SIZE_ASSERT(audio_mode_name_pointer_size, sizeof(g_AUDIO_MODE_NAME_POINTERS) == 16);
