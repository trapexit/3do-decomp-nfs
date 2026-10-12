#pragma once

#include "window_memory.h"

#include "types.h"

extern int32 g_DISTANCE_SLICE_SAVE_PENDING;
extern int32 g_DISTANCE_SLICE_FILE_INDEX;

void
initialize_distance_slice_window(void);
void
free_distance_slice_window(void);
int32
compress_distance_slice_window(Window *window_, int32 byte_count_);
void
save_distance_slice_files(void);
uint32
scan_distance_slice_row(int32 y_);
uint32
scan_distance_slice_column(int32 x_);
void
render_distance_slice_window(void);
void
complete_distance_slice_capture(void);
