#include "runtime_audio_template.h"
#include "runtime_memory.h"
#include "sdk_veneers.h"
#include "stream_message_items.h"
#include "streaming_audio_subscriber.h"

#include "kernel.h"

#define STREAMING_AUDIO_TEMPLATE_MEMORY_NAME "tmplArray"
#define STREAMING_AUDIO_NO_MEMORY -992

int32
initialize_streaming_audio_thread(StreamingAudioContext *context_)
{
  StreamingAudioChannel *channel;
  uint32                 template_byte_count;
  int32                  channel_number;
  int32                  status;
  int32                  signal_status;

  context_->creator_status     = 0;
  context_->request_port       = 0;
  context_->all_buffer_signals = 0;

  status = OpenAudioFolio();
  if(status < 0)
    {
      context_->creator_status = status;
      goto signal_creator;
    }

  template_byte_count = sizeof(TemplateRec) * g_STREAMING_AUDIO_MAX_TEMPLATE_COUNT;
  context_->template_array =
    allocate_runtime_memory(STREAMING_AUDIO_TEMPLATE_MEMORY_NAME, template_byte_count, 0);
  if(context_->template_array == NULL)
    {
      context_->creator_status = STREAMING_AUDIO_NO_MEMORY;
      goto signal_creator;
    }
  RUNTIME_MEMCPY_VENEER(context_->template_array, g_INITIAL_TEMPLATES, template_byte_count);

  context_->output_template_item = LOAD_INSTRUMENT_TEMPLATE_VENEER("mixer2x2.dsp", 0);
  if(context_->output_template_item < 0)
    {
      context_->creator_status = context_->output_template_item;
      goto signal_creator;
    }

  context_->envelope_template_item = LOAD_INSTRUMENT_TEMPLATE_VENEER("envelope.dsp", 0);
  if(context_->envelope_template_item < 0)
    {
      context_->creator_status = context_->envelope_template_item;
      goto signal_creator;
    }

  context_->clock_channel = 0;
  for(channel_number = 0; channel_number < STREAMING_AUDIO_CHANNEL_COUNT; ++channel_number)
    {
      channel                           = &context_->channels[channel_number];
      channel->status                   = 0;
      channel->buffer_count             = 0;
      channel->buffer_pool              = NULL;
      channel->channel_instrument       = 0;
      channel->instrument_started       = 0;
      channel->attachments_running      = 0;
      channel->signal_mask              = 0;
      channel->pending_messages.head    = NULL;
      channel->pending_messages.tail    = NULL;
      channel->in_use_count             = 0;
      channel->in_use_head              = NULL;
      channel->in_use_tail              = NULL;
      channel->output.instrument        = 0;
      channel->output.channel_count     = 0;
      channel->output.left_envelope     = 0;
      channel->output.left_target_knob  = 0;
      channel->output.right_envelope    = 0;
      channel->output.right_target_knob = 0;
      channel->output.current_amplitude = -1;
      channel->output.saved_amplitude   = 0;
      channel->output.muted             = 0;
      channel->output.externally_muted  = 0;
      channel->output.current_pan       = 0;
    }

  status = create_message_port(&context_->request_port_signal);
  if(status > 0)
    {
      context_->request_port = status;
    }

signal_creator:
  signal_status = SendSignal(context_->creator_task, context_->creator_signal);
  if(context_->creator_status < 0 || signal_status < 0)
    {
      return -1;
    }
  return 0;
}
