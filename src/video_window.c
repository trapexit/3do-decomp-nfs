#include "video_window.h"

#include "diagnostics.h"
#include "display_clut.h"
#include "display_graphics.h"
#include "grayscale_palette.h"

#include "item.h"
#include "mem.h"

#define VIDEO_WINDOW_BITMAP_CE_CONTROL 0x24500000
#define VIDEO_WINDOW_BITMAP_CE_MASK 0xFDF00000
#define VIDEO_WINDOW_DEBUG_LEVEL 3
#define VIDEO_WINDOW_WIDTH 320
#define VIDEO_WINDOW_ROW_BYTES 640
#define VIDEO_WINDOW_EVEN_HEIGHT_MASK 0xFFFFFFFEU
#define VIDEO_WINDOW_ALLOCATION_NAME "VIDEO WINDOW"
#define VIDEO_WINDOW_ALIGNMENT_OVERHEAD_BYTES 0x50
#define VIDEO_WINDOW_BITMAP_HEADER_BYTES 0x10
#define VIDEO_WINDOW_ALIGNMENT_BIAS                                                                \
  (DISPLAY_BUFFER_DESCRIPTOR_BYTES + VIDEO_WINDOW_BITMAP_HEADER_BYTES - 1)
#define VIDEO_WINDOW_PREAMBLE_FLAG_MASK 0x00FFFFFF
#define VIDEO_WINDOW_PREAMBLE_FLAGS 0x1E000000
#define VIDEO_WINDOW_PREAMBLE_CONTROL_MASK 0xF000F000
#define VIDEO_WINDOW_PREAMBLE_CONTROL 0x01400000
#define VIDEO_WINDOW_PREAMBLE_HEIGHT_MASK 0xFFE
#define VIDEO_WINDOW_WIDTH_TAG_INDEX 0
#define VIDEO_WINDOW_SCREEN_HEIGHT_TAG_INDEX 1
#define VIDEO_WINDOW_DISPLAY_HEIGHT_TAG_INDEX 2
#define VIDEO_WINDOW_BUFFER_TAG_INDEX 3

const TagArg G_VIDEO_WINDOW_SCREEN_GROUP_TAG_TEMPLATE[VIDEO_WINDOW_SCREEN_GROUP_TAG_COUNT] = {
  { CSG_TAG_BITMAPWIDTH_ARRAY, NULL },
  { CSG_TAG_SCREENHEIGHT, NULL },
  { CSG_TAG_DISPLAYHEIGHT, NULL },
  { CSG_TAG_BITMAPBUF_ARRAY, NULL },
  { CSG_TAG_SCREENCOUNT, (void *) 1 },
  { CSG_TAG_BITMAPCOUNT, (void *) 1 },
  { CSG_TAG_DONE, NULL }
};

typedef char VideoWindowResourcesSize[(sizeof(VideoWindowResources) == 12) ? 1 : -1];
typedef char VideoWindowScreenGroupTagTemplateSize
  [(sizeof(G_VIDEO_WINDOW_SCREEN_GROUP_TAG_TEMPLATE) == 56) ? 1 : -1];

void
create_video_window_resources(VideoWindowResources *resources_,
                              uint8                *bitmap_buffer_,
                              int32                 width_,
                              int32                 height_)
{
  TagArg  tags[VIDEO_WINDOW_SCREEN_GROUP_TAG_COUNT];
  uint8   grayscale_palette[GRAYSCALE_RGB_PALETTE_BYTES];
  Screen *screen;
  Item    screen_group_item;
  Item    screen_item;
  Item    bitmap_item;
  int32   index;

  for(index = 0; index < VIDEO_WINDOW_SCREEN_GROUP_TAG_COUNT; ++index)
    tags[index] = G_VIDEO_WINDOW_SCREEN_GROUP_TAG_TEMPLATE[index];

  tags[VIDEO_WINDOW_WIDTH_TAG_INDEX].ta_Arg          = &width_;
  tags[VIDEO_WINDOW_SCREEN_HEIGHT_TAG_INDEX].ta_Arg  = (void *) height_;
  tags[VIDEO_WINDOW_DISPLAY_HEIGHT_TAG_INDEX].ta_Arg = (void *) height_;
  tags[VIDEO_WINDOW_BUFFER_TAG_INDEX].ta_Arg         = &bitmap_buffer_;

  screen_group_item = CreateScreenGroup(&screen_item, tags);
  if(screen_group_item < 0)
    {
      ScavengeMem();
      screen_group_item = CreateScreenGroup(&screen_item, tags);
      if(screen_group_item < 0)
        fatal_error("addvideowindow - CANNOT CREATE SCREEN GROUP\n");
    }

  screen      = (Screen *) LookupItem(screen_item);
  bitmap_item = screen->scr_TempBitmap->bm.n_Item;
  SetCEControl(bitmap_item, VIDEO_WINDOW_BITMAP_CE_CONTROL, VIDEO_WINDOW_BITMAP_CE_MASK);
  build_grayscale_rgb_palette(grayscale_palette);
  install_simple_vdl(screen_item, bitmap_buffer_, grayscale_palette, 0);

  resources_->screen_group_item = screen_group_item;
  resources_->screen_item       = screen_item;
  resources_->bitmap_item       = bitmap_item;
}

MemoryBlockDescriptor *
create_display_buffer_descriptor_record(int32 width_, int32 height_, uint32 memory_flags_)
{
  MemoryBlockDescriptor   *record;
  DisplayBufferDescriptor *descriptor;
  VideoWindowResources     resources;
  uint32                  *bitmap_header;
  uint8                   *bitmap_buffer;
  uint32                   bitmap_header_address;
  int32                    allocation_byte_count;
  int32                    height;

  height = (int32) (((uint32) height_ + 1U) & VIDEO_WINDOW_EVEN_HEIGHT_MASK);
  if(width_ != VIDEO_WINDOW_WIDTH)
    {
      if(g_DEBUG_OUTPUT_LEVEL >= VIDEO_WINDOW_DEBUG_LEVEL)
        {
          print_debug_message("videowindowdef - WINDOW WIDTH OF %d CHANGED TO 320\n", width_);
        }
      width_ = VIDEO_WINDOW_WIDTH;
    }

  allocation_byte_count =
    (height * VIDEO_WINDOW_ROW_BYTES) + VIDEO_WINDOW_ALIGNMENT_OVERHEAD_BYTES + g_VRAM_PAGE_BYTES;
  record = allocate_memory_record_reporting(VIDEO_WINDOW_ALLOCATION_NAME,
                                            allocation_byte_count,
                                            memory_flags_);
  if(record == NULL)
    fatal_error("videowindowdef - OUT OF MEMORY\n");

  descriptor = (DisplayBufferDescriptor *) record->payload;
  if(descriptor == NULL)
    fatal_error("videowindowdef - BAD BLOCK\n");

  bitmap_header_address =
    memory_pointer_to_address(descriptor) + g_VRAM_PAGE_BYTES + VIDEO_WINDOW_ALIGNMENT_BIAS;
  bitmap_header_address &= ~((uint32) g_VRAM_PAGE_BYTES - 1);
  bitmap_header =
    (uint32 *) memory_address_to_pointer(bitmap_header_address - VIDEO_WINDOW_BITMAP_HEADER_BYTES);
  bitmap_buffer = (uint8 *) bitmap_header + VIDEO_WINDOW_BITMAP_HEADER_BYTES;
  clear_bytes((uint8 *) descriptor, (int32) (bitmap_buffer - (uint8 *) descriptor));

  bitmap_header[0] =
    ((bitmap_header[0] & VIDEO_WINDOW_PREAMBLE_FLAG_MASK) | VIDEO_WINDOW_PREAMBLE_FLAGS);
  bitmap_header[1] =
    ((bitmap_header[1] & VIDEO_WINDOW_PREAMBLE_CONTROL_MASK) | VIDEO_WINDOW_PREAMBLE_CONTROL |
     ((uint32) height & VIDEO_WINDOW_PREAMBLE_HEIGHT_MASK));

  descriptor->words[DISPLAY_BUFFER_DESCRIPTOR_BUFFER_WIDTH_WORD]  = width_;
  descriptor->words[DISPLAY_BUFFER_DESCRIPTOR_BUFFER_HEIGHT_WORD] = height;
  descriptor->words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_WORD]         = width_;
  descriptor->words[DISPLAY_BUFFER_DESCRIPTOR_HEIGHT_WORD]        = height;
  descriptor->words[DISPLAY_BUFFER_DESCRIPTOR_RIGHT_BYTES_WORD]   = VIDEO_WINDOW_ROW_BYTES;
  descriptor->words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_BYTES_WORD]   = VIDEO_WINDOW_ROW_BYTES;
  descriptor->words[DISPLAY_BUFFER_DESCRIPTOR_TYPE_WORD] =
    DISPLAY_BUFFER_DESCRIPTOR_VIDEO_WINDOW_TYPE;
  descriptor->words[DISPLAY_BUFFER_DESCRIPTOR_FLAGS_WORD]         = 0;
  descriptor->words[DISPLAY_BUFFER_DESCRIPTOR_BITMAP_HEADER_WORD] = (uint32) bitmap_header;

  create_video_window_resources(&resources, bitmap_buffer, width_, height);
  descriptor->words[DISPLAY_BUFFER_DESCRIPTOR_BITMAP_ITEM_WORD] = (uint32) resources.bitmap_item;
  descriptor->words[DISPLAY_BUFFER_DESCRIPTOR_DISPLAY_RESOURCE_ITEM_WORD] =
    (uint32) resources.screen_item;
  descriptor->words[DISPLAY_BUFFER_DESCRIPTOR_SCREEN_GROUP_ITEM_WORD] =
    (uint32) resources.screen_group_item;
  return record;
}

DisplayBufferDescriptor *
create_display_buffer_descriptor(int32 width_, int32 height_, uint32 memory_flags_)
{
  return (DisplayBufferDescriptor *) create_display_buffer_descriptor_record(width_,
                                                                             height_,
                                                                             memory_flags_)
    ->payload;
}
