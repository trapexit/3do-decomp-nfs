#include "bitmap_clip.h"

#include "camera_io.h"
#include "ccb_manager.h"
#include "control_input.h"
#include "control_input_event_table.h"
#include "display_environment.h"
#include "display_graphics.h"
#include "fixed_math.h"
#include "game_frame.h"
#include "render_thread_signal.h"
#include "video_window_state.h"

#include "audio.h"
#include "kernel.h"

#define DISPLAY_WIDTH_PIXELS 320
#define DISPLAY_HEIGHT_PIXELS 239
#define DISPLAY_PAGE_COUNT 200
#define DISPLAY_MEMORY_BYTES 0xD8000
#define DISPLAY_MEMORY_LIMIT 0xF8C00
#define DISPLAY_CCB_COUNT 600
#define AUDIO_RATE_LOW 0x800000
#define AUDIO_RATE_HIGH 0xF00000
#define AUDIO_ITEM_TYPE 0x405
#define DISPLAY_CLIP_RIGHT 320
#define DISPLAY_CLIP_BOTTOM 239
#define DISPLAY_BITMAP_PIXEL_BYTES 2
#define DISPLAY_CLIP_EVEN_MASK 0xFFFFFFFE
#define DISPLAY_CLIP_FIXED_SHIFT 16

extern void
configure_display_memory(int32 page_count_, int32 memory_bytes_, int32 memory_limit_);
BitmapDisplayState g_BITMAP_DISPLAY_STATE;

extern int32
initialize_bitmap_display_state_internal(BitmapDisplayState *display_state_);

void
configure_bitmap_display_state(BitmapDisplayState *display_state_)
{
  int32 index;

  display_state_->bitmap_count         = g_DISPLAY_BUFFER_COUNT;
  display_state_->current_bitmap_index = 0;
  display_state_->bitmap_bytes    = g_DISPLAY_WIDTH * g_DISPLAY_HEIGHT * DISPLAY_BITMAP_PIXEL_BYTES;
  display_state_->vram_page_count = display_state_->bitmap_bytes >> g_VRAM_PAGE_SHIFT;

  for(index = 0; index < g_DISPLAY_BUFFER_COUNT; ++index)
    {
      display_state_->display_resource_items[index] =
        (Item) g_DISPLAY_BUFFER_DESCRIPTORS[index]
          .words[DISPLAY_BUFFER_DESCRIPTOR_DISPLAY_RESOURCE_ITEM_WORD];
      display_state_->bitmap_items[index] = (Item) g_DISPLAY_BUFFER_DESCRIPTORS[index]
                                              .words[DISPLAY_BUFFER_DESCRIPTOR_BITMAP_ITEM_WORD];
      display_state_->bitmap_objects[index] =
        (Bitmap *) LookupItem(display_state_->bitmap_items[index]);
    }
}

void
configure_display_clip_rectangle(int32 left_, int32 right_, int32 top_, int32 bottom_)
{
  Item   bitmap_item;
  uint32 aligned_bottom;
  uint32 aligned_left;
  uint32 aligned_right;
  uint32 aligned_top;

  aligned_left   = ((uint32) left_ & DISPLAY_CLIP_EVEN_MASK);
  aligned_right  = (((uint32) right_ + 1U) & DISPLAY_CLIP_EVEN_MASK);
  aligned_top    = ((uint32) top_ & DISPLAY_CLIP_EVEN_MASK);
  aligned_bottom = (((uint32) bottom_ + 1U) & DISPLAY_CLIP_EVEN_MASK);

  g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_X_WORD] = aligned_left;
  g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_X_BYTES_WORD] =
    (aligned_left << DISPLAY_BUFFER_DESCRIPTOR_BYTE_SHIFT);
  g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_WORD] = aligned_right;
  g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_RIGHT_BYTES_WORD] =
    (aligned_right << DISPLAY_BUFFER_DESCRIPTOR_BYTE_SHIFT);
  g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_Y_WORD] = aligned_top;
  g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_HEIGHT_WORD] =
    aligned_bottom;

  bitmap_item =
    (Item) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_BITMAP_ITEM_WORD];
  (void) SetClipOrigin(bitmap_item, 0, 0);
  (void) SetClipWidth(bitmap_item, aligned_right - aligned_left);
  (void) SetClipHeight(bitmap_item, aligned_bottom - aligned_top);
  (void) SetClipOrigin(bitmap_item, aligned_left, aligned_top);
  set_ccb_position_offset((int32) (0U - (aligned_left << DISPLAY_CLIP_FIXED_SHIFT)),
                          (int32) (0U - (aligned_top << DISPLAY_CLIP_FIXED_SHIFT)));
}

void
configure_display_buffer_clip_rectangle(DisplayBufferDescriptor *descriptor_,
                                        int32                    left_,
                                        int32                    right_,
                                        int32                    top_,
                                        int32                    bottom_)
{
  Item   bitmap_item;
  uint32 aligned_bottom;
  uint32 aligned_left;
  uint32 aligned_right;
  uint32 aligned_top;

  aligned_left   = ((uint32) left_ & DISPLAY_CLIP_EVEN_MASK);
  aligned_right  = (((uint32) right_ + 1U) & DISPLAY_CLIP_EVEN_MASK);
  aligned_top    = ((uint32) top_ & DISPLAY_CLIP_EVEN_MASK);
  aligned_bottom = (((uint32) bottom_ + 1U) & DISPLAY_CLIP_EVEN_MASK);

  descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_X_WORD] = aligned_left;
  descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_X_BYTES_WORD] =
    (aligned_left << DISPLAY_BUFFER_DESCRIPTOR_BYTE_SHIFT);
  descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_WORD] = aligned_right;
  descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_RIGHT_BYTES_WORD] =
    (aligned_right << DISPLAY_BUFFER_DESCRIPTOR_BYTE_SHIFT);
  descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_Y_WORD]      = aligned_top;
  descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_HEIGHT_WORD] = aligned_bottom;

  bitmap_item = (Item) descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_BITMAP_ITEM_WORD];
  (void) SetClipOrigin(bitmap_item, 0, 0);
  (void) SetClipWidth(bitmap_item, aligned_right - aligned_left);
  (void) SetClipHeight(bitmap_item, aligned_bottom - aligned_top);
  (void) SetClipOrigin(bitmap_item, aligned_left, aligned_top);
  set_ccb_position_offset((int32) (0U - (aligned_left << DISPLAY_CLIP_FIXED_SHIFT)),
                          (int32) (0U - (aligned_top << DISPLAY_CLIP_FIXED_SHIFT)));
}

int32
initialize_bitmap_display_state_internal(BitmapDisplayState *display_state_)
{
  int32 display_environment;
  int32 display_mode;
  int32 initialization_failed;

  initialization_failed = 0;
  display_environment   = initialize_display_environment();
  if(display_environment == 0)
    initialization_failed = 1;
  initialize_camera_io();
  display_mode = open_math_folio();
  if(display_mode < 0)
    initialization_failed = 1;
  configure_display_memory(DISPLAY_PAGE_COUNT, DISPLAY_MEMORY_BYTES, DISPLAY_MEMORY_LIMIT);
  initialize_default_graphics(0, 0);
  configure_bitmap_display_state(display_state_);
  (void) initialize_control_input_event_table();
  initialize_ccb_manager(NULL, DISPLAY_CCB_COUNT, 1);
  g_CONTROL_PAD_COUNT = 1;
  (void) OpenAudioFolio();
  g_GAME_FRAME_STATE.display_audio_item = OwnAudioClock();
  (void) GetAudioRate();
  SetAudioRate(g_GAME_FRAME_STATE.display_audio_item, AUDIO_RATE_LOW);
  (void) GetAudioDuration();
  SetAudioRate(g_GAME_FRAME_STATE.display_audio_item, AUDIO_RATE_HIGH);
  (void) GetAudioDuration();
  g_GAME_FRAME_STATE.cue        = CreateSizedItem(AUDIO_ITEM_TYPE, NULL, 0);
  g_GAME_FRAME_STATE.cue_signal = GetCueSignal(g_GAME_FRAME_STATE.cue);
  prepare_pending_video_window();
  configure_display_clip_rectangle(0, DISPLAY_CLIP_RIGHT, 0, DISPLAY_CLIP_BOTTOM);
  store_current_display_buffer_descriptor_snapshot();
  configure_display_clip_rectangle(0, DISPLAY_CLIP_RIGHT, 0, DISPLAY_CLIP_BOTTOM);
  return initialization_failed;
}

int32
initialize_bitmap_display_state(void)
{
  return initialize_bitmap_display_state_internal(&g_BITMAP_DISPLAY_STATE) == 0;
}

void
open_bitmap_display_items(void)
{
  g_BITMAP_DISPLAY_STATE.bitmap_items[0] = OpenItem(g_BITMAP_DISPLAY_STATE.bitmap_items[0], NULL);
  g_BITMAP_DISPLAY_STATE.bitmap_items[1] = OpenItem(g_BITMAP_DISPLAY_STATE.bitmap_items[1], NULL);
  g_BITMAP_DISPLAY_STATE.display_resource_items[0] =
    OpenItem(g_BITMAP_DISPLAY_STATE.display_resource_items[0], NULL);
  g_BITMAP_DISPLAY_STATE.display_resource_items[1] =
    OpenItem(g_BITMAP_DISPLAY_STATE.display_resource_items[1], NULL);
}

void
open_render_thread_resources(void)
{
  g_BITMAP_DISPLAY_STATE.bitmap_items[0] = OpenItem(g_BITMAP_DISPLAY_STATE.bitmap_items[0], NULL);
  g_BITMAP_DISPLAY_STATE.bitmap_items[1] = OpenItem(g_BITMAP_DISPLAY_STATE.bitmap_items[1], NULL);
  g_BITMAP_DISPLAY_STATE.display_resource_items[0] =
    OpenItem(g_BITMAP_DISPLAY_STATE.display_resource_items[0], NULL);
  g_BITMAP_DISPLAY_STATE.display_resource_items[1] =
    OpenItem(g_BITMAP_DISPLAY_STATE.display_resource_items[1], NULL);
  (void) initialize_display_environment();
  g_RENDER_THREAD_STATE.vram_io_request = GetVRAMIOReq();
}

void
close_bitmap_display_items(void)
{
  CloseItem(g_BITMAP_DISPLAY_STATE.bitmap_items[0]);
  CloseItem(g_BITMAP_DISPLAY_STATE.bitmap_items[1]);
  CloseItem(g_BITMAP_DISPLAY_STATE.display_resource_items[0]);
  CloseItem(g_BITMAP_DISPLAY_STATE.display_resource_items[1]);
}

void
close_render_thread_resources(void)
{
  CloseItem(g_BITMAP_DISPLAY_STATE.bitmap_items[0]);
  CloseItem(g_BITMAP_DISPLAY_STATE.bitmap_items[1]);
  CloseItem(g_BITMAP_DISPLAY_STATE.display_resource_items[0]);
  CloseItem(g_BITMAP_DISPLAY_STATE.display_resource_items[1]);
  if(g_RENDER_THREAD_STATE.vram_io_request >= 0)
    DeleteItem(g_RENDER_THREAD_STATE.vram_io_request);
  g_RENDER_THREAD_STATE.vram_io_request = -1;
}

Err
reset_current_bitmap_clip(void)
{
  return set_current_bitmap_clip(0, 0, DISPLAY_WIDTH_PIXELS, DISPLAY_HEIGHT_PIXELS);
}

Err
set_current_bitmap_clip(int32 left_, int32 top_, int32 width_, int32 height_)
{
  if(left_ < 0)
    left_ = 0;
  if(top_ < 0)
    top_ = 0;
  if(width_ > DISPLAY_WIDTH_PIXELS)
    width_ = DISPLAY_WIDTH_PIXELS;
  if(height_ > DISPLAY_HEIGHT_PIXELS)
    height_ = DISPLAY_HEIGHT_PIXELS;

  SetClipOrigin(g_BITMAP_DISPLAY_STATE.bitmap_items[g_BITMAP_DISPLAY_STATE.current_bitmap_index],
                left_,
                top_);
  SetClipWidth(g_BITMAP_DISPLAY_STATE.bitmap_items[g_BITMAP_DISPLAY_STATE.current_bitmap_index],
               width_);
  return SetClipHeight(
    g_BITMAP_DISPLAY_STATE.bitmap_items[g_BITMAP_DISPLAY_STATE.current_bitmap_index],
    height_);
}

Err
draw_current_bitmap_cels(CCB *ccb_)
{
  /* Retail observes frame progress before resolving the active bitmap. */
  (void) g_GAME_FRAME_STATE.frame_progress;
  return DrawCels(g_BITMAP_DISPLAY_STATE.bitmap_items[g_BITMAP_DISPLAY_STATE.current_bitmap_index],
                  ccb_);
}

Err
draw_bitmap_cels(Item bitmap_item_, CCB *ccb_)
{
  return DrawCels(bitmap_item_, ccb_);
}
