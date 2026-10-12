#pragma once

#include "graphics.h"

extern GrafCon g_BITMAP_TEXT_GRAPHICS_CONTEXT;

void
set_bitmap_text_pen_color(Color color_);
void
draw_bitmap_text_at(uint8 *text_, Coord x_, Coord y_);
void
draw_bitmap_text(uint8 *text_);
