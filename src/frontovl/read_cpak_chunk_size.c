#include "cpak.h"
#include "tracked_memory.h"

uint32
read_cpak_chunk_size(const uint8 chunk_[])
{
  return ((uint32) chunk_[CPAK_SIZE_HIGH_BYTE] << CPAK_SIZE_HIGH_SHIFT) |
         ((uint32) chunk_[CPAK_SIZE_MIDDLE_BYTE] << CPAK_SIZE_MIDDLE_SHIFT) |
         (uint32) chunk_[CPAK_SIZE_LOW_BYTE];
}

void
copy_cpak_frame(const uint32 source_[], uint32 destination_[])
{
  int32 word;

  for(word = 0; word < CPAK_FRAME_WORD_COUNT; ++word)
    {
      destination_[word] = source_[word];
    }
}

CinepakCodecState *
create_cinepak_codec(void *handler_, uint32 option_)
{
  CinepakCodecState *codec;

  codec = (CinepakCodecState *) malloc(sizeof(CinepakCodecState));
  if(codec != NULL)
    {
      codec->magic   = CINEPAK_CODEC_MAGIC;
      codec->option  = option_;
      codec->handler = handler_;
    }
  return codec;
}

void
delete_cinepak_codec(CinepakCodecState *codec_)
{
  if(codec_->magic == CINEPAK_CODEC_MAGIC)
    {
      free(codec_);
    }
}
