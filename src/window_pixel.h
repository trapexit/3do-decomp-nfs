#pragma once

#include "types.h"

uint32
read_window_pixel(int32 x_, int32 y_);
uint32
read_clipped_window_pixel(int32 x_, int32 y_);
void
write_window_pixel(int32 x_, int32 y_, uint32 value_);
