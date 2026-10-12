#include "display_fade.h"

#include "io_offset_request.h"

#include "graphics.h"
#include "kernel.h"

#define DISPLAY_FADE_CLUT_ENTRY_COUNT 32
#define DISPLAY_FADE_FULL_INTENSITY 0xF8
#define DISPLAY_FADE_BACKGROUND_INDEX 32

static void
_set_display_fade_level(BitmapDisplayState *display_state_, int32 level_)
{
  uint32 color_entries[DISPLAY_FADE_CLUT_ENTRY_COUNT];
  uint8  color;
  int32  index;
  int32  screen_index;

  for(index = 0; index < DISPLAY_FADE_CLUT_ENTRY_COUNT; ++index)
    {
      color                = (uint8) ((level_ * index) / (DISPLAY_FADE_CLUT_ENTRY_COUNT - 1));
      color_entries[index] = MakeCLUTColorEntry(index, color, color, color);
    }

  for(screen_index = 0; screen_index < display_state_->bitmap_count; ++screen_index)
    {
      SetScreenColors(display_state_->display_resource_items[screen_index],
                      color_entries,
                      DISPLAY_FADE_CLUT_ENTRY_COUNT);
    }
}

void
fade_display_to_black(BitmapDisplayState *display_state_, int32 frame_count_)
{
  Item  vbl_io_request;
  int32 frame;
  int32 screen_index;

  vbl_io_request = GetVBLIOReq();

  for(screen_index = 0; screen_index < display_state_->bitmap_count; ++screen_index)
    {
      SetScreenColor(display_state_->display_resource_items[screen_index],
                     MakeCLUTColorEntry(DISPLAY_FADE_BACKGROUND_INDEX, 0, 0, 0));
    }

  for(frame = frame_count_ - 1; frame > 0; --frame)
    {
      send_io_offset_request(vbl_io_request, 1);
      _set_display_fade_level(display_state_, (frame * DISPLAY_FADE_FULL_INTENSITY) / frame_count_);
    }

  _set_display_fade_level(display_state_, 0);
  DeleteItem(vbl_io_request);
}
