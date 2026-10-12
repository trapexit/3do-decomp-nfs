#pragma once

#include "control_subscriber.h"
#include "cpak_subscriber.h"
#include "data_acquisition_state.h"
#include "data_stream_state.h"
#include "streaming_audio_subscriber.h"

#include "dataacq.h"
#include "datastream.h"
#include "dsstreamheader.h"
#include "graphics.h"
#include "subscriberutils.h"

#define MOVIE_FRAME_BUFFER_COUNT 4
#define MOVIE_STREAM_PLAYER_BYTE_COUNT 0x144

struct MoviePlaybackCallbackContext;
struct MovieStreamPlayer;

typedef void  (*MovieFrameCallback)(int32 display_mode_);
typedef int32 (*MoviePlayerServiceCallback)(struct MovieStreamPlayer            *player_,
                                            struct MoviePlaybackCallbackContext *context_);

typedef struct MoviePlaybackOptions
{
  boolean primary_playback_enabled;
  uint8   primary_playback_padding[3];
  boolean decode_difference_frames;
  uint8   decode_difference_padding[3];
  boolean use_timer_wait;
  uint8   timer_wait_padding[3];
} MoviePlaybackOptions;

typedef struct MoviePlaybackRetainedState
{
  uint32 words[9];
} MoviePlaybackRetainedState;

typedef struct MoviePlaybackRetainedDefaults
{
  int32 sentinel;
  int32 disabled;
  int32 frame_buffer_count;
} MoviePlaybackRetainedDefaults;

typedef struct MovieVideoConfiguration
{
  int32   screen_count;
  int32   current_screen;
  uint8   reserved_before_screens[8];
  Item    screen_items[MOVIE_FRAME_BUFFER_COUNT];
  Item    bitmap_items[MOVIE_FRAME_BUFFER_COUNT];
  uint8   reserved_before_bitmaps[16];
  Bitmap *bitmaps[MOVIE_FRAME_BUFFER_COUNT];
  uint8   reserved_tail[8];
} MovieVideoConfiguration;

extern MoviePlaybackOptions          g_MOVIE_PLAYBACK_OPTIONS;
extern MovieFrameCallback            g_MOVIE_FRAME_CALLBACK;
extern int32                         g_MOVIE_STREAM_BUFFER_ADJUSTMENT;
extern MoviePlaybackRetainedState    g_MOVIE_PLAYBACK_RETAINED_STATE;
extern int32                         g_MOVIE_PLAYBACK_X_POSITION;
extern int32                         g_MOVIE_PLAYBACK_Y_POSITION;
extern uint32                        g_MOVIE_STREAM_BLOCK_SIZE;
extern int32                         g_MOVIE_STREAM_BUFFER_COUNT;
extern MoviePlaybackRetainedDefaults g_MOVIE_PLAYBACK_RETAINED_DEFAULTS;
extern int32                         g_MOVIE_AUDIO_CHANNEL_COUNT;
extern uint8                         g_FRONTEND_MOVIE_STREAM_OPTION;
extern uint8                         g_MOVIE_STREAM_OPTION_ALIGNMENT[3];
extern uint32                        g_MOVIE_STREAM_RESERVED_WORD;
extern int32                         g_FRONTEND_MOVIE_MODE;
extern int32                         g_FRONTEND_MOVIE_REQUEST_VALUE;
extern MovieVideoConfiguration       g_MOVIE_VIDEO_CONFIGURATION_STORAGE;

#define G_MOVIE_PLAYBACK_OPTIONS g_MOVIE_PLAYBACK_OPTIONS
#define G_MOVIE_FRAME_CALLBACK g_MOVIE_FRAME_CALLBACK
#define G_MOVIE_STREAM_BUFFER_ADJUSTMENT g_MOVIE_STREAM_BUFFER_ADJUSTMENT
#define G_MOVIE_PLAYBACK_RETAINED_STATE g_MOVIE_PLAYBACK_RETAINED_STATE
#define G_MOVIE_PLAYBACK_X_POSITION g_MOVIE_PLAYBACK_X_POSITION
#define G_MOVIE_PLAYBACK_Y_POSITION g_MOVIE_PLAYBACK_Y_POSITION
#define G_MOVIE_STREAM_BLOCK_SIZE g_MOVIE_STREAM_BLOCK_SIZE
#define G_MOVIE_STREAM_BUFFER_COUNT g_MOVIE_STREAM_BUFFER_COUNT
#define G_MOVIE_PLAYBACK_RETAINED_DEFAULTS g_MOVIE_PLAYBACK_RETAINED_DEFAULTS
#define G_MOVIE_AUDIO_CHANNEL_COUNT g_MOVIE_AUDIO_CHANNEL_COUNT
#define G_FRONTEND_MOVIE_STREAM_OPTION g_FRONTEND_MOVIE_STREAM_OPTION
#define G_MOVIE_STREAM_OPTION_ALIGNMENT g_MOVIE_STREAM_OPTION_ALIGNMENT
#define G_MOVIE_STREAM_RESERVED_WORD g_MOVIE_STREAM_RESERVED_WORD
#define G_FRONTEND_MOVIE_MODE g_FRONTEND_MOVIE_MODE
#define G_FRONTEND_MOVIE_REQUEST_VALUE g_FRONTEND_MOVIE_REQUEST_VALUE
#define G_MOVIE_VIDEO_CONFIGURATION_STORAGE g_MOVIE_VIDEO_CONFIGURATION_STORAGE

typedef struct MovieStreamPlayer
{
  MoviePlayerServiceCallback           service_callback;
  struct MoviePlaybackCallbackContext *service_context;
  DSHeaderChunk                        header;
  DSDataBufPtr                         buffer_list;
  NfsDataAcquisitionContext           *acquisition_context;
  DataStreamState                     *stream;
  void                                *video_configuration;
  Item                                 timer_io_request;
  Item                                 vram_io_request;
  Item                                 message_port;
  Item                                 message_item;
  Item                                 end_of_stream_message_item;
  ControlSubscriberContext            *control_context;
  StreamingAudioContext               *audio_context;
  CpakContextState                    *cpak_context;
  int32                                current_frame_buffer_index;
  int32                                next_frame_buffer_index;
  CpakRecordState                     *cpak_records[MOVIE_FRAME_BUFFER_COUNT];
} MovieStreamPlayer;

typedef struct MoviePlaybackCallbackContext
{
  boolean            clock_reset_pending;
  boolean            clock_initialized;
  boolean            stream_state_pending;
  boolean            secondary_preroll_pending;
  MovieStreamPlayer *secondary_player;
} MoviePlaybackCallbackContext;

typedef char
  MovieStreamPlayerSizeCheck[sizeof(MovieStreamPlayer) == MOVIE_STREAM_PLAYER_BYTE_COUNT ? 1 : -1];

void
set_movie_frame_callback(MovieFrameCallback callback_);
void
initialize_movie_stream_header(DSHeaderChunk *header_, boolean audio_enabled_);
int32
initialize_movie_stream_player(MovieStreamPlayer *player_, char *stream_path_);
int32
dispose_movie_stream_player(MovieStreamPlayer *player_);
int32
service_movie_playback(MovieStreamPlayer *player_, MoviePlaybackCallbackContext *context_);
uint32
play_movie_stream_player(MovieStreamPlayer            *player_,
                         void                         *video_configuration_,
                         int32                         x_position_,
                         int32                         y_position_,
                         MoviePlayerServiceCallback    service_callback_,
                         MoviePlaybackCallbackContext *service_context_);
boolean
play_movie_stream(char *stream_path_, uint8 audio_enabled_, char *secondary_stream_path_);
