#include "frontend_movies.h"

#include "directory_veneers.h"
#include "frontend_audio_control.h"
#include "frontend_frame.h"
#include "post_race_legacy.h"
#include "runtime_memory.h"
#include "runtime_string.h"
#include "sdk_veneers.h"
#include "tracked_memory.h"

#define FRONTEND_MOVIE_DIRECTORY "Movies"
#define FRONTEND_MOVIE_LIMIT 200
#define FRONTEND_MOVIE_NAME_BYTE_COUNT 40
#define FRONTEND_MOVIE_SUFFIX_BYTE_COUNT 7
#define FRONTEND_MOVIE_POINTER_BYTES 800
#define FRONTEND_MOVIE_NAME_BYTES 8000
#define FRONTEND_HOST_CODE_LIMIT 200
#define FRONTEND_OVERLAY_TEXT_BYTE_COUNT 20
#define FRONTEND_OVERLAY_TEXT_X 47
#define FRONTEND_OVERLAY_TEXT_A_Y 46
#define FRONTEND_OVERLAY_TEXT_B_Y 74
#define FRONTEND_OVERLAY_TEXT_C_Y 150
#define FRONTEND_OVERLAY_TEXT_D_Y 191

char        *g_FRONTEND_MOVIE_NAMES;
const char **g_FRONTEND_MOVIE_NAME_POINTERS;

char g_FRONTEND_OVERLAY_TEXT_A[FRONTEND_OVERLAY_TEXT_BYTE_COUNT];
char g_FRONTEND_OVERLAY_TEXT_B[FRONTEND_OVERLAY_TEXT_BYTE_COUNT];
char g_FRONTEND_OVERLAY_TEXT_D[FRONTEND_OVERLAY_TEXT_BYTE_COUNT];
char g_FRONTEND_OVERLAY_TEXT_C[FRONTEND_OVERLAY_TEXT_BYTE_COUNT];

int32
enumerate_frontend_movies(void)
{
  DirectoryEntry entry;
  Directory     *directory;
  int32          movie_count;
  int32          movie_name_offset;
  int32          name_length;

  directory   = open_runtime_directory(FRONTEND_MOVIE_DIRECTORY);
  movie_count = 0;
  while(read_runtime_directory(directory, &entry) >= 0)
    {
      movie_name_offset = (movie_count * FRONTEND_MOVIE_NAME_BYTE_COUNT);
      copy_null_terminated_string((g_FRONTEND_MOVIE_NAMES + movie_name_offset), entry.de_FileName);
      name_length = runtime_string_length((g_FRONTEND_MOVIE_NAMES + movie_name_offset));
      g_FRONTEND_MOVIE_NAMES[(movie_name_offset +
                              (name_length - FRONTEND_MOVIE_SUFFIX_BYTE_COUNT))] = 0;
      g_FRONTEND_MOVIE_NAME_POINTERS[movie_count] =
        (g_FRONTEND_MOVIE_NAMES + movie_name_offset);
      ++movie_count;
      if(movie_count >= FRONTEND_MOVIE_LIMIT)
        {
          break;
        }
    }
  CLOSE_DIRECTORY_VENEER(directory);
  return movie_count;
}

void
draw_frontend_movie_overlay_text(void)
{
  draw_runtime_text(g_FRONTEND_OVERLAY_TEXT_A, FRONTEND_OVERLAY_TEXT_X, FRONTEND_OVERLAY_TEXT_A_Y);
  draw_runtime_text(g_FRONTEND_OVERLAY_TEXT_B, FRONTEND_OVERLAY_TEXT_X, FRONTEND_OVERLAY_TEXT_B_Y);
  draw_runtime_text(g_FRONTEND_OVERLAY_TEXT_C, FRONTEND_OVERLAY_TEXT_X, FRONTEND_OVERLAY_TEXT_C_Y);
  draw_runtime_text(g_FRONTEND_OVERLAY_TEXT_D, FRONTEND_OVERLAY_TEXT_X, FRONTEND_OVERLAY_TEXT_D_Y);
}

void
play_frontend_movie_sequence(void)
{
  int32 movie_count;
  int32 movie_index;
  int32 host_code;
  int32 display_result;

  g_FRONTEND_OVERLAY_TEXT_A[0] = 0;
  g_FRONTEND_OVERLAY_TEXT_B[0] = 0;
  g_FRONTEND_OVERLAY_TEXT_C[0] = 0;
  g_FRONTEND_OVERLAY_TEXT_D[0] = 0;

  g_FRONTEND_MOVIE_NAME_POINTERS = allocate_primary_memory(FRONTEND_MOVIE_POINTER_BYTES);
  g_FRONTEND_MOVIE_NAMES         = allocate_primary_memory(FRONTEND_MOVIE_NAME_BYTES);
  movie_count                    = enumerate_frontend_movies();

  movie_index = 0;
  while(movie_index < movie_count)
    {
      host_code = runtime_string_to_integer(g_FRONTEND_MOVIE_NAME_POINTERS[movie_index]);
      if(host_code > 0 && host_code < FRONTEND_HOST_CODE_LIMIT)
        {
          display_result = present_frontend_host(g_FRONTEND_MOVIE_NAME_POINTERS[movie_index],
                                                 present_frontend_host_frame,
                                                 "frontend/display/hosts/hosti",
                                                 "bgnd",
                                                 g_FRONTEND_SCENE_AUDIO_ACTIVE,
                                                 0);
        }
      else
        {
          display_result = present_frontend_scene(1,
                                                  &g_FRONTEND_MOVIE_NAME_POINTERS[movie_index],
                                                  0,
                                                  NULL,
                                                  NULL,
                                                  g_FRONTEND_SCENE_AUDIO_ACTIVE,
                                                  1,
                                                  0);
        }
      if(display_result == 0)
        {
          break;
        }
      ++movie_index;
    }

  free_primary_memory(g_FRONTEND_MOVIE_NAME_POINTERS);
  free_primary_memory(g_FRONTEND_MOVIE_NAMES);
}
