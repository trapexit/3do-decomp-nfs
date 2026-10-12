#include "window_fill.h"

#include "display_buffer_state.h"
#include "display_graphics.h"
#include "memory.h"
#include "quad_render.h"
#include "vram_transfer.h"

#include "graphics.h"

#define WINDOW_FILL_ROW_BYTE_COUNT_WORD 0
#define WINDOW_FILL_MODE_WORD 9
#define WINDOW_FILL_ROW_OFFSETS_WORD 10
#define WINDOW_FILL_BUFFER_WORD 11
#define WINDOW_FILL_BITMAP_DATA_OFFSET 0x10
#define WINDOW_FILL_DUPLICATE_SHIFT 16

int32 g_FAST_VRAM_FILL_ENABLED = 1;

void
fill_display_window(uint32 value_)
{
  int32 height;
  int32 page_count;
  int32 width;
  int32 x;
  int32 y;

  if((g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_TYPE_WORD] ==
      DISPLAY_BUFFER_DESCRIPTOR_VIDEO_WINDOW_TYPE) &&
     (g_FAST_VRAM_FILL_ENABLED != 0))
    {
      x = (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_X_WORD];
      y = (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_Y_WORD];
      width =
        (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_WORD];
      height =
        (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_HEIGHT_WORD];
      if((x == 0) && (y == 0) &&
         (width == (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT
                     .words[DISPLAY_BUFFER_DESCRIPTOR_BUFFER_WIDTH_WORD]) &&
         (height == (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT
                      .words[DISPLAY_BUFFER_DESCRIPTOR_BUFFER_HEIGHT_WORD]))
        {
          page_count = (width * height) >> (g_VRAM_PAGE_SHIFT - 1);
          (void) SetVRAMPages(g_VRAM_IO_REQUEST,
                              (uint8 *) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT
                                  .words[DISPLAY_BUFFER_DESCRIPTOR_BITMAP_HEADER_WORD] +
                                WINDOW_FILL_BITMAP_DATA_OFFSET,
                              (int32) (value_ | (value_ << WINDOW_FILL_DUPLICATE_SHIFT)),
                              page_count,
                              -1);
          return;
        }
    }

  x = (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_X_WORD];
  y = (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_Y_WORD];
  width =
    (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_WORD] - x;
  height =
    (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_HEIGHT_WORD] - y;
  fill_window_rectangle(x, y, width, height, value_);
}

void
fill_window_rectangle(int32 x_, int32 y_, int32 width_, int32 height_, uint32 value_)
{
  uint32 *row_offsets;
  uint8  *row;
  int32   row_byte_count;

  if((width_ <= 0) || (height_ <= 0))
    return;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[WINDOW_FILL_MODE_WORD] != 0)
    {
      draw_solid_quad(value_, x_, y_, x_ + width_, y_, x_ + width_, y_ + height_, x_, y_ + height_);
      return;
    }

  row_offsets = (uint32 *) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[WINDOW_FILL_ROW_OFFSETS_WORD];
  row         = (uint8 *) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[WINDOW_FILL_BUFFER_WORD];
  row += row_offsets[y_] + x_;
  row_byte_count =
    (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[WINDOW_FILL_ROW_BYTE_COUNT_WORD];
  while(height_ > 0)
    {
      fill_bytes(row, width_, value_);
      row += row_byte_count;
      height_--;
    }
}

void
fill_clipped_window_rectangle(int32 x_, int32 y_, int32 width_, int32 height_, uint32 value_)
{
  int32 clip_amount;

  clip_amount =
    ((int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_Y_WORD] - y_);
  if(clip_amount > 0)
    {
      y_ += clip_amount;
      height_ -= clip_amount;
    }

  clip_amount =
    ((y_ + height_) -
     (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_HEIGHT_WORD]);
  if(clip_amount > 0)
    height_ -= clip_amount;

  clip_amount =
    ((int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_X_WORD] - x_);
  if(clip_amount > 0)
    {
      x_ += clip_amount;
      width_ -= clip_amount;
    }

  clip_amount =
    ((x_ + width_) -
     (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_WORD]);
  if(clip_amount > 0)
    width_ -= clip_amount;
  if((width_ <= 0) || (height_ <= 0))
    return;

  fill_window_rectangle(x_, y_, width_, height_, value_);
}
