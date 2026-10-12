/* Frontovl movie-audio glue.
 *
 * Subsystem: Cinepak movie player (audio path).
 * The oracle drives movie audio through its own streaming-audio subscriber
 * (see streaming_audio_subscriber) but gates stereo presentation with a
 * single global flag defined here. This is custom audio glue rather than the
 * devkit's generic PlayCPAKStream audio handling. */

#include "movie_audio.h"

int32 g_STEREO_MOVIE_AUDIO_ENABLED = 0;

/* enable_stereo_movie_audio (orig 0x0000D5D8)
 * Turns the global stereo-movie-audio flag on. Movie-setup paths call this
 * when stereo presentation is wanted; the streaming-audio subscriber reads
 * g_STEREO_MOVIE_AUDIO_ENABLED while wiring playback. */

void
enable_stereo_movie_audio(void)
{
  g_STEREO_MOVIE_AUDIO_ENABLED = 1;
}

/* disable_stereo_movie_audio (orig 0x0000D5F8)
 * Clears the global stereo-movie-audio flag. Inverse of
 * enable_stereo_movie_audio; returns the player to mono movie audio. */

void
disable_stereo_movie_audio(void)
{
  g_STEREO_MOVIE_AUDIO_ENABLED = 0;
}
