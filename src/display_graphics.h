#pragma once

#include "display_buffer_state.h"

#define DISPLAY_GRAPHICS_MAX_BUFFER_COUNT 4

extern int32 g_DISPLAY_BUFFER_COUNT;
extern int32 g_DISPLAY_CONFIGURATION_MODE;
extern int32 g_VRAM_PAGE_BYTES;
extern int32 g_VRAM_PAGE_SHIFT;
extern int32 g_DISPLAY_WIDTH;
extern int32 g_DISPLAY_HEIGHT;
extern int32 g_DISPLAY_HORIZONTAL_INSET;
extern int32 g_DISPLAY_VERTICAL_INSET;
extern int32 g_DISPLAY_CONTENT_WIDTH;
extern int32 g_DISPLAY_CONTENT_HEIGHT;
extern int32 g_DISPLAY_FIELD_FREQUENCY;

void
initialize_graphics(int32 width_, int32 height_, int32 buffer_count_);
void
initialize_default_graphics(int32 width_, int32 height_);
