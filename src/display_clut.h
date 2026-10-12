#pragma once

#include "display_buffer_state.h"

#include "kernel.h"

void
install_simple_vdl(Item         screen_item_,
                   uint8       *bitmap_buffer_,
                   const uint8 *rgb_palette_,
                   int32        display_now_);
void
set_display_clut(const DisplayBufferDescriptor *descriptor_, const uint8 *rgb_palette_);
