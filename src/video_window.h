#pragma once

#include "display_buffer_state.h"
#include "memory.h"

#include "graphics.h"

#define VIDEO_WINDOW_SCREEN_GROUP_TAG_COUNT 7

typedef struct VideoWindowResources
{
  Item screen_group_item;
  Item screen_item;
  Item bitmap_item;
} VideoWindowResources;

extern const TagArg G_VIDEO_WINDOW_SCREEN_GROUP_TAG_TEMPLATE[VIDEO_WINDOW_SCREEN_GROUP_TAG_COUNT];

void
create_video_window_resources(VideoWindowResources *resources_,
                              uint8                *bitmap_buffer_,
                              int32                 width_,
                              int32                 height_);
MemoryBlockDescriptor *
create_display_buffer_descriptor_record(int32 width_, int32 height_, uint32 memory_flags_);
DisplayBufferDescriptor *
create_display_buffer_descriptor(int32 width_, int32 height_, uint32 memory_flags_);
