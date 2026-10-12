#include "streaming_audio_buffer_queue.h"
#include "streaming_audio_subscriber.h"

int32
dispatch_streaming_audio_control(StreamingAudioContext *context_, SubscriberMsgPtr message_)
{
  SAudioCtlBlock *control;
  int32           channel_number;

  control = (SAudioCtlBlock *) message_->msg.control.controlArg2;

  switch(message_->msg.control.controlArg1)
    {
      case kSAudioCtlOpLoadTemplates:
        return LoadTemplates((struct SAudioContext *) context_->template_array,
                             control->loadTemplates.tagListPtr,
                             g_STREAMING_AUDIO_MAX_TEMPLATE_COUNT);

      case kSAudioCtlOpSetVol:
        channel_number = control->amplitude.channelNumber;
        if(channel_number >= SA_SUBS_MAX_CHANNELS)
          {
            return SAUDIO_ERROR_CHANNEL_OUT_OF_RANGE;
          }
        return set_audio_channel_amplitude(context_, channel_number, control->amplitude.value);

      case kSAudioCtlOpSetPan:
        channel_number = control->pan.channelNumber;
        if(channel_number >= SA_SUBS_MAX_CHANNELS)
          {
            return SAUDIO_ERROR_CHANNEL_OUT_OF_RANGE;
          }
        return set_audio_channel_pan(context_, channel_number, control->pan.value);

      case kSAudioCtlOpSetClockChan:
        channel_number = control->clock.channelNumber;
        if(channel_number < SA_SUBS_MAX_CHANNELS)
          {
            context_->clock_channel = channel_number;
            return 0;
          }
        return SAUDIO_ERROR_CHANNEL_OUT_OF_RANGE;

      case kSAudioCtlOpGetVol:
        channel_number = control->amplitude.channelNumber;
        if(channel_number >= SA_SUBS_MAX_CHANNELS)
          {
            return SAUDIO_ERROR_CHANNEL_OUT_OF_RANGE;
          }
        return get_audio_channel_amplitude(context_, channel_number, &control->amplitude.value);

      case kSAudioCtlOpGetFreq:
        return 0;

      case kSAudioCtlOpGetPan:
        channel_number = control->pan.channelNumber;
        if(channel_number >= SA_SUBS_MAX_CHANNELS)
          {
            return SAUDIO_ERROR_CHANNEL_OUT_OF_RANGE;
          }
        return get_audio_channel_pan(context_, channel_number, &control->pan.value);

      case kSAudioCtlOpGetClockChan:
        channel_number = control->clock.channelNumber;
        if(channel_number < SA_SUBS_MAX_CHANNELS)
          {
            control->clock.channelNumber = context_->clock_channel;
            return 0;
          }
        return SAUDIO_ERROR_CHANNEL_OUT_OF_RANGE;

      case kSAudioCtlOpCloseChannel:
        return close_streaming_audio_channel(context_, control->CloseSAudioChannel.channelNumber);

      case kSAudioCtlOpFlushChannel:
        return flush_streaming_audio_channel(context_, control->FlushSAudioChannel.channelNumber);

      case kSAudioCtlOpMute:
        channel_number = control->mute.channelNumber;
        if(channel_number >= SA_SUBS_MAX_CHANNELS)
          {
            return SAUDIO_ERROR_CHANNEL_OUT_OF_RANGE;
          }
        return mute_audio_channel(context_, channel_number, 1);

      case kSAudioCtlOpUnMute:
        channel_number = control->unMute.channelNumber;
        if(channel_number >= SA_SUBS_MAX_CHANNELS)
          {
            return SAUDIO_ERROR_CHANNEL_OUT_OF_RANGE;
          }
        return unmute_audio_channel(context_, channel_number, 1);

      default:
        return 0;
    }
}
