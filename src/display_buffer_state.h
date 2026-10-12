#pragma once

#include "types.h"

#define DISPLAY_BUFFER_DESCRIPTOR_BYTES 0x40
#define DISPLAY_BUFFER_DESCRIPTOR_COUNT 4
#define DISPLAY_BUFFER_DESCRIPTOR_WORD_COUNT 16
#define DISPLAY_BUFFER_DESCRIPTOR_BUFFER_WIDTH_WORD 0
#define DISPLAY_BUFFER_DESCRIPTOR_BUFFER_HEIGHT_WORD 1
#define DISPLAY_BUFFER_DESCRIPTOR_X_WORD 2
#define DISPLAY_BUFFER_DESCRIPTOR_Y_WORD 3
#define DISPLAY_BUFFER_DESCRIPTOR_WIDTH_WORD 4
#define DISPLAY_BUFFER_DESCRIPTOR_HEIGHT_WORD 5
#define DISPLAY_BUFFER_DESCRIPTOR_X_BYTES_WORD 6
#define DISPLAY_BUFFER_DESCRIPTOR_RIGHT_BYTES_WORD 7
#define DISPLAY_BUFFER_DESCRIPTOR_WIDTH_BYTES_WORD 8
#define DISPLAY_BUFFER_DESCRIPTOR_TYPE_WORD 9
#define DISPLAY_BUFFER_DESCRIPTOR_FLAGS_WORD 10
#define DISPLAY_BUFFER_DESCRIPTOR_BITMAP_HEADER_WORD 11
#define DISPLAY_BUFFER_DESCRIPTOR_BITMAP_ITEM_WORD 12
#define DISPLAY_BUFFER_DESCRIPTOR_DISPLAY_RESOURCE_ITEM_WORD 13
#define DISPLAY_BUFFER_DESCRIPTOR_SCREEN_GROUP_ITEM_WORD 15
#define DISPLAY_BUFFER_DESCRIPTOR_BYTE_SHIFT 2
#define DISPLAY_BUFFER_DESCRIPTOR_VIDEO_WINDOW_TYPE 2

typedef struct DisplayBufferDescriptor
{
  uint32 words[DISPLAY_BUFFER_DESCRIPTOR_WORD_COUNT];
} DisplayBufferDescriptor;

typedef struct DisplayBufferState
{
  DisplayBufferDescriptor descriptor_snapshot;
  DisplayBufferDescriptor primary_descriptor;
} DisplayBufferState;

extern DisplayBufferDescriptor g_DISPLAY_BUFFER_DESCRIPTORS[DISPLAY_BUFFER_DESCRIPTOR_COUNT];
extern DisplayBufferDescriptor g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT;
extern int32                   g_CURRENT_DISPLAY_BUFFER_INDEX;

void
set_display_buffer_index(int32 index_);
DisplayBufferDescriptor *
get_current_display_buffer_descriptor(void);
DisplayBufferDescriptor *
get_previous_display_buffer_descriptor(void);
int32
get_current_display_buffer_index(void);
void
display_screen_descriptor(const DisplayBufferDescriptor *descriptor_);
void
display_screen_by_buffer_index(int32 index_);
void
display_current_screen(void);
void
advance_display_buffer(void);
void
store_display_buffer_descriptor_snapshot(const DisplayBufferDescriptor *descriptor_);
void
store_display_buffer_descriptor_snapshot_by_index(int32 index_);
void
store_current_display_buffer_descriptor_snapshot(void);
void
configure_display_buffer_descriptor_snapshot(int32 x_, int32 width_, int32 y_, int32 height_);
void
configure_display_buffer_descriptor(DisplayBufferDescriptor *descriptor_,
                                    int32                    x_,
                                    int32                    width_,
                                    int32                    y_,
                                    int32                    height_);
void
save_display_buffer_state(DisplayBufferState *state_);
void
restore_display_buffer_state(const DisplayBufferState *state_);
