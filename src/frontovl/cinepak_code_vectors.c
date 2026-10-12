#include "types.h"
#define CINEPAK_VECTOR_Y_2_INDEX 2
#define CINEPAK_VECTOR_Y_3_INDEX 3
#define CINEPAK_VECTOR_BLUE_CHROMA_INDEX 4
#define CINEPAK_VECTOR_RED_CHROMA_INDEX 5
#define CINEPAK_CODE_VECTOR_BYTE_COUNT 6
#define CINEPAK_CHROMA_SCALE 2
#define CINEPAK_CHROMA_HIGH_BYTE_SHIFT 8
#define CINEPAK_CHROMA_LOW_BYTE_MASK 0xFF
#define CINEPAK_BLUE_OFFSET_SHIFT 7
#define CINEPAK_GREEN_BLUE_SHIFT 2
#define CINEPAK_ALTERNATE_DITHER_OFFSET 2
#define CINEPAK_ALTERNATE_COLOR_OFFSET 4
#define CINEPAK_RED_COMPONENT_SHIFT 10
#define CINEPAK_GREEN_COMPONENT_SHIFT 5
#define CINEPAK_PIXEL_PAIR_SHIFT 16
#define CINEPAK_DESTINATION_WORD_COUNT 2
#define CINEPAK_SMOOTH_LUMINANCE_COUNT 4

void
expand_cinepak_detail_code_vectors(const uint8 **source_,
                                    uint32       *destination_,
                                    int32         count_,
                                    const uint32 *colors_,
                                    const uint32 *dither_,
                                    int32         dithered_)
{
  const uint8 *source;
  uint32       pixel_0;
  uint32       pixel_1;
  uint32       pixel_2;
  uint32       pixel_3;
  uint32       chroma_word;
  int32        red_chroma;
  int32        blue_offset;
  int32        green_offset;
  int32        red_offset;
  int32        vector_index;
  int32        y_0;
  int32        y_1;
  int32        y_2;
  int32        y_3;

  source = *source_;
  for(vector_index = 0; vector_index < count_; vector_index += 1)
    {
      y_0         = source[0];
      y_1         = source[1];
      y_2         = source[CINEPAK_VECTOR_Y_2_INDEX];
      y_3         = source[CINEPAK_VECTOR_Y_3_INDEX];
      chroma_word = (((uint32) source[CINEPAK_VECTOR_BLUE_CHROMA_INDEX]
                      << CINEPAK_CHROMA_HIGH_BYTE_SHIFT) |
                     source[CINEPAK_VECTOR_RED_CHROMA_INDEX]);
      red_chroma = (signed char) (chroma_word & CINEPAK_CHROMA_LOW_BYTE_MASK);
      source += CINEPAK_CODE_VECTOR_BYTE_COUNT;

      red_offset   = (red_chroma * CINEPAK_CHROMA_SCALE);
      blue_offset  = ((int32) (int16) chroma_word >> CINEPAK_BLUE_OFFSET_SHIFT);
      green_offset = -((blue_offset >> CINEPAK_GREEN_BLUE_SHIFT) +
                       (red_offset >> 1));

      if(dithered_ != 0)
        {
          pixel_0 = (dither_[y_0 + green_offset] |
                     (colors_[y_0 + red_offset] << CINEPAK_RED_COMPONENT_SHIFT) |
                     colors_[y_0 + blue_offset]);
          pixel_1 = (dither_[y_1 + green_offset + CINEPAK_ALTERNATE_DITHER_OFFSET] |
                     (colors_[y_1 + red_offset + CINEPAK_ALTERNATE_COLOR_OFFSET]
                      << CINEPAK_RED_COMPONENT_SHIFT) |
                     colors_[y_1 + blue_offset + CINEPAK_ALTERNATE_COLOR_OFFSET]);
          pixel_2 = (dither_[y_2 + green_offset + CINEPAK_ALTERNATE_DITHER_OFFSET] |
                     (colors_[y_2 + red_offset + CINEPAK_ALTERNATE_COLOR_OFFSET]
                      << CINEPAK_RED_COMPONENT_SHIFT) |
                     colors_[y_2 + blue_offset + CINEPAK_ALTERNATE_COLOR_OFFSET]);
          pixel_3 = (dither_[y_3 + green_offset] |
                     (colors_[y_3 + red_offset] << CINEPAK_RED_COMPONENT_SHIFT) |
                     colors_[y_3 + blue_offset]);
        }
      else
        {
          pixel_0 = ((colors_[y_0 + red_offset] << CINEPAK_RED_COMPONENT_SHIFT) |
                     (colors_[y_0 + green_offset] << CINEPAK_GREEN_COMPONENT_SHIFT) |
                     colors_[y_0 + blue_offset]);
          pixel_1 =
            ((colors_[y_1 + red_offset + CINEPAK_ALTERNATE_COLOR_OFFSET]
              << CINEPAK_RED_COMPONENT_SHIFT) |
             (colors_[y_1 + green_offset + CINEPAK_ALTERNATE_COLOR_OFFSET]
              << CINEPAK_GREEN_COMPONENT_SHIFT) |
             colors_[y_1 + blue_offset + CINEPAK_ALTERNATE_COLOR_OFFSET]);
          pixel_2 =
            ((colors_[y_2 + red_offset + CINEPAK_ALTERNATE_COLOR_OFFSET]
              << CINEPAK_RED_COMPONENT_SHIFT) |
             (colors_[y_2 + green_offset + CINEPAK_ALTERNATE_COLOR_OFFSET]
              << CINEPAK_GREEN_COMPONENT_SHIFT) |
             colors_[y_2 + blue_offset + CINEPAK_ALTERNATE_COLOR_OFFSET]);
          pixel_3 = ((colors_[y_3 + red_offset] << CINEPAK_RED_COMPONENT_SHIFT) |
                     (colors_[y_3 + green_offset] << CINEPAK_GREEN_COMPONENT_SHIFT) |
                     colors_[y_3 + blue_offset]);
        }

      destination_[0] = ((pixel_0 << CINEPAK_PIXEL_PAIR_SHIFT) | pixel_2);
      destination_[1] = ((pixel_1 << CINEPAK_PIXEL_PAIR_SHIFT) | pixel_3);
      destination_ += CINEPAK_DESTINATION_WORD_COUNT;
    }
  *source_ = source;
}


void
expand_cinepak_smooth_code_vectors(const uint8 **source_,
                                    uint32       *destination_,
                                    int32         count_,
                                    const uint32 *colors_,
                                    const uint32 *dither_,
                                    int32         dithered_)
{
  const uint8 *source;
  uint32       first_pixel;
  uint32       second_pixel;
  uint32       chroma_word;
  int32        red_chroma;
  int32        blue_offset;
  int32        green_offset;
  int32        red_offset;
  int32        vector_index;
  int32        y_index;
  int32        y;

  source = *source_;
  for(vector_index = 0; vector_index < count_; vector_index += 1)
    {
      chroma_word = (((uint32) source[CINEPAK_VECTOR_BLUE_CHROMA_INDEX]
                      << CINEPAK_CHROMA_HIGH_BYTE_SHIFT) |
                     source[CINEPAK_VECTOR_RED_CHROMA_INDEX]);
      red_chroma = (signed char) (chroma_word & CINEPAK_CHROMA_LOW_BYTE_MASK);
      red_offset   = (red_chroma * CINEPAK_CHROMA_SCALE);
      blue_offset  = ((int32) (int16) chroma_word >> CINEPAK_BLUE_OFFSET_SHIFT);
      green_offset = -((blue_offset >> CINEPAK_GREEN_BLUE_SHIFT) +
                       (red_offset >> 1));

      for(y_index = 0; y_index < CINEPAK_SMOOTH_LUMINANCE_COUNT; y_index += 1)
        {
          y = source[y_index];
          if(dithered_ != 0)
            {
              first_pixel = (dither_[y + green_offset] |
                             (colors_[y + red_offset] << CINEPAK_RED_COMPONENT_SHIFT) |
                             colors_[y + blue_offset]);
              second_pixel =
                (dither_[y + green_offset + CINEPAK_ALTERNATE_DITHER_OFFSET] |
                 (colors_[y + red_offset + CINEPAK_ALTERNATE_COLOR_OFFSET]
                  << CINEPAK_RED_COMPONENT_SHIFT) |
                 colors_[y + blue_offset + CINEPAK_ALTERNATE_COLOR_OFFSET]);
            }
          else
            {
              first_pixel = ((colors_[y + red_offset] << CINEPAK_RED_COMPONENT_SHIFT) |
                             (colors_[y + green_offset] << CINEPAK_GREEN_COMPONENT_SHIFT) |
                             colors_[y + blue_offset]);
              second_pixel =
                ((colors_[y + red_offset + CINEPAK_ALTERNATE_COLOR_OFFSET]
                  << CINEPAK_RED_COMPONENT_SHIFT) |
                 (colors_[y + green_offset + CINEPAK_ALTERNATE_COLOR_OFFSET]
                  << CINEPAK_GREEN_COMPONENT_SHIFT) |
                 colors_[y + blue_offset + CINEPAK_ALTERNATE_COLOR_OFFSET]);
            }

          destination_[0] = ((first_pixel << CINEPAK_PIXEL_PAIR_SHIFT) | second_pixel);
          destination_[1] = (first_pixel | (second_pixel << CINEPAK_PIXEL_PAIR_SHIFT));
          destination_ += CINEPAK_DESTINATION_WORD_COUNT;
        }
      source += CINEPAK_CODE_VECTOR_BYTE_COUNT;
    }
  *source_ = source;
}
