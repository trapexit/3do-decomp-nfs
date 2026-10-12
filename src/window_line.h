#pragma once

#include "types.h"

int32
clip_window_line(int32 *start_x_, int32 *start_y_, int32 *end_x_, int32 *end_y_);
void
draw_window_line(int32 start_x_, int32 start_y_, int32 end_x_, int32 end_y_, uint32 value_);
