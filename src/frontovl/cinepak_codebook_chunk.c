#include "types.h"

#define CINEPAK_FULL_DETAIL_CODEBOOK_CHUNK 0x20
#define CINEPAK_SELECTIVE_DETAIL_CODEBOOK_CHUNK 0x21
#define CINEPAK_FULL_SMOOTH_CODEBOOK_CHUNK 0x22
#define CINEPAK_SELECTIVE_SMOOTH_CODEBOOK_CHUNK 0x23
#define CINEPAK_CHUNK_HEADER_BYTE_COUNT 4
#define CINEPAK_CODE_VECTOR_BYTE_COUNT 6
#define CINEPAK_SELECTIVE_VECTOR_COUNT 256
#define CINEPAK_DETAIL_VECTOR_WORD_COUNT 2
#define CINEPAK_SMOOTH_VECTOR_WORD_COUNT 8
#define CINEPAK_FIRST_SELECTION_BIT 0x80000000UL
#define CINEPAK_CHUNK_SIZE_MASK 0x00FFFFFFUL
#define CINEPAK_CODEBOOK_KIND_MASK 3

typedef void (*CinepakVectorExpander)(const uint8 **source_,
                                      uint32       *destination_,
                                      int32         count_,
                                      const uint32 *colors_,
                                      const uint32 *dither_,
                                      int32         dithered_);

extern void expand_cinepak_detail_code_vectors(const uint8 **source_,
                                                uint32       *destination_,
                                                int32         count_,
                                                const uint32 *colors_,
                                                const uint32 *dither_,
                                                int32         dithered_);
extern void expand_cinepak_smooth_code_vectors(const uint8 **source_,
                                                uint32       *destination_,
                                                int32         count_,
                                                const uint32 *colors_,
                                                const uint32 *dither_,
                                                int32         dithered_);
extern unsigned long read_cinepak_chunk_word(const unsigned char *bytes_);
extern unsigned long read_cinepak_frame_word(const unsigned char *bytes_);

void
expand_cinepak_codebook_chunk(const uint8 *chunk_,
                               uint32       *destination_,
                               int32         smooth_,
                               const uint32 *colors_,
                               const uint32 *dither_,
                               int32         dithered_)
{
  CinepakVectorExpander expander;
  uint32                chunk_word;
  uint32                selection_bits;
  uint32                selection_mask;
  int32                 chunk_data_byte_count;
  int32                 destination_word_count;
  int32                 vector_count;
  int32                 vector_index;
  uint8                 chunk_type;

  chunk_type = *chunk_;
  if((smooth_ & CINEPAK_CODEBOOK_KIND_MASK) == 0)
    {
      expander = expand_cinepak_detail_code_vectors;
      destination_word_count = CINEPAK_DETAIL_VECTOR_WORD_COUNT;
    }
  else
    {
      expander = expand_cinepak_smooth_code_vectors;
      destination_word_count = CINEPAK_SMOOTH_VECTOR_WORD_COUNT;
    }

  if((chunk_type == CINEPAK_FULL_DETAIL_CODEBOOK_CHUNK) ||
     (chunk_type == CINEPAK_FULL_SMOOTH_CODEBOOK_CHUNK))
    {
      chunk_word = read_cinepak_chunk_word(chunk_);
      chunk_ += CINEPAK_CHUNK_HEADER_BYTE_COUNT;
      chunk_data_byte_count = ((chunk_word & CINEPAK_CHUNK_SIZE_MASK) -
                               CINEPAK_CHUNK_HEADER_BYTE_COUNT);
      if(chunk_data_byte_count <= 0)
        return;

      vector_count = (chunk_data_byte_count / CINEPAK_CODE_VECTOR_BYTE_COUNT);
      expander(&chunk_,
               destination_,
               vector_count,
               colors_,
               dither_,
               dithered_);
      return;
    }

  if((chunk_type == CINEPAK_SELECTIVE_DETAIL_CODEBOOK_CHUNK) ||
     (chunk_type == CINEPAK_SELECTIVE_SMOOTH_CODEBOOK_CHUNK))
    {
      chunk_word = read_cinepak_chunk_word(chunk_);
      chunk_ += CINEPAK_CHUNK_HEADER_BYTE_COUNT;
      chunk_data_byte_count = ((chunk_word & CINEPAK_CHUNK_SIZE_MASK) -
                               CINEPAK_CHUNK_HEADER_BYTE_COUNT);
      if(chunk_data_byte_count <= 0)
        return;

      selection_bits = 0;
      selection_mask = 0;
      for(vector_index = 0; vector_index < CINEPAK_SELECTIVE_VECTOR_COUNT;
          vector_index += 1)
        {
          if(selection_mask == 0)
            {
              selection_bits = read_cinepak_frame_word(chunk_);
              chunk_ += CINEPAK_CHUNK_HEADER_BYTE_COUNT;
              selection_mask = CINEPAK_FIRST_SELECTION_BIT;
            }
          if((selection_bits & selection_mask) != 0)
            {
              expander(&chunk_,
                       destination_,
                       1,
                       colors_,
                       dither_,
                       dithered_);
            }
          destination_ += destination_word_count;
          selection_mask >>= 1;
        }
      return;
    }

  chunk_word = read_cinepak_chunk_word(chunk_);
  chunk_ += (chunk_word & CINEPAK_CHUNK_SIZE_MASK);
}
