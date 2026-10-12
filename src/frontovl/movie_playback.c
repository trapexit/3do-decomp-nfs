#include "movie_playback.h"

#include "controller_input.h"
#include "data_stream_buffers.h"
#include "data_stream_context_pool.h"
#include "data_stream_requests.h"
#include "frontend_movie_stream.h"
#include "frontend_text_layout.h"
#include "movie_frame_buffer.h"
#include "page_aligned_io.h"
#include "runtime_error.h"
#include "runtime_io.h"
#include "runtime_memory.h"
#include "runtime_timing.h"
#include "stream_message_items.h"
#include "timer_io.h"

#include "graphics.h"
#include "kernel.h"
#include "sacontrolmsgs.h"

#define MOVIE_STREAM_BLOCK_BYTE_COUNT 0xC000
#define MOVIE_STREAM_SILENT_BUFFER_COUNT 5
#define MOVIE_STREAM_AUDIO_BUFFER_COUNT 10
#define MOVIE_STREAM_THREAD_PRIORITY 9
#define MOVIE_DATA_ACQUISITION_PRIORITY 8
#define MOVIE_SUBSCRIBER_MESSAGE_COUNT 300
#define MOVIE_ENABLED_AUDIO_CHANNEL_MASK 1
#define MOVIE_AUDIO_TEMPLATE_TAGS 0x02020201
#define MOVIE_FILM_SUBSCRIBER_PRIORITY 7
#define MOVIE_CONTROL_SUBSCRIBER_PRIORITY 11
#define MOVIE_AUDIO_SUBSCRIBER_PRIORITY 10
#define MOVIE_FILM_SUBSCRIBER_INDEX 0
#define MOVIE_CONTROL_SUBSCRIBER_INDEX 1
#define MOVIE_AUDIO_SUBSCRIBER_INDEX 2
#define MOVIE_AUDIO_TERMINATOR_INDEX 3
#define MOVIE_FILM_CHUNK_TYPE CHAR4LITERAL('F', 'I', 'L', 'M')
#define MOVIE_CONTROL_CHUNK_TYPE CHAR4LITERAL('C', 'T', 'R', 'L')
#define MOVIE_AUDIO_CHUNK_TYPE CHAR4LITERAL('S', 'N', 'D', 'S')
#define MOVIE_PLAYER_BUFFER_ERROR (-2002)
#define MOVIE_PLAYER_UNKNOWN_SUBSCRIBER_ERROR (-2003)
#define MOVIE_FIRST_OPTIONAL_AUDIO_CHANNEL 1
#define MOVIE_AUDIO_CHANNEL_COUNT 32
#define MOVIE_STREAM_SERVICE_STATE_MASK 0x0202
#define MOVIE_FRAME_SKIP_DIFFERENCE_OPTIONS 0x203
#define MOVIE_FRAME_DECODE_DIFFERENCE_OPTIONS 0x213
#define MOVIE_FRAME_ADVANCE_OPTIONS 1
#define MOVIE_FRAME_CALLBACK_OPTIONS 0x400
#define MOVIE_FRAME_BUFFER_PAGE_COUNT 0x4B
#define MOVIE_STREAM_START_OPTIONS 2
#define MOVIE_FRAME_BUFFER_READY_FLAG 0x200
#define MOVIE_FRAME_COMPLETION_MASK 3
#define MOVIE_PLAYBACK_OPTION_ENABLED 1
#define MOVIE_PLAYBACK_OPTION_DISABLED 0
#define MOVIE_PLAYBACK_OPTION_PADDING 0
#define MOVIE_VIDEO_CONFIGURATION_BYTE_COUNT 88
#define MOVIE_PREROLL_BUFFER_NUMERATOR 3
#define MOVIE_PREROLL_BUFFER_DENOMINATOR 4
#define MOVIE_PREROLL_WAIT_TICKS 60
#define MOVIE_STREAM_END_STATUS (-1000)
#define MOVIE_CANCEL_INPUT_MASK 0x0E000000
#define MOVIE_DEFAULT_X_POSITION (-1)
#define MOVIE_DEFAULT_Y_POSITION (-1)
#define MOVIE_INITIAL_STREAM_BLOCK_BYTE_COUNT 0x10000
#define MOVIE_INITIAL_STREAM_BUFFER_COUNT 5
#define MOVIE_FIRST_AUDIO_CHANNEL 1

typedef char MovieVideoConfigurationSizeCheck
  [sizeof(MovieVideoConfiguration) == MOVIE_VIDEO_CONFIGURATION_BYTE_COUNT ? 1 : -1];
typedef char MoviePlaybackRetainedStateSizeCheck[sizeof(MoviePlaybackRetainedState) == 36 ? 1 : -1];
typedef char
  MoviePlaybackRetainedDefaultsSizeCheck[sizeof(MoviePlaybackRetainedDefaults) == 12 ? 1 : -1];

typedef char
  MovieStreamOptionAlignmentSizeCheck[sizeof(g_MOVIE_STREAM_OPTION_ALIGNMENT) == 3 ? 1 : -1];
typedef char
  MovieVideoConfigurationStorageSizeCheck[sizeof(g_MOVIE_VIDEO_CONFIGURATION_STORAGE) == 88 ? 1 :
                                                                                              -1];
MoviePlaybackOptions g_MOVIE_PLAYBACK_OPTIONS =
  {
    MOVIE_PLAYBACK_OPTION_ENABLED,
    { MOVIE_PLAYBACK_OPTION_PADDING },
    MOVIE_PLAYBACK_OPTION_ENABLED,
    { MOVIE_PLAYBACK_OPTION_PADDING },
    MOVIE_PLAYBACK_OPTION_DISABLED,
    { MOVIE_PLAYBACK_OPTION_PADDING }
  };
MovieFrameCallback            g_MOVIE_FRAME_CALLBACK            = NULL;
int32                         g_MOVIE_STREAM_BUFFER_ADJUSTMENT   = 0;
MoviePlaybackRetainedState    g_MOVIE_PLAYBACK_RETAINED_STATE    = { { 0 } };
int32                         g_MOVIE_PLAYBACK_X_POSITION        = MOVIE_DEFAULT_X_POSITION;
int32                         g_MOVIE_PLAYBACK_Y_POSITION        = MOVIE_DEFAULT_Y_POSITION;
uint32 g_MOVIE_STREAM_BLOCK_SIZE = MOVIE_INITIAL_STREAM_BLOCK_BYTE_COUNT;
int32  g_MOVIE_STREAM_BUFFER_COUNT = MOVIE_INITIAL_STREAM_BUFFER_COUNT;
MoviePlaybackRetainedDefaults g_MOVIE_PLAYBACK_RETAINED_DEFAULTS =
  {
    -1,
    0,
    MOVIE_FRAME_BUFFER_COUNT
  };
int32                   g_MOVIE_AUDIO_CHANNEL_COUNT         = MOVIE_FRAME_BUFFER_COUNT;
uint8                   g_FRONTEND_MOVIE_STREAM_OPTION      = 1;
uint8                   g_MOVIE_STREAM_OPTION_ALIGNMENT[3]  = { 0, 0, 0 };
uint32                  g_MOVIE_STREAM_RESERVED_WORD        = 0;
int32                   g_FRONTEND_MOVIE_MODE               = 0;
int32                   g_FRONTEND_MOVIE_REQUEST_VALUE      = 0;
MovieVideoConfiguration g_MOVIE_VIDEO_CONFIGURATION_STORAGE =
  {
    0,
    0,
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { NULL },
    { 0 }
  };

void
set_movie_frame_callback(MovieFrameCallback callback_)
{
  G_MOVIE_FRAME_CALLBACK = callback_;
}

void
initialize_movie_stream_header(DSHeaderChunk *header_, boolean audio_enabled_)
{
  header_->headerVersion   = DS_STREAM_VERSION;
  header_->streamBlockSize = MOVIE_STREAM_BLOCK_BYTE_COUNT;
  if(audio_enabled_)
    {
      header_->streamBuffers = MOVIE_STREAM_AUDIO_BUFFER_COUNT;
    }
  else
    {
      header_->streamBuffers = MOVIE_STREAM_SILENT_BUFFER_COUNT;
    }
  header_->streamBuffers += G_MOVIE_STREAM_BUFFER_ADJUSTMENT;
  header_->streamerDeltaPri = MOVIE_STREAM_THREAD_PRIORITY;
  header_->dataAcqDeltaPri  = MOVIE_DATA_ACQUISITION_PRIORITY;
  header_->numSubsMsgs      = MOVIE_SUBSCRIBER_MESSAGE_COUNT;
  header_->audioClockChan   = 0;
  header_->enableAudioChan  = MOVIE_ENABLED_AUDIO_CHANNEL_MASK;
  if(audio_enabled_)
    {
      header_->preloadInstList[0] = MOVIE_AUDIO_TEMPLATE_TAGS;
      header_->preloadInstList[1] = 0;
    }
  else
    {
      header_->preloadInstList[0] = 0;
    }
  header_->subscriberList[MOVIE_FILM_SUBSCRIBER_INDEX].subscriberType = MOVIE_FILM_CHUNK_TYPE;
  header_->subscriberList[MOVIE_FILM_SUBSCRIBER_INDEX].deltaPriority =
    MOVIE_FILM_SUBSCRIBER_PRIORITY;
  header_->subscriberList[MOVIE_CONTROL_SUBSCRIBER_INDEX].subscriberType = MOVIE_CONTROL_CHUNK_TYPE;
  header_->subscriberList[MOVIE_CONTROL_SUBSCRIBER_INDEX].deltaPriority =
    MOVIE_CONTROL_SUBSCRIBER_PRIORITY;
  if(audio_enabled_)
    {
      header_->subscriberList[MOVIE_AUDIO_SUBSCRIBER_INDEX].subscriberType = MOVIE_AUDIO_CHUNK_TYPE;
      header_->subscriberList[MOVIE_AUDIO_SUBSCRIBER_INDEX].deltaPriority =
        MOVIE_AUDIO_SUBSCRIBER_PRIORITY;
      header_->subscriberList[MOVIE_AUDIO_TERMINATOR_INDEX].subscriberType = 0;
      header_->subscriberList[MOVIE_AUDIO_TERMINATOR_INDEX].deltaPriority  = 0;
    }
  else
    {
      header_->subscriberList[MOVIE_AUDIO_SUBSCRIBER_INDEX].subscriberType = 0;
      header_->subscriberList[MOVIE_AUDIO_SUBSCRIBER_INDEX].deltaPriority  = 0;
    }
}

int32
initialize_movie_stream_player(MovieStreamPlayer *player_, char *stream_path_)
{
  DSHeaderSubs  *subscriber;
  SAudioCtlBlock audio_control;
  boolean        has_audio;
  int32          subscriber_index;
  int32          channel_index;
  int32          frame_buffer_index;
  int32          status;

  has_audio                           = false;
  player_->buffer_list                = NULL;
  player_->stream                     = NULL;
  player_->acquisition_context        = NULL;
  player_->message_item               = 0;
  player_->end_of_stream_message_item = 0;
  player_->message_port               = 0;
  player_->control_context            = NULL;
  player_->audio_context              = NULL;
  player_->cpak_context               = NULL;
  player_->current_frame_buffer_index = 0;
  player_->next_frame_buffer_index    = 0;
  for(frame_buffer_index = 0; frame_buffer_index < MOVIE_FRAME_BUFFER_COUNT; frame_buffer_index++)
    {
      player_->cpak_records[frame_buffer_index] = NULL;
    }

  player_->timer_io_request = create_frontend_timer_io_request();
  if(player_->timer_io_request < 0)
    {
      return player_->timer_io_request;
    }
  player_->vram_io_request = GetVRAMIOReq();
  if(player_->vram_io_request < 0)
    {
      return player_->vram_io_request;
    }
  player_->buffer_list =
    create_data_stream_buffer_list(player_->header.streamBuffers, player_->header.streamBlockSize);
  if(player_->buffer_list == NULL)
    {
      return MOVIE_PLAYER_BUFFER_ERROR;
    }
  player_->message_port = create_message_port(NULL);
  if(player_->message_port < 0)
    {
      status = player_->message_port;
      goto cleanup;
    }
  player_->message_item = create_stream_message_item(player_->message_port);
  if(player_->message_item < 0)
    {
      status = player_->message_item;
      goto cleanup;
    }
  player_->end_of_stream_message_item = create_stream_message_item(player_->message_port);
  if(player_->end_of_stream_message_item < 0)
    {
      status = player_->end_of_stream_message_item;
      goto cleanup;
    }
  status = create_data_acquisition(&player_->acquisition_context,
                                   stream_path_,
                                   player_->header.dataAcqDeltaPri);
  if(status != 0)
    {
      goto cleanup;
    }
  status = create_data_stream(&player_->stream,
                              player_->buffer_list,
                              player_->header.streamBlockSize,
                              player_->header.streamerDeltaPri,
                              player_->header.numSubsMsgs);
  if(status != 0)
    {
      goto cleanup;
    }
  status = connect_data_stream(player_->message_item,
                               NULL,
                               (DSStreamCBPtr) player_->stream,
                               player_->acquisition_context->request_port);
  if(status != 0)
    {
      goto cleanup;
    }

  subscriber_index = 0;
  while(player_->header.subscriberList[subscriber_index].subscriberType != 0)
    {
      subscriber = &player_->header.subscriberList[subscriber_index];
      if(subscriber->subscriberType == MOVIE_CONTROL_CHUNK_TYPE)
        {
          status = create_control_subscriber(&player_->control_context,
                                             (DSStreamCBPtr) player_->stream,
                                             subscriber->deltaPriority);
          if(status == 0)
            {
              status = subscribe_to_data_stream(player_->message_item,
                                                NULL,
                                                (DSStreamCBPtr) player_->stream,
                                                MOVIE_CONTROL_CHUNK_TYPE,
                                                player_->control_context->standard.requestPort);
            }
        }
      else if(subscriber->subscriberType == MOVIE_FILM_CHUNK_TYPE)
        {
          status = create_cpak_subscriber(&player_->cpak_context,
                                          MOVIE_FRAME_BUFFER_COUNT,
                                          subscriber->deltaPriority);
          if(status == 0)
            {
              status = subscribe_to_data_stream(player_->message_item,
                                                NULL,
                                                (DSStreamCBPtr) player_->stream,
                                                MOVIE_FILM_CHUNK_TYPE,
                                                player_->cpak_context->request_port);
            }
          for(frame_buffer_index = 0;
              (status == 0) && (frame_buffer_index < MOVIE_FRAME_BUFFER_COUNT);
              frame_buffer_index++)
            {
              status = initialize_cpak_record((DSStreamCBPtr) player_->stream,
                                              player_->cpak_context,
                                              &player_->cpak_records[frame_buffer_index],
                                              frame_buffer_index,
                                              true);
            }
          player_->current_frame_buffer_index = 0;
        }
      else if(subscriber->subscriberType == MOVIE_AUDIO_CHUNK_TYPE)
        {
          status = create_streaming_audio_subscriber(&player_->audio_context,
                                                     (DSStreamCBPtr) player_->stream,
                                                     subscriber->deltaPriority);
          if(status == 0)
            {
              status = subscribe_to_data_stream(player_->message_item,
                                                NULL,
                                                (DSStreamCBPtr) player_->stream,
                                                MOVIE_AUDIO_CHUNK_TYPE,
                                                player_->audio_context->request_port);
            }
          has_audio = true;
        }
      else
        {
          status = MOVIE_PLAYER_UNKNOWN_SUBSCRIBER_ERROR;
        }
      if(status != 0)
        {
          goto cleanup;
        }
      subscriber_index++;
    }

  if(has_audio)
    {
      audio_control.loadTemplates.tagListPtr = player_->header.preloadInstList;
      status                                 = control_data_stream(player_->message_item,
                                                                   NULL,
                                                                   (DSStreamCBPtr) player_->stream,
                                                                   MOVIE_AUDIO_CHUNK_TYPE,
                                                                   kSAudioCtlOpLoadTemplates,
                                                                   &audio_control);
      for(channel_index = MOVIE_FIRST_OPTIONAL_AUDIO_CHANNEL;
          (status == 0) && (channel_index < MOVIE_AUDIO_CHANNEL_COUNT);
          channel_index++)
        {
          if((player_->header.enableAudioChan & (1L << channel_index)) != 0)
            {
              status = set_data_stream_channel(player_->message_item,
                                               NULL,
                                               (DSStreamCBPtr) player_->stream,
                                               MOVIE_AUDIO_CHUNK_TYPE,
                                               channel_index,
                                               CHAN_ENABLED);
            }
        }
      if(status == 0)
        {
          audio_control.clock.channelNumber = player_->header.audioClockChan;
          status                            = control_data_stream(player_->message_item,
                                                                  NULL,
                                                                  (DSStreamCBPtr) player_->stream,
                                                                  MOVIE_AUDIO_CHUNK_TYPE,
                                                                  kSAudioCtlOpSetClockChan,
                                                                  &audio_control);
        }
    }
  if(status == 0)
    {
      return 0;
    }

cleanup:
  dispose_movie_stream_player(player_);
  return status;
}

int32
dispose_movie_stream_player(MovieStreamPlayer *player_)
{
  int32 frame_buffer_index;
  int32 status;

  if(player_->stream != NULL)
    {
      dispose_data_stream(player_->message_item, player_->stream);
    }
  if(player_->acquisition_context != NULL)
    {
      dispose_data_acquisition(player_->acquisition_context);
    }
  if(player_->control_context != NULL)
    {
      dispose_control_subscriber(player_->control_context);
    }
  if(player_->cpak_records[0] != NULL)
    {
      for(frame_buffer_index = 0; frame_buffer_index < MOVIE_FRAME_BUFFER_COUNT;
          frame_buffer_index++)
        {
          destroy_cpak_record(player_->cpak_context,
                              player_->cpak_records[frame_buffer_index],
                              player_->cpak_records[frame_buffer_index]->channel);
        }
    }
  if(player_->cpak_context != NULL)
    {
      dispose_cpak_subscriber(player_->cpak_context);
    }
  if(player_->audio_context != NULL)
    {
      dispose_streaming_audio_subscriber(player_->audio_context);
    }
  if(player_->buffer_list != NULL)
    {
      dispose_data_stream_buffer_list(player_->buffer_list);
    }
  if(player_->message_item != 0)
    {
      remove_stream_message_item(player_->message_item);
    }
  if(player_->end_of_stream_message_item != 0)
    {
      remove_stream_message_item(player_->end_of_stream_message_item);
    }
  if(player_->message_port != 0)
    {
      remove_message_port(player_->message_port);
    }
  if(player_->timer_io_request != 0)
    {
      DeleteItem(player_->timer_io_request);
    }
  status = 0;
  if(player_->vram_io_request != 0)
    {
      status = player_->vram_io_request;
      DeleteItem(player_->vram_io_request);
    }

  return status;
}

int32
service_movie_playback(MovieStreamPlayer *player_, MoviePlaybackCallbackContext *context_)
{
  uint32 controller_input;

  if(!context_->clock_initialized)
    {
      set_data_stream_clock(player_->stream, 0);
      context_->clock_initialized   = true;
      context_->clock_reset_pending = false;
    }
  if((player_->stream->flags & MOVIE_STREAM_SERVICE_STATE_MASK) != 0)
    {
      if(context_->stream_state_pending)
        {
          context_->stream_state_pending = false;
        }
      if(context_->secondary_preroll_pending)
        {
          preroll_data_stream(context_->secondary_player->message_item,
                              NULL,
                              (DSStreamCBPtr) context_->secondary_player->stream);
        }
    }
  controller_input = read_stable_controller_input();
  if(controller_input != 0)
    {
      return 1;
    }

  return 0;
}
uint32
play_movie_stream_player(MovieStreamPlayer            *player_,
                         void                         *video_configuration_,
                         int32                         x_position_,
                         int32                         y_position_,
                         MoviePlayerServiceCallback    service_callback_,
                         MoviePlaybackCallbackContext *service_context_)
{
  MovieVideoConfiguration *video;
  MovieImage               output_image;
  DSRequestMsg             end_of_stream_request;
  CpakRecordState         *record;
  Bitmap                  *current_bitmap;
  Bitmap                  *next_bitmap;
  uint32                   process_options;
  uint32                   player_result;
  uint32                   frame_time;
  int32                    frame_buffer_index;
  int32                    completed_frame_count;
  int32                    last_completed_frame_count;
  int32                    frame_changed;
  int32                    status;

  video  = (MovieVideoConfiguration *) video_configuration_;
  record = player_->cpak_records[player_->current_frame_buffer_index];
  if(record->header.compression_type != 0)
    {
      record->header.compression_type = 0;
    }
  player_->video_configuration = video_configuration_;
  player_->service_callback    = service_callback_;
  player_->service_context     = service_context_;
  if(G_MOVIE_FRAME_CALLBACK != NULL)
    {
      synchronize_runtime_frame();
    }
  status                       = start_data_stream(player_->message_item,
                                                   NULL,
                                                   (DSStreamCBPtr) player_->stream,
                                                   MOVIE_STREAM_START_OPTIONS);
  if(status != 0)
    {
      return status;
    }
  status = wait_for_data_stream_end(player_->end_of_stream_message_item,
                                    &end_of_stream_request,
                                    (DSStreamCBPtr) player_->stream);
  if(status != 0)
    {
      return status;
    }
  if(G_MOVIE_FRAME_CALLBACK == NULL)
    {
      DisplayScreen(video->screen_items[video->current_screen], 0);
      next_bitmap    = video->bitmaps[(video->current_screen + 1) & 1];
      current_bitmap = video->bitmaps[video->current_screen];
    }
  else
    {
      next_bitmap    = NULL;
      current_bitmap = NULL;
    }
  configure_movie_frame_placement(x_position_,
                                  y_position_,
                                  (x_position_ == -1) && (y_position_ == -1));

  output_image.base_address       = NULL;
  output_image.row_byte_count     = 0;
  output_image.frame_base_address = NULL;
  output_image.width              = 0;
  output_image.height             = 0;
  output_image.x_position         = 0;
  output_image.y_position         = 0;
  player_result                   = 0;
  last_completed_frame_count      = 0;
  completed_frame_count           = 0;
  for(;;)
    {
      if(player_->service_callback != NULL)
        {
          player_result = player_->service_callback(player_, player_->service_context);
        }
      if((player_result != 0) ||
         poll_for_stream_message(player_->message_port, NULL, NULL, NULL, &status))
        {
          stop_data_stream(player_->message_item, NULL, (DSStreamCBPtr) player_->stream, 1);
          if(player_->cpak_context != NULL)
            {
              flush_cpak_channel(player_->cpak_context,
                                 player_->cpak_records[player_->current_frame_buffer_index],
                                 0);
            }
          if(G_MOVIE_FRAME_CALLBACK != NULL)
            {
              delete_movie_video_window();
            }
          return player_result;
        }

      frame_changed = 0;
      if(player_->cpak_context != NULL)
        {
          if(last_completed_frame_count == completed_frame_count)
            {
              status = wait_for_io_completion(player_->vram_io_request);
              if(status < 0)
                {
                  runtime_debug_printf("WaitIO error");
                  print_runtime_error(status);
                }
            }
          for(frame_buffer_index = 0; frame_buffer_index < MOVIE_FRAME_BUFFER_COUNT;
              frame_buffer_index++)
            {
              record = player_->cpak_records[frame_buffer_index];
              if((frame_buffer_index == player_->next_frame_buffer_index) &&
                 (player_->next_frame_buffer_index != player_->current_frame_buffer_index))
                {
                  if(G_MOVIE_FRAME_CALLBACK == NULL)
                    {
                      status = process_cpak_frames(player_->cpak_context,
                                                   record,
                                                   MOVIE_FRAME_ADVANCE_OPTIONS,
                                                   -1,
                                                   &frame_time,
                                                   output_image.frame_base_address,
                                                   output_image.row_byte_count);
                    }
                  else
                    {
                      status = process_cpak_frames(player_->cpak_context,
                                                   record,
                                                   MOVIE_FRAME_ADVANCE_OPTIONS |
                                                     MOVIE_FRAME_CALLBACK_OPTIONS,
                                                   -1,
                                                   &frame_time,
                                                   output_image.frame_base_address,
                                                   output_image.row_byte_count);
                      G_MOVIE_FRAME_CALLBACK(g_CPAK_FRAME_RESULT);
                    }
                  if(status < 0)
                    {
                      break;
                    }
                  if((status & MOVIE_FRAME_ADVANCE_OPTIONS) != 0)
                    {
                      player_->current_frame_buffer_index = player_->next_frame_buffer_index;
                      completed_frame_count++;
                    }
                }
              else if(frame_buffer_index == player_->current_frame_buffer_index)
                {
                  do
                    {
                      process_options = MOVIE_FRAME_SKIP_DIFFERENCE_OPTIONS;
                      if(G_MOVIE_PLAYBACK_OPTIONS.decode_difference_frames)
                        {
                          process_options = MOVIE_FRAME_DECODE_DIFFERENCE_OPTIONS;
                        }
                      if(G_MOVIE_FRAME_CALLBACK == NULL)
                        {
                          status = process_cpak_frames(player_->cpak_context,
                                                       record,
                                                       process_options,
                                                       -1,
                                                       &frame_time,
                                                       output_image.frame_base_address,
                                                       output_image.row_byte_count);
                        }
                      else
                        {
                          if(!is_cpak_callback_frame_due(record))
                            {
                              status = 0;
                            }
                          else
                            {
                              status =
                                process_cpak_frames(player_->cpak_context,
                                                    record,
                                                    process_options |
                                                      MOVIE_FRAME_CALLBACK_OPTIONS,
                                                    -1,
                                                    &frame_time,
                                                    output_image.frame_base_address,
                                                    output_image.row_byte_count);
                              G_MOVIE_FRAME_CALLBACK(g_CPAK_FRAME_RESULT);
                            }
                        }
                      if(status < 0)
                        {
                          break;
                        }
                      if((status & MOVIE_FRAME_BUFFER_READY_FLAG) != 0)
                        {
                          output_image.width               = record->header.width;
                          output_image.height              = record->header.height;
                          player_->next_frame_buffer_index = frame_buffer_index;
                          if(G_MOVIE_FRAME_CALLBACK == NULL)
                            {
                              position_movie_frame_buffer(&output_image, next_bitmap);
                            }
                        }
                    }
                  while((status != 0) && ((status & MOVIE_FRAME_COMPLETION_MASK) == 0));
                  if((status & MOVIE_FRAME_COMPLETION_MASK) != 0)
                    {
                      completed_frame_count++;
                      frame_changed = 1;
                    }
                }
              else
                {
                  if(G_MOVIE_FRAME_CALLBACK == NULL)
                    {
                      status = process_cpak_frames(player_->cpak_context,
                                                   record,
                                                   0,
                                                   -1,
                                                   &frame_time,
                                                   NULL,
                                                   0);
                    }
                  else
                    {
                      status = process_cpak_frames(player_->cpak_context,
                                                   record,
                                                   MOVIE_FRAME_CALLBACK_OPTIONS,
                                                   -1,
                                                   &frame_time,
                                                   NULL,
                                                   0);
                    }
                }
            }
          record = player_->cpak_records[player_->current_frame_buffer_index];
          if((last_completed_frame_count != completed_frame_count) &&
             is_cpak_current_frame_due(record))
            {
              if(G_MOVIE_FRAME_CALLBACK == NULL)
                {
                  send_page_aligned_io(player_->vram_io_request,
                                       current_bitmap->bm_Buffer,
                                       next_bitmap->bm_Buffer,
                                       MOVIE_FRAME_BUFFER_PAGE_COUNT,
                                       -1);
                }
              frame_changed              = 1;
              last_completed_frame_count = completed_frame_count;
            }
          signal_cpak_free_messages(player_->cpak_context);
        }
      if(!frame_changed)
        {
          if(G_MOVIE_PLAYBACK_OPTIONS.use_timer_wait)
            {
              wait_runtime_timer_ticks(player_->timer_io_request, 1);
            }
          else
            {
              Yield();
            }
        }
    }
}

boolean
play_movie_stream(char *stream_path_, uint8 audio_enabled_, char *secondary_stream_path_)
{
  MoviePlaybackCallbackContext callback_context;
  MovieStreamPlayer            primary_player;
  MovieStreamPlayer            secondary_player;
  SAudioCtlBlock               audio_control;
  uint8                        video_configuration[MOVIE_VIDEO_CONFIGURATION_BYTE_COUNT];
  void                        *video_configuration_pointer;
  boolean                      saved_primary_playback_enabled;
  boolean                      has_secondary_stream;
  boolean                      cancelled;
  boolean                      failed;
  int32                        channel_status;
  int32                        channel_index;
  int32                        filled_buffer_count;
  int32                        required_buffer_count;
  int32                        play_result;
  int32                        status;

  if(G_MOVIE_FRAME_CALLBACK == NULL)
    {
      get_runtime_video_configuration(video_configuration);
      video_configuration_pointer = video_configuration;
    }
  else
    {
      video_configuration_pointer = NULL;
    }

  initialize_movie_stream_header(&primary_player.header, audio_enabled_);
  primary_player.header.streamBlockSize = G_MOVIE_STREAM_BLOCK_SIZE;
  primary_player.header.streamBuffers   = G_MOVIE_STREAM_BUFFER_COUNT;
  secondary_player                      = primary_player;
  status = initialize_movie_stream_player(&primary_player, stream_path_);
  if(status < 0)
    {
      return false;
    }
  has_secondary_stream = secondary_stream_path_ != NULL;
  if(has_secondary_stream)
    {
      status = initialize_movie_stream_player(&secondary_player, secondary_stream_path_);
      if(status < 0)
        {
          return false;
        }
    }

  preroll_data_stream(primary_player.message_item, NULL, (DSStreamCBPtr) primary_player.stream);
  callback_context.clock_reset_pending       = false;
  callback_context.clock_initialized         = false;
  callback_context.stream_state_pending      = false;
  callback_context.secondary_preroll_pending = has_secondary_stream;
  callback_context.secondary_player          = &secondary_player;
  cancelled                                  = false;
  required_buffer_count = (primary_player.header.streamBuffers * MOVIE_PREROLL_BUFFER_NUMERATOR) /
                          MOVIE_PREROLL_BUFFER_DENOMINATOR;
  filled_buffer_count   = get_filled_data_stream_buffer_count(primary_player.stream);
  while((filled_buffer_count >= 0) && (filled_buffer_count < required_buffer_count))
    {
      if((read_stable_controller_input() & MOVIE_CANCEL_INPUT_MASK) != 0)
        {
          cancelled = true;
          break;
        }
      filled_buffer_count = get_filled_data_stream_buffer_count(primary_player.stream);
      if(filled_buffer_count == MOVIE_STREAM_END_STATUS)
        {
          cancelled = true;
          break;
        }
      wait_runtime_timer_ticks(primary_player.timer_io_request, MOVIE_PREROLL_WAIT_TICKS);
    }

  play_result                    = status;
  failed                         = cancelled;
  saved_primary_playback_enabled = G_MOVIE_PLAYBACK_OPTIONS.primary_playback_enabled;
  if(has_secondary_stream)
    {
      G_MOVIE_PLAYBACK_OPTIONS.primary_playback_enabled = false;
    }
  if(!cancelled)
    {
      for(channel_index = MOVIE_FIRST_AUDIO_CHANNEL; channel_index < G_MOVIE_AUDIO_CHANNEL_COUNT;
          channel_index++)
        {
          status = get_data_stream_channel(primary_player.message_item,
                                           NULL,
                                           (DSStreamCBPtr) primary_player.stream,
                                           MOVIE_AUDIO_CHUNK_TYPE,
                                           channel_index,
                                           &channel_status);
          if(status < 0)
            {
              break;
            }
          channel_status |= CHAN_ENABLED;
          status = set_data_stream_channel(primary_player.message_item,
                                           NULL,
                                           (DSStreamCBPtr) primary_player.stream,
                                           MOVIE_AUDIO_CHUNK_TYPE,
                                           channel_index,
                                           channel_status);
          if(status < 0)
            {
              break;
            }
          status = get_data_stream_channel(primary_player.message_item,
                                           NULL,
                                           (DSStreamCBPtr) primary_player.stream,
                                           MOVIE_AUDIO_CHUNK_TYPE,
                                           channel_index,
                                           &channel_status);
          if(status < 0)
            {
              break;
            }
          audio_control.mute.channelNumber = channel_index;
          status = control_data_stream(primary_player.message_item,
                                       NULL,
                                       (DSStreamCBPtr) primary_player.stream,
                                       MOVIE_AUDIO_CHUNK_TYPE,
                                       kSAudioCtlOpMute,
                                       &audio_control);
          if(status < 0)
            {
              break;
            }
        }

      if(G_FRONTEND_MOVIE_MODE != 0)
        {
          begin_runtime_frame();
          clear_runtime_display(0);
          finish_runtime_frame();
          present_runtime_frame();
          synchronize_runtime_frame();
          begin_runtime_frame();
          clear_runtime_display(0);
          finish_runtime_frame();
          present_runtime_frame();
          set_runtime_grayscale_palette();
        }
      play_result = play_movie_stream_player(&primary_player,
                                             video_configuration_pointer,
                                             G_MOVIE_PLAYBACK_X_POSITION,
                                             G_MOVIE_PLAYBACK_Y_POSITION,
                                             service_movie_playback,
                                             &callback_context);
      if(play_result != 0)
        {
          failed = true;
        }
      if(has_secondary_stream)
        {
          play_result = play_movie_stream_player(&secondary_player,
                                                 video_configuration_pointer,
                                                 G_MOVIE_PLAYBACK_X_POSITION,
                                                 G_MOVIE_PLAYBACK_Y_POSITION,
                                                 NULL,
                                                 NULL);
        }
    }
  G_MOVIE_PLAYBACK_OPTIONS.primary_playback_enabled = saved_primary_playback_enabled;
  dispose_movie_stream_player(&primary_player);
  if(has_secondary_stream)
    {
      dispose_movie_stream_player(&secondary_player);
    }
  if(play_result < 0)
    {
      failed = true;
    }

  return !failed;
}
