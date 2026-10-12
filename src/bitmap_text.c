#include "bitmap_text.h"

#include "bitmap_clip.h"


GrafCon g_BITMAP_TEXT_GRAPHICS_CONTEXT;

void
set_bitmap_text_pen_color(Color color_)
{
  SetBGPen(&g_BITMAP_TEXT_GRAPHICS_CONTEXT, color_);
  SetFGPen(&g_BITMAP_TEXT_GRAPHICS_CONTEXT, color_);
}

void
draw_bitmap_text_at(uint8 *text_, Coord x_, Coord y_)
{
  MoveTo(&g_BITMAP_TEXT_GRAPHICS_CONTEXT, x_, y_);
  (void) DrawText8(&g_BITMAP_TEXT_GRAPHICS_CONTEXT,
                   g_BITMAP_DISPLAY_STATE.bitmap_items[g_BITMAP_DISPLAY_STATE.current_bitmap_index],
                   text_);
}

void
draw_bitmap_text(uint8 *text_)
{
  (void) DrawText8(&g_BITMAP_TEXT_GRAPHICS_CONTEXT,
                   g_BITMAP_DISPLAY_STATE.bitmap_items[g_BITMAP_DISPLAY_STATE.current_bitmap_index],
                   text_);
}
