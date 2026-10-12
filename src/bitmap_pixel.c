#include "bitmap_pixel.h"

#include "ccb_manager.h"
#include "display_buffer_state.h"
#include "memory.h"

#define CCB_BACKGROUND_DISABLE_FLAG 0x10000000
#define CCB_FIXED_ONE 0x00010000
#define CCB_PREAMBLE_WORD_OFFSET_FLAG 0x00001000
#define CCB_ROW_PREAMBLE 1
#define DISPLAY_BUFFER_DESCRIPTOR_CEL_MODE_WORD 9
#define DISPLAY_BUFFER_DESCRIPTOR_ROW_TABLE_WORD 10
#define DISPLAY_BUFFER_DESCRIPTOR_BASE_OFFSET_WORD 11
#define CCB_POSITION_SHIFT 16
#define PACKED_CEL_ROW_BITS 32
#define PACKED_CEL_ROW_BYTES 4
#define PACKED_SOURCE_BYTE_BITS 8
#define PACKED_SOURCE_HIGH_BIT 0x80

uint16       g_BITMAP_TEXT_PALETTE[2] = { 0, 0 };
uint32       g_HIGH_BIT_MASKS[33] = { 0x00000000, 0x80000000, 0xC0000000, 0xE0000000, 0xF0000000,
                                      0xF8000000, 0xFC000000, 0xFE000000, 0xFF000000, 0xFF800000,
                                      0xFFC00000, 0xFFE00000, 0xFFF00000, 0xFFF80000, 0xFFFC0000,
                                      0xFFFE0000, 0xFFFF0000, 0xFFFF8000, 0xFFFFC000, 0xFFFFE000,
                                      0xFFFFF000, 0xFFFFF800, 0xFFFFFC00, 0xFFFFFE00, 0xFFFFFF00,
                                      0xFFFFFF80, 0xFFFFFFC0, 0xFFFFFFE0, 0xFFFFFFF0, 0xFFFFFFF8,
                                      0xFFFFFFFC, 0xFFFFFFFE, 0xFFFFFFFF };
typedef char BitmapTextPaletteSize[(sizeof(g_BITMAP_TEXT_PALETTE) == 4) ? 1 : -1];
typedef char HighBitMasksSize[(sizeof(g_HIGH_BIT_MASKS) == 132) ? 1 : -1];

const uint8 *
draw_1_bit_bitmap_row(int32 x_, int32 y_, const uint8 *source_, int32 pixel_count_)
{
  int32      bit;
  int32      chunk_bits;
  RenderCcb *ccb;
  uint32     packed_pixels;
  uint8      pixels;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_CEL_MODE_WORD] == 0)
    {
      while(pixel_count_ > 0)
        {
          pixels = *source_;
          for(bit = 0; bit < PACKED_SOURCE_BYTE_BITS; ++bit)
            {
              if((pixels & (PACKED_SOURCE_HIGH_BIT >> bit)) != 0)
                draw_clipped_bitmap_pixel(x_, y_, g_BITMAP_TEXT_PALETTE[1]);

              x_++;
            }

          source_++;
          pixel_count_ -= PACKED_SOURCE_BYTE_BITS;
        }

      return source_;
    }

  while(pixel_count_ > 0)
    {
      chunk_bits = pixel_count_;
      if(chunk_bits > PACKED_CEL_ROW_BITS)
        chunk_bits = PACKED_CEL_ROW_BITS;

      move_bytes(source_, (uint8 *) &packed_pixels, PACKED_CEL_ROW_BYTES);
      packed_pixels &= g_HIGH_BIT_MASKS[chunk_bits];
      if(packed_pixels != 0)
        {
          ccb = allocate_template_ccb();
          ccb->fields.flags &= ~CCB_BACKGROUND_DISABLE_FLAG;
          ccb->fields.preamble_0 = CCB_ROW_PREAMBLE;
          ccb->fields.preamble_1 = ((uint32) (chunk_bits - 1) | CCB_PREAMBLE_WORD_OFFSET_FLAG);
          ccb->fields.palette    = g_BITMAP_TEXT_PALETTE;
          ccb->fields.x_position += (x_ * CCB_FIXED_ONE);
          ccb->fields.y_position += (y_ * CCB_FIXED_ONE);
          move_bytes((const uint8 *) &packed_pixels, (uint8 *) &ccb->width, PACKED_CEL_ROW_BYTES);
          finalize_ccb_batch(ccb);
        }

      pixel_count_ -= PACKED_CEL_ROW_BITS;
      x_ += PACKED_CEL_ROW_BITS;
      source_ += ((chunk_bits + PACKED_SOURCE_BYTE_BITS - 1) / PACKED_SOURCE_BYTE_BITS);
    }

  return source_;
}

void
draw_clipped_bitmap_pixel(int32 x_, int32 y_, uint32 color_)
{
  uint8     *destination;
  uint32    *row_offsets;
  RenderCcb *ccb;

  if(x_ < (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_X_WORD])
    return;
  if(x_ >= (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_WORD])
    return;
  if(y_ < (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_Y_WORD])
    return;
  if(y_ >=
     (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_HEIGHT_WORD])
    return;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_CEL_MODE_WORD] != 0)
    {
      ccb = allocate_template_ccb();
      ccb->fields.flags &= ~CCB_BACKGROUND_DISABLE_FLAG;
      ccb->width = (int32) (color_ << CCB_POSITION_SHIFT);
      ccb->fields.x_position += (x_ * CCB_FIXED_ONE);
      ccb->fields.y_position += (y_ * CCB_FIXED_ONE);
      finalize_ccb_batch(ccb);
      return;
    }

  row_offsets =
    (uint32 *) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_ROW_TABLE_WORD];
  destination =
    (uint8 *)
      g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_BASE_OFFSET_WORD] +
    row_offsets[y_];
  destination[x_] = (uint8) color_;
}
