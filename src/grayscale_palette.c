#include "grayscale_palette.h"

#include "display_buffer_state.h"
#include "display_clut.h"
#include "display_graphics.h"
#include "frontend_input_wait.h"
#include "memory.h"

#define RGB_GREEN_BYTE 1
#define RGB_BLUE_BYTE 2
#define LINEAR_GRAY_SHIFT 3
#define LINEAR_GREEN_BIAS 4
#define PALETTE_ROUNDING_DIVISOR 2
#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

int32 g_USE_GAMMA_GRAYSCALE                         = 0;
uint8 g_GAMMA_GRAYSCALE_RAMP[GRAYSCALE_COLOR_COUNT] = {
  0x00, 0x08, 0x0F, 0x17, 0x1F, 0x25, 0x2C, 0x32, 0x39, 0x3F, 0x45, 0x4B, 0x51, 0x57, 0x5D, 0x63,
  0x6A, 0x71, 0x78, 0x7F, 0x87, 0x8F, 0x98, 0xA0, 0xAB, 0xB5, 0xBF, 0xCA, 0xD8, 0xE4, 0xF1, 0xFF
};
COMPILE_TIME_SIZE_ASSERT(gamma_grayscale_ramp_size, sizeof(g_GAMMA_GRAYSCALE_RAMP) == 32);

void
build_grayscale_rgb_palette(uint8 *rgb_palette_)
{
  int32 index;
  int32 output_offset;
  uint8 intensity;

  output_offset = 0;
  if(g_USE_GAMMA_GRAYSCALE != 0)
    {
      for(index = 0; index < GRAYSCALE_COLOR_COUNT; index++)
        {
          intensity                                    = g_GAMMA_GRAYSCALE_RAMP[index];
          rgb_palette_[output_offset]                  = intensity;
          rgb_palette_[output_offset + RGB_GREEN_BYTE] = intensity;
          rgb_palette_[output_offset + RGB_BLUE_BYTE]  = intensity;
          output_offset += GRAYSCALE_RGB_COLOR_BYTES;
        }

      return;
    }

  for(index = 0; index < GRAYSCALE_COLOR_COUNT; index++)
    {
      intensity                                    = (uint8) (index << LINEAR_GRAY_SHIFT);
      rgb_palette_[output_offset]                  = intensity;
      rgb_palette_[output_offset + RGB_GREEN_BYTE] = (uint8) (intensity + LINEAR_GREEN_BIAS);
      rgb_palette_[output_offset + RGB_BLUE_BYTE]  = intensity;
      output_offset += GRAYSCALE_RGB_COLOR_BYTES;
    }
}

void
apply_linear_grayscale_clut(void)
{
  uint8 grayscale_palette[GRAYSCALE_RGB_PALETTE_BYTES];
  int32 buffer_index;

  g_USE_GAMMA_GRAYSCALE = 0;
  build_grayscale_rgb_palette(grayscale_palette);
  for(buffer_index = 0; buffer_index < g_DISPLAY_BUFFER_COUNT; ++buffer_index)
    {
      set_display_clut(&g_DISPLAY_BUFFER_DESCRIPTORS[buffer_index], grayscale_palette);
    }
}

void
apply_gamma_grayscale_clut(void)
{
  uint8 grayscale_palette[GRAYSCALE_RGB_PALETTE_BYTES];
  int32 buffer_index;

  g_USE_GAMMA_GRAYSCALE = 1;
  build_grayscale_rgb_palette(grayscale_palette);
  for(buffer_index = 0; buffer_index < g_DISPLAY_BUFFER_COUNT; ++buffer_index)
    {
      set_display_clut(&g_DISPLAY_BUFFER_DESCRIPTORS[buffer_index], grayscale_palette);
    }
}

void
transition_display_clut(const uint8 *source_palette_,
                        const uint8 *target_palette_,
                        int32        frame_count_)
{
  uint8  interpolated_palette[GRAYSCALE_RGB_PALETTE_BYTES];
  uint32 weighted_value;
  int32  denominator;
  int32  remaining;
  int32  rounding;
  int32  buffer_index;
  int32  byte_index;
  int32  frame;

  if(frame_count_ > 0)
    {
      denominator = (frame_count_ - 1);
      rounding    = (denominator / PALETTE_ROUNDING_DIVISOR);
      for(frame = 0; frame < frame_count_; ++frame)
        {
          remaining = (denominator - frame);
          for(byte_index = 0; byte_index < GRAYSCALE_RGB_PALETTE_BYTES; ++byte_index)
            {
              weighted_value =
                (((uint32) frame * target_palette_[byte_index]) +
                 ((uint32) remaining * source_palette_[byte_index]) + (uint32) rounding);
              interpolated_palette[byte_index] = (uint8) (weighted_value / (uint32) denominator);
            }

          set_display_clut(&g_DISPLAY_BUFFER_DESCRIPTORS[g_CURRENT_DISPLAY_BUFFER_INDEX],
                           interpolated_palette);
          wait_for_frontend_input();
        }
    }

  for(buffer_index = 0; buffer_index < g_DISPLAY_BUFFER_COUNT; ++buffer_index)
    {
      set_display_clut(&g_DISPLAY_BUFFER_DESCRIPTORS[buffer_index], target_palette_);
    }
}

void
fade_display_clut_from_black(int32 legacy_start_, int32 legacy_count_, int32 frame_count_)
{
  uint8 black_palette[GRAYSCALE_RGB_PALETTE_BYTES];
  uint8 grayscale_palette[GRAYSCALE_RGB_PALETTE_BYTES];

  (void) legacy_start_;
  (void) legacy_count_;
  build_grayscale_rgb_palette(grayscale_palette);
  clear_bytes(black_palette, GRAYSCALE_RGB_PALETTE_BYTES);
  transition_display_clut(black_palette, grayscale_palette, frame_count_);
}

void
fade_display_clut_to_black(int32 legacy_start_, int32 legacy_count_, int32 frame_count_)
{
  uint8 black_palette[GRAYSCALE_RGB_PALETTE_BYTES];
  uint8 grayscale_palette[GRAYSCALE_RGB_PALETTE_BYTES];

  (void) legacy_start_;
  (void) legacy_count_;
  build_grayscale_rgb_palette(grayscale_palette);
  clear_bytes(black_palette, GRAYSCALE_RGB_PALETTE_BYTES);
  transition_display_clut(grayscale_palette, black_palette, frame_count_);
}
