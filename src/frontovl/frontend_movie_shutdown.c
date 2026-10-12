/* Frontovl movie-stream shutdown lifecycle. */

#include "frontend_movie_shutdown.h"

#include "control_subscriber.h"
#include "cpak_subscriber.h"
#include "data_acquisition_context_pool.h"
#include "data_stream_context_pool.h"
#include "streaming_audio_subscriber.h"

int32
shutdown_frontend_movie_stream(void)
{
  int32 status;

  status = close_data_streaming();
  status = close_stream_request_pool();
  status = close_control_subscriber();
  status = destroy_cpak_memory_pools();
  status = close_streaming_audio_subscriber();

  return status;
}
