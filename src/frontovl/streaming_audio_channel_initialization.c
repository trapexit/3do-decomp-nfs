/* Owns Audio Folio item creation for each streamed-audio buffer. */

#include "streaming_audio_subscriber.h"

#include "audio.h"
#include "kernel.h"

#define STREAMING_AUDIO_SAMPLE_TAG_COUNT 6
#define STREAMING_AUDIO_ATTACHMENT_TAG_COUNT 2

typedef enum StreamingAudioSampleTagIndex
{
  STREAMING_AUDIO_SAMPLE_TAG_INDEX_CHANNELS,
  STREAMING_AUDIO_SAMPLE_TAG_INDEX_BITS,
  STREAMING_AUDIO_SAMPLE_TAG_INDEX_RATE,
  STREAMING_AUDIO_SAMPLE_TAG_INDEX_COMPRESSION_RATIO,
  STREAMING_AUDIO_SAMPLE_TAG_INDEX_COMPRESSION_TYPE,
  STREAMING_AUDIO_SAMPLE_TAG_INDEX_END
} StreamingAudioSampleTagIndex;

typedef enum StreamingAudioAttachmentTagIndex
{
  STREAMING_AUDIO_ATTACHMENT_TAG_INDEX_FLAGS,
  STREAMING_AUDIO_ATTACHMENT_TAG_INDEX_END
} StreamingAudioAttachmentTagIndex;

/* Keep the retail tag and item creation order so partial failures expose the
   same pool-entry state to channel cleanup. */
int32
initialize_streaming_audio_buffer(void *argument_, void *pool_entry_)
{
  StreamingAudioBufferInitialization *initialization;
  TagArg                              attachment_tags[STREAMING_AUDIO_ATTACHMENT_TAG_COUNT];
  StreamingAudioBuffer               *buffer;
  StreamingAudioChannel              *channel;
  SAudioHeaderChunk                  *header;
  TagArg                              sample_tags[STREAMING_AUDIO_SAMPLE_TAG_COUNT];
  int32                               status;

  initialization = (StreamingAudioBufferInitialization *) argument_;
  buffer         = (StreamingAudioBuffer *) pool_entry_;
  channel        = initialization->channel;
  header         = initialization->header;
  /* Portfolio TagArg stores immediate 32-bit values in the ta_Arg slot. */

  sample_tags[STREAMING_AUDIO_SAMPLE_TAG_INDEX_CHANNELS].ta_Tag = AF_TAG_CHANNELS;
  sample_tags[STREAMING_AUDIO_SAMPLE_TAG_INDEX_CHANNELS].ta_Arg =
    (void *) header->sampleDesc.numChannels;
  sample_tags[STREAMING_AUDIO_SAMPLE_TAG_INDEX_BITS].ta_Tag = AF_TAG_NUMBITS;
  sample_tags[STREAMING_AUDIO_SAMPLE_TAG_INDEX_BITS].ta_Arg =
    (void *) header->sampleDesc.sampleSize;
  sample_tags[STREAMING_AUDIO_SAMPLE_TAG_INDEX_RATE].ta_Tag = AF_TAG_SAMPLE_RATE;
  sample_tags[STREAMING_AUDIO_SAMPLE_TAG_INDEX_RATE].ta_Arg =
    (void *) ((uint32) header->sampleDesc.sampleRate << 16);
  sample_tags[STREAMING_AUDIO_SAMPLE_TAG_INDEX_COMPRESSION_RATIO].ta_Tag = AF_TAG_COMPRESSIONRATIO;
  sample_tags[STREAMING_AUDIO_SAMPLE_TAG_INDEX_COMPRESSION_RATIO].ta_Arg =
    (void *) header->sampleDesc.compressionRatio;
  sample_tags[STREAMING_AUDIO_SAMPLE_TAG_INDEX_COMPRESSION_TYPE].ta_Tag = AF_TAG_COMPRESSIONTYPE;
  sample_tags[STREAMING_AUDIO_SAMPLE_TAG_INDEX_COMPRESSION_TYPE].ta_Arg =
    (void *) header->sampleDesc.compressionType;
  sample_tags[STREAMING_AUDIO_SAMPLE_TAG_INDEX_END].ta_Tag = TAG_END;
  sample_tags[STREAMING_AUDIO_SAMPLE_TAG_INDEX_END].ta_Arg = NULL;

  buffer->sample_item = CreateSample(sample_tags);
  if(buffer->sample_item < 0)
    return 0;

  buffer->attachment_item = AttachSample(channel->channel_instrument, buffer->sample_item, NULL);
  if(buffer->attachment_item < 0)
    return 0;

  attachment_tags[STREAMING_AUDIO_ATTACHMENT_TAG_INDEX_FLAGS].ta_Tag = AF_TAG_SET_FLAGS;
  attachment_tags[STREAMING_AUDIO_ATTACHMENT_TAG_INDEX_FLAGS].ta_Arg = (void *) AF_ATTF_NOAUTOSTART;
  attachment_tags[STREAMING_AUDIO_ATTACHMENT_TAG_INDEX_END].ta_Tag   = TAG_END;
  attachment_tags[STREAMING_AUDIO_ATTACHMENT_TAG_INDEX_END].ta_Arg   = NULL;
  status = SetAudioItemInfo(buffer->attachment_item, attachment_tags);
  if(status < 0)
    return 0;

  buffer->cue_item = CreateSizedItem(MKNODEID(AUDIONODE, AUDIO_CUE_NODE), NULL, 0);
  if(buffer->cue_item < 0)
    return 0;

  status = MonitorAttachment(buffer->attachment_item, buffer->cue_item, CUE_AT_END);
  if(status < 0)
    return 0;

  buffer->signal       = GetCueSignal(buffer->cue_item);
  channel->signal_mask = (channel->signal_mask | buffer->signal);
  return 1;
}
