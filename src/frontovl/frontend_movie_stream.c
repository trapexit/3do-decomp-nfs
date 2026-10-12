/* Frontovl movie-stream startup boundary. */

#include "frontend_movie_stream.h"

#include "control_subscriber.h"
#include "cpak_subscriber.h"
#include "data_acquisition_context_pool.h"
#include "data_stream_context_pool.h"
#include "frontend_movie_shutdown.h"
#include "runtime_string.h"
#include "scavenge_kernel_memory.h"
#include "streaming_audio_subscriber.h"

#define FRONTEND_MOVIE_PATH_PREFIX "Movies/"
#define FRONTEND_MOVIE_PATH_SUFFIX ".stream"
#define FRONTEND_MOVIE_PATH_BYTE_COUNT 40
#define FRONTEND_MOVIE_CONTEXT_COUNT 2

int32
initialize_frontend_movie_stream(void)
{
  int32 status;

  status = initialize_stream_request_pool(FRONTEND_MOVIE_CONTEXT_COUNT);
  if(status != 0)
    {
      return status;
    }
  status = initialize_data_streaming(FRONTEND_MOVIE_CONTEXT_COUNT);
  if(status != 0)
    {
      return status;
    }
  status = initialize_cpak_memory_pools();
  if(status != 0)
    {
      return status;
    }
  status = initialize_streaming_audio_subscriber();
  if(status != 0)
    {
      return status;
    }
  status = initialize_control_subscriber();

  return status;
}

int32
play_frontend_movie_request(const char *shape_name_,
                            int32       request_value_,
                            int32       reserved0_,
                            int32       reserved1_,
                            int32       reserved2_)
{
  char  movie_path[FRONTEND_MOVIE_PATH_BYTE_COUNT];
  int32 result;

  (void) reserved0_;
  (void) reserved1_;
  (void) reserved2_;
  initialize_frontend_movie_stream();
  copy_null_terminated_string(movie_path, FRONTEND_MOVIE_PATH_PREFIX);
  append_runtime_string(movie_path, shape_name_);
  append_runtime_string(movie_path, FRONTEND_MOVIE_PATH_SUFFIX);
  G_FRONTEND_MOVIE_REQUEST_VALUE = request_value_;
  result                         = start_frontend_movie_stream(movie_path);
  shutdown_frontend_movie_stream();

  return result;
}

int32
play_frontend_movie_stream(const char *shape_name_, MovieFrameCallback frame_callback_)
{
  char  movie_path[FRONTEND_MOVIE_PATH_BYTE_COUNT];
  int32 result;

  initialize_frontend_movie_stream();
  copy_null_terminated_string(movie_path, FRONTEND_MOVIE_PATH_PREFIX);
  append_runtime_string(movie_path, shape_name_);
  append_runtime_string(movie_path, FRONTEND_MOVIE_PATH_SUFFIX);
  scavenge_kernel_memory();
  set_movie_frame_callback(frame_callback_);
  result = play_movie_stream(movie_path, G_FRONTEND_MOVIE_STREAM_OPTION, NULL);
  set_movie_frame_callback(NULL);
  shutdown_frontend_movie_stream();

  return result;
}

int32
start_frontend_movie_stream(char *path_)
{
  scavenge_kernel_memory();
  return play_movie_stream(path_, G_FRONTEND_MOVIE_STREAM_OPTION, NULL);
}
