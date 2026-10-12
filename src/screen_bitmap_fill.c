#include "screen_bitmap_fill.h"

#include "kernel.h"

void
fill_screen_bitmap(Item screen_item_, Color color_)
{
  GrafCon graphics_context;
  Rect    rectangle;
  Bitmap *bitmap;
  Screen *screen;

  SetFGPen(&graphics_context, color_);
  screen                 = (Screen *) LookupItem(screen_item_);
  bitmap                 = screen->scr_TempBitmap;
  rectangle.rect_XLeft   = 0;
  rectangle.rect_YTop    = 0;
  rectangle.rect_XRight  = bitmap->bm_Width;
  rectangle.rect_YBottom = bitmap->bm_Height;
  (void) FillRect(bitmap->bm.n_Item, &graphics_context, &rectangle);
}
