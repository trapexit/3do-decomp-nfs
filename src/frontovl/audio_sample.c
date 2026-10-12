#include "audio_sample.h"

#include "sdk_veneers.h"

#include "audio.h"

#define MONO_SAMPLE_CHANNEL_COUNT 1
#define STEREO_SAMPLE_CHANNEL_COUNT 2
#define SAMPLE_WIDTH_BITS 16
#define MONO_SAMPLE_RATE ((uint32) 22050 << 16)
#define STEREO_SAMPLE_RATE ((uint32) 44100 << 16)

enum
{
  SAMPLE_CHANNELS_TAG_INDEX,
  SAMPLE_SUSTAIN_BEGIN_TAG_INDEX,
  SAMPLE_SUSTAIN_END_TAG_INDEX,
  SAMPLE_RATE_TAG_INDEX,
  SAMPLE_BITS_TAG_INDEX,
  SAMPLE_ADDRESS_TAG_INDEX,
  SAMPLE_BYTE_COUNT_TAG_INDEX,
  SAMPLE_END_TAG_INDEX,
  SAMPLE_TAG_COUNT
};

Item
create_mono_22050_sample(void *sample_data_, int32 byte_count_)
{
  TagArg tags[SAMPLE_TAG_COUNT];

  tags[SAMPLE_CHANNELS_TAG_INDEX].ta_Tag      = AF_TAG_CHANNELS;
  tags[SAMPLE_CHANNELS_TAG_INDEX].ta_Arg      = (void *) MONO_SAMPLE_CHANNEL_COUNT;
  tags[SAMPLE_SUSTAIN_BEGIN_TAG_INDEX].ta_Tag = AF_TAG_SUSTAINBEGIN;
  tags[SAMPLE_SUSTAIN_BEGIN_TAG_INDEX].ta_Arg = (void *) -1;
  tags[SAMPLE_SUSTAIN_END_TAG_INDEX].ta_Tag   = AF_TAG_SUSTAINEND;
  tags[SAMPLE_SUSTAIN_END_TAG_INDEX].ta_Arg   = (void *) -1;
  tags[SAMPLE_RATE_TAG_INDEX].ta_Tag          = AF_TAG_SAMPLE_RATE;
  tags[SAMPLE_RATE_TAG_INDEX].ta_Arg          = (void *) MONO_SAMPLE_RATE;
  tags[SAMPLE_BITS_TAG_INDEX].ta_Tag          = AF_TAG_NUMBITS;
  tags[SAMPLE_BITS_TAG_INDEX].ta_Arg          = (void *) SAMPLE_WIDTH_BITS;
  tags[SAMPLE_ADDRESS_TAG_INDEX].ta_Tag       = AF_TAG_ADDRESS;
  tags[SAMPLE_ADDRESS_TAG_INDEX].ta_Arg       = sample_data_;
  tags[SAMPLE_BYTE_COUNT_TAG_INDEX].ta_Tag    = AF_TAG_NUMBYTES;
  tags[SAMPLE_BYTE_COUNT_TAG_INDEX].ta_Arg    = (void *) byte_count_;
  tags[SAMPLE_END_TAG_INDEX].ta_Tag           = TAG_END;

  return CreateSample(tags);
}

Item
create_stereo_44100_sample(void *sample_data_, int32 byte_count_)
{
  TagArg tags[SAMPLE_TAG_COUNT];

  tags[SAMPLE_CHANNELS_TAG_INDEX].ta_Tag = AF_TAG_CHANNELS;
  tags[SAMPLE_CHANNELS_TAG_INDEX].ta_Arg = (void *) STEREO_SAMPLE_CHANNEL_COUNT;
  tags[SAMPLE_SUSTAIN_BEGIN_TAG_INDEX].ta_Tag = AF_TAG_SUSTAINBEGIN;
  tags[SAMPLE_SUSTAIN_BEGIN_TAG_INDEX].ta_Arg = (void *) -1;
  tags[SAMPLE_SUSTAIN_END_TAG_INDEX].ta_Tag   = AF_TAG_SUSTAINEND;
  tags[SAMPLE_SUSTAIN_END_TAG_INDEX].ta_Arg   = (void *) -1;
  tags[SAMPLE_RATE_TAG_INDEX].ta_Tag          = AF_TAG_SAMPLE_RATE;
  tags[SAMPLE_RATE_TAG_INDEX].ta_Arg          = (void *) STEREO_SAMPLE_RATE;
  tags[SAMPLE_BITS_TAG_INDEX].ta_Tag          = AF_TAG_NUMBITS;
  tags[SAMPLE_BITS_TAG_INDEX].ta_Arg          = (void *) SAMPLE_WIDTH_BITS;
  tags[SAMPLE_ADDRESS_TAG_INDEX].ta_Tag       = AF_TAG_ADDRESS;
  tags[SAMPLE_ADDRESS_TAG_INDEX].ta_Arg       = sample_data_;
  tags[SAMPLE_BYTE_COUNT_TAG_INDEX].ta_Tag    = AF_TAG_NUMBYTES;
  tags[SAMPLE_BYTE_COUNT_TAG_INDEX].ta_Arg    = (void *) byte_count_;
  tags[SAMPLE_END_TAG_INDEX].ta_Tag           = TAG_END;

  return CreateSample(tags);
}
