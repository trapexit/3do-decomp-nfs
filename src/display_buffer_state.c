#include "display_buffer_state.h"

#include "diagnostics.h"
#include "display_graphics.h"
#include "frontend_input_wait.h"
#include "video_window_state.h"
#include "window_fill.h"

#include "graphics.h"

#define DISPLAY_SCREEN_ERROR_MESSAGE "showpage - DISPLAYSCREEN () FAILED, ERROR=%d\n"

char g_ELECTRONIC_ARTS_COPYRIGHT_NOTICE[72] =
  "Copyright (C) Electronic Arts Canada Inc. 1993.  All rights reserved.";
DisplayBufferDescriptor  g_DISPLAY_BUFFER_DESCRIPTORS[DISPLAY_BUFFER_DESCRIPTOR_COUNT];
DisplayBufferDescriptor *g_DISPLAY_BUFFER_DESCRIPTOR_POINTERS[DISPLAY_BUFFER_DESCRIPTOR_COUNT] = {
  &g_DISPLAY_BUFFER_DESCRIPTORS[0],
  &g_DISPLAY_BUFFER_DESCRIPTORS[1],
  &g_DISPLAY_BUFFER_DESCRIPTORS[2],
  &g_DISPLAY_BUFFER_DESCRIPTORS[3]
};
uint8                   g_DISPLAY_BUFFER_RESERVED_STATE[12];
DisplayBufferDescriptor g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT;
int32                   g_CURRENT_DISPLAY_BUFFER_INDEX;

void
set_display_buffer_index(int32 index_)
{
  DisplayBufferState state;

  save_display_buffer_state(&state);
  prepare_pending_video_window();
  fill_display_window((uint32) index_);
  store_current_display_buffer_descriptor_snapshot();
  fill_display_window((uint32) index_);
  restore_display_buffer_state(&state);
}

DisplayBufferDescriptor *
get_current_display_buffer_descriptor(void)
{
  return &g_DISPLAY_BUFFER_DESCRIPTORS[g_CURRENT_DISPLAY_BUFFER_INDEX];
}

DisplayBufferDescriptor *
get_previous_display_buffer_descriptor(void)
{
  return &g_DISPLAY_BUFFER_DESCRIPTORS[1 - g_CURRENT_DISPLAY_BUFFER_INDEX];
}

int32
get_current_display_buffer_index(void)
{
  return g_CURRENT_DISPLAY_BUFFER_INDEX;
}

void
display_screen_descriptor(const DisplayBufferDescriptor *descriptor_)
{
  Err result;

  result =
    DisplayScreen((Item) descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_DISPLAY_RESOURCE_ITEM_WORD],
                  0);
  if(result < 0)
    fatal_error(DISPLAY_SCREEN_ERROR_MESSAGE, result);
}

void
display_screen_by_buffer_index(int32 index_)
{
  display_screen_descriptor(&g_DISPLAY_BUFFER_DESCRIPTORS[index_]);
}

void
display_current_screen(void)
{
  display_screen_by_buffer_index(g_CURRENT_DISPLAY_BUFFER_INDEX);
}

void
advance_display_buffer(void)
{
  ++g_CURRENT_DISPLAY_BUFFER_INDEX;
  if(g_CURRENT_DISPLAY_BUFFER_INDEX >= g_DISPLAY_BUFFER_COUNT)
    g_CURRENT_DISPLAY_BUFFER_INDEX = 0;

  display_screen_by_buffer_index(g_CURRENT_DISPLAY_BUFFER_INDEX);
  begin_frontend_input_wait();
}

void
store_display_buffer_descriptor_snapshot(const DisplayBufferDescriptor *descriptor_)
{
  g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT = *descriptor_;
}

void
store_display_buffer_descriptor_snapshot_by_index(int32 index_)
{
  store_display_buffer_descriptor_snapshot(&g_DISPLAY_BUFFER_DESCRIPTORS[index_]);
}

void
store_current_display_buffer_descriptor_snapshot(void)
{
  store_display_buffer_descriptor_snapshot_by_index(g_CURRENT_DISPLAY_BUFFER_INDEX);
}

void
configure_display_buffer_descriptor_snapshot(int32 x_, int32 width_, int32 y_, int32 height_)
{
  g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_X_WORD] = x_;
  g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_X_BYTES_WORD] =
    ((uint32) x_ << DISPLAY_BUFFER_DESCRIPTOR_BYTE_SHIFT);
  g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_WORD]  = width_;
  g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_Y_WORD]      = y_;
  g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_HEIGHT_WORD] = height_;
  g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_BYTES_WORD] =
    ((uint32) width_ << DISPLAY_BUFFER_DESCRIPTOR_BYTE_SHIFT);
}

void
configure_display_buffer_descriptor(DisplayBufferDescriptor *descriptor_,
                                    int32                    x_,
                                    int32                    width_,
                                    int32                    y_,
                                    int32                    height_)
{
  descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_X_WORD] = x_;
  descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_X_BYTES_WORD] =
    ((uint32) x_ << DISPLAY_BUFFER_DESCRIPTOR_BYTE_SHIFT);
  descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_WORD]  = width_;
  descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_Y_WORD]      = y_;
  descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_HEIGHT_WORD] = height_;
  descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_BYTES_WORD] =
    ((uint32) width_ << DISPLAY_BUFFER_DESCRIPTOR_BYTE_SHIFT);
}

void
save_display_buffer_state(DisplayBufferState *state_)
{
  state_->descriptor_snapshot = g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT;
  state_->primary_descriptor  = g_DISPLAY_BUFFER_DESCRIPTORS[0];
}

void
restore_display_buffer_state(const DisplayBufferState *state_)
{
  g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT = state_->descriptor_snapshot;
  g_DISPLAY_BUFFER_DESCRIPTORS[0]      = state_->primary_descriptor;
}
