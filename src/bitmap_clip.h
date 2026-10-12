#pragma once

#include "display_buffer_state.h"

#include "graphics.h"
#include "types.h"

#define DISPLAY_BITMAP_COUNT 2
#define DISPLAY_BITMAP_SLOT_COUNT 6

typedef struct BitmapDisplayState
{
  int32   bitmap_count;
  int32   current_bitmap_index;
  int32   vram_page_count;
  int32   bitmap_bytes;
  Item    display_resource_items[DISPLAY_BITMAP_SLOT_COUNT];
  Item    bitmap_items[DISPLAY_BITMAP_SLOT_COUNT];
  Bitmap *bitmap_objects[DISPLAY_BITMAP_SLOT_COUNT];
} BitmapDisplayState;

extern BitmapDisplayState g_BITMAP_DISPLAY_STATE;

void
configure_bitmap_display_state(BitmapDisplayState *display_state_);
void
configure_display_clip_rectangle(int32 left_, int32 right_, int32 top_, int32 bottom_);
void
configure_display_buffer_clip_rectangle(DisplayBufferDescriptor *descriptor_,
                                        int32                    left_,
                                        int32                    right_,
                                        int32                    top_,
                                        int32                    bottom_);
int32
initialize_bitmap_display_state(void);
void
open_bitmap_display_items(void);
void
open_render_thread_resources(void);
void
close_bitmap_display_items(void);
void
close_render_thread_resources(void);
Err
reset_current_bitmap_clip(void);
Err
set_current_bitmap_clip(int32 left_, int32 top_, int32 width_, int32 height_);
Err
draw_current_bitmap_cels(CCB *ccb_);
Err
draw_bitmap_cels(Item bitmap_item_, CCB *ccb_);
