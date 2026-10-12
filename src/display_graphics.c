#include "display_graphics.h"

#include "ccb_manager.h"
#include "default_memory_manager_callback.h"
#include "diagnostics.h"
#include "display_palette.h"
#include "frontend_input_wait.h"
#include "graphics_folio.h"
#include "memory.h"
#include "video_window.h"
#include "vram_transfer.h"

#include "graphics.h"

#define DISPLAY_GRAPHICS_MIN_BUFFER_COUNT 1
#define DISPLAY_GRAPHICS_DEFAULT_BUFFER_COUNT 2
#define DISPLAY_GRAPHICS_DEFAULT_WIDTH 320
#define DISPLAY_GRAPHICS_DEFAULT_HEIGHT 240
#define DISPLAY_GRAPHICS_MEMORY_FLAGS 0x220
#define DISPLAY_GRAPHICS_VRAM_PAGE_SHIFT 9

typedef union DisplayPaletteSource
{
  int32        (*callback)(void);
  const uint8 *bytes;
} DisplayPaletteSource;

int32 g_DISPLAY_BUFFER_COUNT;
int32 g_DISPLAY_CONFIGURATION_MODE;
uint8 g_DISPLAY_CONFIGURATION_RESERVED_STATE[0x1C];
int32 g_VRAM_PAGE_BYTES = 2048;
int32 g_VRAM_PAGE_SHIFT = 11;
int32 g_DISPLAY_WIDTH;
int32 g_DISPLAY_HEIGHT;
int32 g_DISPLAY_HORIZONTAL_INSET = 16;
int32 g_DISPLAY_VERTICAL_INSET   = 16;
int32 g_DISPLAY_CONTENT_WIDTH    = 288;
int32 g_DISPLAY_CONTENT_HEIGHT   = 208;
uint8 g_DISPLAY_DIMENSION_RESERVED_STATE[0x14];
int32 g_DISPLAY_FIELD_FREQUENCY = 60;

void
initialize_graphics(int32 width_, int32 height_, int32 buffer_count_)
{
  DisplayBufferDescriptor *descriptor;
  DisplayPaletteSource     palette_source;
  int32                    index;

  if((buffer_count_ < DISPLAY_GRAPHICS_MIN_BUFFER_COUNT) ||
     (buffer_count_ > DISPLAY_GRAPHICS_MAX_BUFFER_COUNT))
    {
      fatal_error("initgraphics - %d PAGES REQUESTED.\n"
                  "MUST BE 1 PAGE - 4 PAGES\n",
                  buffer_count_);
    }

  if(g_MEMORY_ALLOCATION_START == NULL)
    {
      fatal_error("initgraphics - INITMEMMAN REQUIRED BEFORE INITGRAPHICS\n");
    }

  g_DISPLAY_CONFIGURATION_MODE = DISPLAY_GRAPHICS_VRAM_PAGE_SHIFT;
  if(width_ == 0)
    width_ = DISPLAY_GRAPHICS_DEFAULT_WIDTH;
  if(height_ == 0)
    height_ = DISPLAY_GRAPHICS_DEFAULT_HEIGHT;

  if(open_graphics_folio() != 0)
    fatal_error("initgraphics - CANNOT OPEN GRAPHICS FOLIO\n");

  g_DISPLAY_BUFFER_COUNT = buffer_count_;
  QueryGraphics(QUERYGRAF_TAG_FIELDFREQ, &g_DISPLAY_FIELD_FREQUENCY);

  for(index = 0; index < buffer_count_; ++index)
    {
      descriptor = create_display_buffer_descriptor(width_, height_, DISPLAY_GRAPHICS_MEMORY_FLAGS);
      g_DISPLAY_BUFFER_DESCRIPTORS[index] = *descriptor;
    }

  store_current_display_buffer_descriptor_snapshot();
  initialize_vram_transfer();
  set_display_buffer_index(0);

  for(index = 0; index < buffer_count_; ++index)
    {
      AddScreenGroup((Item) g_DISPLAY_BUFFER_DESCRIPTORS[index]
                       .words[DISPLAY_BUFFER_DESCRIPTOR_SCREEN_GROUP_ITEM_WORD],
                     NULL);
    }

  initialize_frontend_wait_io_request();
  wait_for_frontend_input();
  display_screen_by_buffer_index(0);

  g_DISPLAY_WIDTH          = width_;
  g_DISPLAY_HEIGHT         = height_;
  g_DISPLAY_CONTENT_WIDTH  = width_ - g_DISPLAY_HORIZONTAL_INSET;
  g_DISPLAY_CONTENT_HEIGHT = height_ - g_DISPLAY_VERTICAL_INSET;

  palette_source.callback = default_memory_manager_callback;
  initialize_palette_entries(0, DISPLAY_RGB555_PALETTE_ENTRY_COUNT, palette_source.bytes);
  initialize_ccb_manager(NULL, 1, 0);
}

void
initialize_default_graphics(int32 width_, int32 height_)
{
  initialize_graphics(width_, height_, DISPLAY_GRAPHICS_DEFAULT_BUFFER_COUNT);
}
