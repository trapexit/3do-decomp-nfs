#pragma once

#include "memory.h"

#include "item.h"
#include "types.h"

typedef struct Window
{
  int32   width;
  int32   height;
  int32   clip_left;
  int32   clip_top;
  int32   clip_right;
  int32   clip_bottom;
  uint32  reserved_18;
  int32   row_byte_count;
  int32   buffer_row_byte_count;
  int32   pixel_byte_shift;
  void   *extension;
  uint32 *preamble;
  Item    bitmap_item;
  uint32  reserved_34[2];
  Item    screen_group_item;
  uint32  preamble_words[2];
  uint32  reserved_48[2];
} Window;

extern void *g_WINDOW_BITMAP_BUFFER_OVERRIDE;
extern int32 g_WINDOW_WIDTH_QUANTA[18];

uint32 *
get_window_preamble(uint32 *preamble_);
Item
create_window_bitmap(void *buffer_, int32 width_, int32 height_);
MemoryBlockDescriptor *
allocate_window_record(int32 width_, int32 height_, uint32 memory_flags_);
Window *
allocate_window(int32 width_, int32 height_, uint32 memory_flags_);
void
free_window(Window *window_);
void
free_window_record(MemoryBlockDescriptor *record_);
