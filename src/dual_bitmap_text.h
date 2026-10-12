#pragma once

#include "graphics.h"

extern GrafCon g_DUAL_BITMAP_TEXT_GRAPHICS_CONTEXT;

void
set_dual_bitmap_text_pen_color(Color color_);
void
draw_dual_bitmap_text_at(uint8 *text_, Coord x_, Coord y_);
void
draw_dual_bitmap_text(uint8 *text_);
