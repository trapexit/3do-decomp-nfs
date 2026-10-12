#include "runtime_audio_template.h"
#include "streaming_audio_subscriber.h"

#include "audio.h"
#include "satemplates.h"

#define STREAMING_AUDIO_TEMPLATE_COUNT 10

enum
{
  STREAMING_AUDIO_8_BIT_SAMPLE_SIZE = 8,
  STREAMING_AUDIO_HALF_SAMPLE_RATE  = 22050,
  STREAMING_AUDIO_FULL_SAMPLE_RATE  = 44100
};

const char G_HALF_MONO8_INSTRUMENT[16]          = "halfmono8.dsp";
const char G_HALF_STEREO8_INSTRUMENT[16]        = "halfstereo8.dsp";
const char G_FIXED_STEREO8_INSTRUMENT[20]       = "fixedstereo8.dsp";
const char G_FIXED_MONO8_INSTRUMENT[16]         = "fixedmono8.dsp";
const char G_FIXED_MONO_SAMPLE_INSTRUMENT[20]   = "fixedmonosample.dsp";
const char G_FIXED_STEREO_SAMPLE_INSTRUMENT[24] = "fixedstereosample.dsp";
const char G_DCSQXD_MONO_INSTRUMENT[16]         = "dcsqxdmono.dsp";
const char G_DCSQXD_STEREO_INSTRUMENT[20]       = "dcsqxdstereo.dsp";
const char G_DCSQXD_HALF_MONO_INSTRUMENT[20]    = "dcsqxdhalfmono.dsp";
const char G_DCSQXD_HALF_STEREO_INSTRUMENT[24]  = "dcsqxdhalfstereo.dsp";

TemplateRec g_INITIAL_TEMPLATES[STREAMING_AUDIO_TEMPLATE_COUNT] = {
  { 0x01010202, 0, (char *) G_HALF_MONO8_INSTRUMENT },
  { 0x01020202, 0, (char *) G_HALF_STEREO8_INSTRUMENT },
  { 0x01020102, 0, (char *) G_FIXED_STEREO8_INSTRUMENT },
  { 0x01010102, 0, (char *) G_FIXED_MONO8_INSTRUMENT },
  { 0x01010101, 0, (char *) G_FIXED_MONO_SAMPLE_INSTRUMENT },
  { 0x01020101, 0, (char *) G_FIXED_STEREO_SAMPLE_INSTRUMENT },
  { 0x02010101, 0, (char *) G_DCSQXD_MONO_INSTRUMENT },
  { 0x02020101, 0, (char *) G_DCSQXD_STEREO_INSTRUMENT },
  { 0x02010201, 0, (char *) G_DCSQXD_HALF_MONO_INSTRUMENT },
  { 0x02020201, 0, (char *) G_DCSQXD_HALF_STEREO_INSTRUMENT }
};

typedef char StreamingAudioInitialTemplatesSizeCheck[sizeof(g_INITIAL_TEMPLATES) == 120 ? 1 : -1];

int32 g_STREAMING_AUDIO_MAX_TEMPLATE_COUNT = STREAMING_AUDIO_TEMPLATE_COUNT;

int32
GetTemplateTag(SAudioSampleDescriptorPtr descriptor_)
{
  if(descriptor_->numChannels == SA_MONO)
    {
      if(descriptor_->compressionType == ID_SDX2)
        {
          if(descriptor_->sampleRate == STREAMING_AUDIO_HALF_SAMPLE_RATE)
            {
              return SA_22K_16B_M_SDX2;
            }
          return SA_44K_16B_M_SDX2;
        }

      if(descriptor_->sampleSize == STREAMING_AUDIO_8_BIT_SAMPLE_SIZE)
        {
          if(descriptor_->sampleRate == STREAMING_AUDIO_HALF_SAMPLE_RATE)
            {
              return SA_22K_8B_M;
            }
          return SA_44K_8B_M;
        }

      if(descriptor_->sampleRate == STREAMING_AUDIO_FULL_SAMPLE_RATE)
        {
          return SA_44K_16B_M;
        }
    }

  if(descriptor_->numChannels == SA_STEREO)
    {
      if(descriptor_->compressionType == ID_SDX2)
        {
          if(descriptor_->sampleRate == STREAMING_AUDIO_HALF_SAMPLE_RATE)
            {
              return SA_22K_16B_S_SDX2;
            }
          return SA_44K_16B_S_SDX2;
        }

      if(descriptor_->sampleSize == STREAMING_AUDIO_8_BIT_SAMPLE_SIZE)
        {
          if(descriptor_->sampleRate == STREAMING_AUDIO_HALF_SAMPLE_RATE)
            {
              return SA_22K_8B_S;
            }
          return SA_44K_8B_S;
        }

      if(descriptor_->sampleRate == STREAMING_AUDIO_FULL_SAMPLE_RATE)
        {
          return SA_44K_16B_S;
        }
    }

  return -1;
}

Item
GetTemplateItem(struct SAudioContext *template_context_, int32 template_tag_, int32 template_count_)
{
  TemplateRecPtr template;
  int32 template_index;

  template       = (TemplateRecPtr) template_context_;
  template_index = 0;
  while(template_index < template_count_)
    {
      if(template->templateTag == template_tag_)
        {
          if(template->templateItem == 0)
            template->templateItem = load_runtime_instrument_template(template->instrumentName, 0);
          return template->templateItem;
        }

      ++template_index;
      ++template;
    }

  return SAUDIO_ERROR_TEMPLATE_NOT_FOUND;
}

int32
LoadTemplates(struct SAudioContext *template_context_, int32 *template_tags_, int32 template_count_)
{
  Item template_item;

  while(*template_tags_ != 0)
    {
      template_item = GetTemplateItem(template_context_, *template_tags_, template_count_);
      if(template_item < 0)
        return template_item;

      ++template_tags_;
    }

  return 0;
}
