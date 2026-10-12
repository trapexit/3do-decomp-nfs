/* Owns the ordered audio-folio setup for one streamed movie-audio channel. */

#include "item_cleanup.h"
#include "movie_audio.h"
#include "streaming_audio_subscriber.h"

#include "audio.h"
#include "satemplates.h"

#if defined(__ARMCC_VERSION)
#pragma no_optimize_crossjump
#pragma no_optimize_cse
#pragma no_optimize_multiple_loads
#pragma no_optimize_scheduling
#endif

#define STREAMING_AUDIO_ENVELOPE_INCREMENT_KNOB_NAME "Env.incr"
#define STREAMING_AUDIO_ENVELOPE_OUTPUT_NAME "Output"
#define STREAMING_AUDIO_ENVELOPE_REQUEST_KNOB_NAME "Env.request"
#define STREAMING_AUDIO_INPUT_0_NAME "Input0"
#define STREAMING_AUDIO_INPUT_1_NAME "Input1"
#define STREAMING_AUDIO_LEFT_GAIN_0_NAME "LeftGain0"
#define STREAMING_AUDIO_LEFT_GAIN_1_NAME "LeftGain1"
#define STREAMING_AUDIO_LEFT_OUTPUT_NAME "LeftOutput"
#define STREAMING_AUDIO_RIGHT_GAIN_0_NAME "RightGain0"
#define STREAMING_AUDIO_RIGHT_GAIN_1_NAME "RightGain1"
#define STREAMING_AUDIO_RIGHT_OUTPUT_NAME "RightOutput"

typedef enum StreamingAudioInitError
{
  STREAMING_AUDIO_INIT_ERROR_TEMPLATE_NOT_FOUND        = -7000,
  STREAMING_AUDIO_INIT_ERROR_ALLOC_INSTRUMENT_FAILED   = -6996,
  STREAMING_AUDIO_INIT_ERROR_UNSUPPORTED_SAMPLE_TYPE   = -6995,
  STREAMING_AUDIO_INIT_ERROR_CONNECT_INSTRUMENT_FAILED = -6994,
  STREAMING_AUDIO_INIT_ERROR_GRAB_KNOB_FAILED          = -6993,
  STREAMING_AUDIO_INIT_ERROR_ALLOC_BUFFER_POOL_FAILED  = -6992,
  STREAMING_AUDIO_INIT_ERROR_STREAM_VERSION_INVALID    = -6988
} StreamingAudioInitError;

typedef enum StreamingAudioInitValue
{
  STREAMING_AUDIO_INIT_VALUE_SUPPORTED_STREAM_VERSION = 0,
  STREAMING_AUDIO_INIT_VALUE_INTERNAL_MUTE            = 0,
  STREAMING_AUDIO_INIT_VALUE_MONO_CHANNEL_COUNT       = 1,
  STREAMING_AUDIO_INIT_VALUE_CHANNEL_ENABLED          = 0x01,
  STREAMING_AUDIO_INIT_VALUE_STEREO_CHANNEL_COUNT     = 2,
  STREAMING_AUDIO_INIT_VALUE_CHANNEL_ACTIVE           = 0x02,
  STREAMING_AUDIO_INIT_VALUE_ENVELOPE_RATE            = 10,
  STREAMING_AUDIO_INIT_VALUE_INSTRUMENT_PRIORITY      = 100,
  STREAMING_AUDIO_INIT_VALUE_UNSET_AMPLITUDE          = -1
} StreamingAudioInitValue;

/* The retail routine exposes every partial initialization to later cleanup.
   Keep its single ordered body so each failure preserves the same side effects. */
int32
initialize_streaming_audio_channel(StreamingAudioContext *context_, SAudioHeaderChunk *header_)
{
  StreamingAudioBufferInitialization buffer_initialization;
  StreamingAudioChannel             *channel;
  StreamingAudioOutput              *output;
  Item                               template_item;
  Item                               temporary_knob;
  int32                              status;
  int32                              template_tag;

  channel = &context_->channels[header_->channel];
  output  = &channel->output;

  if(header_->version > STREAMING_AUDIO_INIT_VALUE_SUPPORTED_STREAM_VERSION)
    return STREAMING_AUDIO_INIT_ERROR_STREAM_VERSION_INVALID;

  if(header_->channel == 0)
    channel->status = (channel->status | STREAMING_AUDIO_INIT_VALUE_CHANNEL_ENABLED);

  channel->status = (channel->status | STREAMING_AUDIO_INIT_VALUE_CHANNEL_ACTIVE);

  if(output->instrument == 0)
    {
      status = AllocInstrument(context_->output_template_item,
                               STREAMING_AUDIO_INIT_VALUE_INSTRUMENT_PRIORITY);
      if(status < 0)
        return STREAMING_AUDIO_INIT_ERROR_ALLOC_INSTRUMENT_FAILED;

      output->instrument = status;
      StartInstrument(output->instrument, NULL);

      status = AllocInstrument(context_->envelope_template_item,
                               STREAMING_AUDIO_INIT_VALUE_INSTRUMENT_PRIORITY);
      if(status < 0)
        return STREAMING_AUDIO_INIT_ERROR_ALLOC_INSTRUMENT_FAILED;

      output->left_envelope = status;
      status                        = ConnectInstruments(output->left_envelope,
                                                         STREAMING_AUDIO_ENVELOPE_OUTPUT_NAME,
                                                         output->instrument,
                                                         STREAMING_AUDIO_LEFT_GAIN_0_NAME);
      if(status < 0)
        return STREAMING_AUDIO_INIT_ERROR_CONNECT_INSTRUMENT_FAILED;

      if((header_->sampleDesc.numChannels == STREAMING_AUDIO_INIT_VALUE_STEREO_CHANNEL_COUNT) &&
         (g_STEREO_MOVIE_AUDIO_ENABLED == 1))
        {
          status = ConnectInstruments(output->left_envelope,
                                      STREAMING_AUDIO_ENVELOPE_OUTPUT_NAME,
                                      output->instrument,
                                      STREAMING_AUDIO_RIGHT_GAIN_0_NAME);
          if(status < 0)
            return STREAMING_AUDIO_INIT_ERROR_CONNECT_INSTRUMENT_FAILED;
        }

      status =
        GrabKnob(output->left_envelope, STREAMING_AUDIO_ENVELOPE_INCREMENT_KNOB_NAME);
      if(status < 0)
        return STREAMING_AUDIO_INIT_ERROR_GRAB_KNOB_FAILED;

      temporary_knob = status;
      TweakRawKnob(temporary_knob, STREAMING_AUDIO_INIT_VALUE_ENVELOPE_RATE);
      delete_resource_item(temporary_knob);

      status = GrabKnob(output->left_envelope, STREAMING_AUDIO_ENVELOPE_REQUEST_KNOB_NAME);
      if(status < 0)
        return STREAMING_AUDIO_INIT_ERROR_GRAB_KNOB_FAILED;

      output->left_target_knob = status;

      if(header_->sampleDesc.numChannels == STREAMING_AUDIO_INIT_VALUE_MONO_CHANNEL_COUNT)
        {
          status = AllocInstrument(context_->envelope_template_item,
                                   STREAMING_AUDIO_INIT_VALUE_INSTRUMENT_PRIORITY);
          if(status < 0)
            return STREAMING_AUDIO_INIT_ERROR_ALLOC_INSTRUMENT_FAILED;

          output->right_envelope = status;
          status                         = ConnectInstruments(output->right_envelope,
                                                              STREAMING_AUDIO_ENVELOPE_OUTPUT_NAME,
                                                              output->instrument,
                                                              STREAMING_AUDIO_RIGHT_GAIN_0_NAME);
          if(status < 0)
            return STREAMING_AUDIO_INIT_ERROR_CONNECT_INSTRUMENT_FAILED;

          status =
            GrabKnob(output->right_envelope, STREAMING_AUDIO_ENVELOPE_INCREMENT_KNOB_NAME);
          if(status < 0)
            return STREAMING_AUDIO_INIT_ERROR_GRAB_KNOB_FAILED;

          temporary_knob = status;
          TweakRawKnob(temporary_knob, STREAMING_AUDIO_INIT_VALUE_ENVELOPE_RATE);
          delete_resource_item(temporary_knob);

          status =
            GrabKnob(output->right_envelope, STREAMING_AUDIO_ENVELOPE_REQUEST_KNOB_NAME);
          if(status < 0)
            return STREAMING_AUDIO_INIT_ERROR_GRAB_KNOB_FAILED;

          output->right_target_knob = status;
          StartInstrument(output->right_envelope, NULL);
        }
      else
        {
          status = ConnectInstruments(output->left_envelope,
                                      STREAMING_AUDIO_ENVELOPE_OUTPUT_NAME,
                                      output->instrument,
                                      STREAMING_AUDIO_RIGHT_GAIN_1_NAME);
          if(status < 0)
            return STREAMING_AUDIO_INIT_ERROR_CONNECT_INSTRUMENT_FAILED;

          if(g_STEREO_MOVIE_AUDIO_ENABLED == 1)
            {
              status = ConnectInstruments(output->left_envelope,
                                          STREAMING_AUDIO_ENVELOPE_OUTPUT_NAME,
                                          output->instrument,
                                          STREAMING_AUDIO_LEFT_GAIN_1_NAME);
              if(status < 0)
                return STREAMING_AUDIO_INIT_ERROR_CONNECT_INSTRUMENT_FAILED;
            }
        }

      StartInstrument(output->left_envelope, NULL);
    }

  if(channel->channel_instrument == 0)
    {
      if(header_->sampleDesc.numChannels == STREAMING_AUDIO_INIT_VALUE_MONO_CHANNEL_COUNT)
        output->channel_count = STREAMING_AUDIO_INIT_VALUE_MONO_CHANNEL_COUNT;
      else
        output->channel_count = STREAMING_AUDIO_INIT_VALUE_STEREO_CHANNEL_COUNT;

      channel->buffer_count = header_->numBuffers;
      status = GetTemplateTag(&header_->sampleDesc);
      if(status < 0)
        return STREAMING_AUDIO_INIT_ERROR_UNSUPPORTED_SAMPLE_TYPE;
      template_tag = status;

      template_item = GetTemplateItem((struct SAudioContext *) context_->template_array,
                                      template_tag,
                                      g_STREAMING_AUDIO_MAX_TEMPLATE_COUNT);
      if(template_item < 0)
        return STREAMING_AUDIO_INIT_ERROR_TEMPLATE_NOT_FOUND;

      status = AllocInstrument(template_item, STREAMING_AUDIO_INIT_VALUE_INSTRUMENT_PRIORITY);
      if(status < 0)
        return STREAMING_AUDIO_INIT_ERROR_ALLOC_INSTRUMENT_FAILED;

      channel->channel_instrument = status;
      if(output->channel_count == STREAMING_AUDIO_INIT_VALUE_MONO_CHANNEL_COUNT)
        {
          status = ConnectInstruments(channel->channel_instrument,
                                      STREAMING_AUDIO_ENVELOPE_OUTPUT_NAME,
                                      output->instrument,
                                      STREAMING_AUDIO_INPUT_0_NAME);
          if(status < 0)
            return STREAMING_AUDIO_INIT_ERROR_CONNECT_INSTRUMENT_FAILED;
        }
      else
        {
          status = ConnectInstruments(channel->channel_instrument,
                                      STREAMING_AUDIO_LEFT_OUTPUT_NAME,
                                      output->instrument,
                                      STREAMING_AUDIO_INPUT_0_NAME);
          if(status < 0)
            return STREAMING_AUDIO_INIT_ERROR_CONNECT_INSTRUMENT_FAILED;

          status = ConnectInstruments(channel->channel_instrument,
                                      STREAMING_AUDIO_RIGHT_OUTPUT_NAME,
                                      output->instrument,
                                      STREAMING_AUDIO_INPUT_1_NAME);
          if(status < 0)
            return STREAMING_AUDIO_INIT_ERROR_CONNECT_INSTRUMENT_FAILED;
        }

      StartInstrument(channel->channel_instrument, NULL);
      channel->instrument_started = 1;
      mute_audio_channel(context_, header_->channel, STREAMING_AUDIO_INIT_VALUE_INTERNAL_MUTE);

      if(output->saved_amplitude == STREAMING_AUDIO_INIT_VALUE_UNSET_AMPLITUDE)
        {
          if((output->channel_count == STREAMING_AUDIO_INIT_VALUE_STEREO_CHANNEL_COUNT) &&
             (g_STEREO_MOVIE_AUDIO_ENABLED == 1))
            status = set_audio_channel_amplitude(
              context_,
              header_->channel,
              (header_->initialAmplitude / STREAMING_AUDIO_INIT_VALUE_STEREO_CHANNEL_COUNT));
          else
            status =
              set_audio_channel_amplitude(context_, header_->channel, header_->initialAmplitude);
        }

      if(status < 0)
        return status;

      if(output->channel_count == STREAMING_AUDIO_INIT_VALUE_MONO_CHANNEL_COUNT)
        {
          if(output->current_pan == 0)
            status = set_audio_channel_pan(context_, header_->channel, header_->initialPan);
          else
            status = set_audio_channel_pan(context_, header_->channel, output->current_pan);
        }

      if(status < 0)
        return status;
    }

  if(channel->buffer_pool == NULL)
    {
      channel->buffer_pool = create_memory_pool(header_->numBuffers, sizeof(StreamingAudioBuffer));
      if(channel->buffer_pool == NULL)
        return STREAMING_AUDIO_INIT_ERROR_ALLOC_BUFFER_POOL_FAILED;

      buffer_initialization.channel = channel;
      buffer_initialization.header  = header_;
      for_each_free_memory_pool_entry(channel->buffer_pool,
                                      initialize_streaming_audio_buffer,
                                      &buffer_initialization);
    }

  return 0;
}
