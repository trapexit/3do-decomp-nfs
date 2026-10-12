/* Frontovl Cinepak frame codebook preparation. */

#include "cinepak_codebook.h"

#include "byte_range.h"
#include "cinepak_word_reader.h"

#define CINEPAK_CHUNK_SIZE_MASK 0x00FFFFFF
#define CINEPAK_STRIP_HEADER_BYTE_COUNT 12
#define CINEPAK_FRAME_HEADER_BYTE_COUNT 10
#define CINEPAK_STRIP_BYTE_COUNT 0x2800
#define CINEPAK_SMOOTH_CODEBOOK_OFFSET 0x800
#define CINEPAK_HANDLER_COLOR_TABLE_OFFSET 0x3100
#define CINEPAK_HANDLER_DITHER_TABLE_OFFSET 0x100
#define CINEPAK_CODEBOOK_FULL_DETAIL 0x20
#define CINEPAK_CODEBOOK_PARTIAL_DETAIL 0x21
#define CINEPAK_CODEBOOK_FULL_SMOOTH 0x22
#define CINEPAK_CODEBOOK_PARTIAL_SMOOTH 0x23
#define CINEPAK_STRIP_KEY 0x10
#define CINEPAK_STRIP_DIFFERENCE 0x11
#define CINEPAK_FRAME_KEY 0
#define CINEPAK_FRAME_DIFFERENCE 1
#define CINEPAK_OUTPUT_STANDARD 0
#define CINEPAK_OUTPUT_DITHERED 1
#define CINEPAK_STRIP_COUNT_HIGH_INDEX 8
#define CINEPAK_STRIP_COUNT_LOW_INDEX 9
#define CINEPAK_STRIP_COUNT_HIGH_SHIFT 8

extern void
expand_cinepak_codebook_chunk(const uint8  *chunk_,
                              uint32       *destination_,
                              int32         smooth_,
                              const uint32 *color_table_,
                              const uint32 *dither_table_,
                              int32         dithered_);

static int32
_prepare_cinepak_frame_codebooks(CinepakCodecState *codec_, const uint8 *frame_, int32 dithered_)
{
  const uint8  *chunk;
  const uint8  *frame_end;
  const uint8  *strip_end;
  const uint32 *color_table;
  const uint32 *dither_table;
  int32         strip;
  int32         strip_count;
  uint32        chunk_size;
  uint32       *detail_codebook;
  uint32       *smooth_codebook;
  uint8        *handler;

  if((frame_[0] != CINEPAK_FRAME_KEY) && (frame_[0] != CINEPAK_FRAME_DIFFERENCE))
    return -1;

  handler      = (uint8 *) codec_->handler;
  color_table  = (const uint32 *) (handler + CINEPAK_HANDLER_COLOR_TABLE_OFFSET);
  dither_table = (const uint32 *) (handler + CINEPAK_HANDLER_DITHER_TABLE_OFFSET);
  chunk_size   = read_cinepak_chunk_word(frame_) & CINEPAK_CHUNK_SIZE_MASK;
  frame_end    = frame_ + chunk_size;
  strip_count = ((int32) frame_[CINEPAK_STRIP_COUNT_HIGH_INDEX] << CINEPAK_STRIP_COUNT_HIGH_SHIFT) |
                frame_[CINEPAK_STRIP_COUNT_LOW_INDEX];
  chunk       = frame_ + CINEPAK_FRAME_HEADER_BYTE_COUNT;

  for(strip = 0; strip < strip_count; ++strip)
    {
      while((chunk[0] != CINEPAK_STRIP_KEY) && (chunk[0] != CINEPAK_STRIP_DIFFERENCE))
        {
          chunk_size = read_cinepak_chunk_word(chunk) & CINEPAK_CHUNK_SIZE_MASK;
          chunk += chunk_size;
          if(chunk >= frame_end)
            return -1;
        }

      chunk_size = read_cinepak_chunk_word(chunk) & CINEPAK_CHUNK_SIZE_MASK;
      strip_end  = chunk + chunk_size;
      if((strip != 0) && (frame_[0] == CINEPAK_FRAME_KEY) && (chunk[0] == CINEPAK_STRIP_DIFFERENCE))
        {
          copy_byte_range_forward(codec_->data + ((strip - 1) * CINEPAK_STRIP_BYTE_COUNT),
                                  codec_->data + (strip * CINEPAK_STRIP_BYTE_COUNT),
                                  CINEPAK_STRIP_BYTE_COUNT);
        }
      chunk += CINEPAK_STRIP_HEADER_BYTE_COUNT;
      detail_codebook = (uint32 *) (codec_->data + (strip * CINEPAK_STRIP_BYTE_COUNT));
      smooth_codebook = (uint32 *) ((uint8 *) detail_codebook + CINEPAK_SMOOTH_CODEBOOK_OFFSET);

      while(chunk < strip_end)
        {
          if((chunk[0] == CINEPAK_CODEBOOK_FULL_DETAIL) ||
             (chunk[0] == CINEPAK_CODEBOOK_PARTIAL_DETAIL))
            {
              expand_cinepak_codebook_chunk(chunk,
                                            detail_codebook,
                                            0,
                                            color_table,
                                            dither_table,
                                            dithered_);
            }
          else if((chunk[0] == CINEPAK_CODEBOOK_FULL_SMOOTH) ||
                  (chunk[0] == CINEPAK_CODEBOOK_PARTIAL_SMOOTH))
            {
              expand_cinepak_codebook_chunk(chunk,
                                            smooth_codebook,
                                            1,
                                            color_table,
                                            dither_table,
                                            dithered_);
            }
          chunk_size = read_cinepak_chunk_word(chunk) & CINEPAK_CHUNK_SIZE_MASK;
          chunk += chunk_size;
        }
    }
  return 0;
}

int32
pre_decompress_cinepak_frame(CinepakCodecState *codec_, const uint8 *frame_)
{
  return _prepare_cinepak_frame_codebooks(codec_, frame_, CINEPAK_OUTPUT_STANDARD);
}

int32
pre_decompress_cinepak_frame_24(CinepakCodecState *codec_, const uint8 *frame_)
{
  return _prepare_cinepak_frame_codebooks(codec_, frame_, CINEPAK_OUTPUT_DITHERED);
}
