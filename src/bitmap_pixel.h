#pragma once

#include "types.h"

extern uint16 g_BITMAP_TEXT_PALETTE[2];
extern uint32 g_HIGH_BIT_MASKS[33];

const uint8 *
draw_1_bit_bitmap_row(int32 x_, int32 y_, const uint8 *source_, int32 pixel_count_);
void
draw_clipped_bitmap_pixel(int32 x_, int32 y_, uint32 color_);
