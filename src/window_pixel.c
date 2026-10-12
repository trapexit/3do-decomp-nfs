#include "window_pixel.h"

#include "ccb_manager.h"
#include "display_buffer_state.h"

#include "graphics.h"

#define WINDOW_PIXEL_MODE_WORD 9
#define WINDOW_PIXEL_ROW_OFFSETS_WORD 10
#define WINDOW_PIXEL_BUFFER_WORD 11
#define WINDOW_PIXEL_BACKGROUND_DISABLE_FLAG 0x10000000
#define WINDOW_PIXEL_POSITION_SHIFT 16

uint32
read_window_pixel(int32 x_, int32 y_)
{
  GrafCon graphics_context;
  uint32 *row_offsets;
  uint8  *buffer;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[WINDOW_PIXEL_MODE_WORD] != 0)
    {
      return ReadPixel((Item) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT
                         .words[DISPLAY_BUFFER_DESCRIPTOR_BITMAP_ITEM_WORD],
                       &graphics_context,
                       x_,
                       y_);
    }

  row_offsets =
    (uint32 *) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[WINDOW_PIXEL_ROW_OFFSETS_WORD];
  buffer = (uint8 *) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[WINDOW_PIXEL_BUFFER_WORD];
  return buffer[row_offsets[y_] + x_];
}

uint32
read_clipped_window_pixel(int32 x_, int32 y_)
{
  if(x_ < (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_X_WORD])
    return 0;
  if(x_ >= (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_WORD])
    return 0;
  if(y_ < (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_Y_WORD])
    return 0;
  if(y_ >=
     (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_HEIGHT_WORD])
    return 0;

  return read_window_pixel(x_, y_);
}

void
write_window_pixel(int32 x_, int32 y_, uint32 value_)
{
  RenderCcb *ccb;
  uint32    *row_offsets;
  uint8     *buffer;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[WINDOW_PIXEL_MODE_WORD] != 0)
    {
      ccb = allocate_template_ccb();
      ccb->fields.flags &= ~WINDOW_PIXEL_BACKGROUND_DISABLE_FLAG;
      ccb->width = (int32) (value_ << WINDOW_PIXEL_POSITION_SHIFT);
      ccb->fields.x_position += x_ << WINDOW_PIXEL_POSITION_SHIFT;
      ccb->fields.y_position += y_ << WINDOW_PIXEL_POSITION_SHIFT;
      finalize_ccb_batch(ccb);
      return;
    }

  row_offsets =
    (uint32 *) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[WINDOW_PIXEL_ROW_OFFSETS_WORD];
  buffer = (uint8 *) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[WINDOW_PIXEL_BUFFER_WORD];
  buffer[row_offsets[y_] + x_] = (uint8) value_;
}
