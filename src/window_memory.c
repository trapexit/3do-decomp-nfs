#include "window_memory.h"

#include "diagnostics.h"

#include "graphics.h"

#define WINDOW_BITMAP_TAG_COUNT 4
#define WINDOW_BITMAP_WIDTH_TAG_INDEX 0
#define WINDOW_BITMAP_HEIGHT_TAG_INDEX 1
#define WINDOW_BITMAP_BUFFER_TAG_INDEX 2
#define WINDOW_BITMAP_END_TAG_INDEX 3
#define WINDOW_BITMAP_DEFAULT_WIDTH 320
#define WINDOW_BITMAP_DEFAULT_HEIGHT 240
#define WINDOW_BITMAP_CE_CONTROL 0x24500000
#define WINDOW_BITMAP_CE_MASK 0xFDF00000
#define WINDOW_WIDTH_QUANTUM_COUNT 18
#define WINDOW_WIDTH_LAST_INDEX (WINDOW_WIDTH_QUANTUM_COUNT - 1)
#define WINDOW_HEADER_BYTES 0x50
#define WINDOW_PIXEL_BYTES 2
#define WINDOW_PIXEL_BYTE_SHIFT 1
#define WINDOW_EVEN_HEIGHT_MASK 0xFFFFFFFEU
#define WINDOW_UNINITIALIZED_BYTE 0xFB
#define WINDOW_PREAMBLE_FLAG_MASK 0xFF000000
#define WINDOW_PREAMBLE_FLAGS 0x1E000000
#define WINDOW_PREAMBLE_DIMENSION_MASK 0xFFF
#define WINDOW_PREAMBLE_WIDTH_SHIFT 16
#define WINDOW_DEBUG_LEVEL 3
#define WINDOW_ALLOCATION_NAME "MEM WINDOW"

void        *g_WINDOW_BITMAP_BUFFER_OVERRIDE;
int32        g_WINDOW_WIDTH_QUANTA[18] = { 32,  64,  96,   128,  160,  256,  320,  384,  512,
                                           576, 640, 1024, 1056, 1088, 1152, 1280, 1536, 2048 };
const TagArg G_WINDOW_BITMAP_TAG_TEMPLATE[WINDOW_BITMAP_TAG_COUNT] = {
  { CBM_TAG_WIDTH, (void *) WINDOW_BITMAP_DEFAULT_WIDTH },
  { CBM_TAG_HEIGHT, (void *) WINDOW_BITMAP_DEFAULT_HEIGHT },
  { CBM_TAG_BUFFER, NULL },
  { TAG_END, NULL }
};
typedef char WindowSize[(sizeof(Window) == WINDOW_HEADER_BYTES) ? 1 : -1];
typedef char WindowWidthQuantaSize[(sizeof(g_WINDOW_WIDTH_QUANTA) == 72) ? 1 : -1];
typedef char WindowBitmapTagTemplateSize
  [(sizeof(G_WINDOW_BITMAP_TAG_TEMPLATE) == 32) ? 1 : -1];

static int32
_select_window_width(int32 requested_width_);

static void
_initialize_window(Window *window_, int32 allocation_byte_count_, int32 width_, int32 height_);

uint32 *
get_window_preamble(uint32 *preamble_)
{
  return preamble_;
}

Item
create_window_bitmap(void *buffer_, int32 width_, int32 height_)
{
  TagArg tags[WINDOW_BITMAP_TAG_COUNT];
  Item   bitmap_item;
  int32  index;

  for(index = 0; index < WINDOW_BITMAP_TAG_COUNT; ++index)
    tags[index] = G_WINDOW_BITMAP_TAG_TEMPLATE[index];

  tags[WINDOW_BITMAP_WIDTH_TAG_INDEX].ta_Arg  = (void *) width_;
  tags[WINDOW_BITMAP_HEIGHT_TAG_INDEX].ta_Arg = (void *) height_;
  tags[WINDOW_BITMAP_BUFFER_TAG_INDEX].ta_Arg = buffer_;

  bitmap_item = CreateSizedItem(MKNODEID(NST_GRAPHICS, BITMAPNODE), tags, 0);
  if(bitmap_item < 0)
    fatal_error("addwindow - CANNOT CREATE BITMAP\n");

  SetCEControl(bitmap_item, WINDOW_BITMAP_CE_CONTROL, WINDOW_BITMAP_CE_MASK);
  return bitmap_item;
}

MemoryBlockDescriptor *
allocate_window_record(int32 width_, int32 height_, uint32 memory_flags_)
{
  MemoryBlockDescriptor *record;
  int32                  allocation_byte_count;

  width_                = _select_window_width(width_);
  height_               = (int32) (((uint32) height_ + 1U) & WINDOW_EVEN_HEIGHT_MASK);
  allocation_byte_count = (WINDOW_HEADER_BYTES + (width_ * WINDOW_PIXEL_BYTES * height_));
  record =
    allocate_memory_record_reporting(WINDOW_ALLOCATION_NAME, allocation_byte_count, memory_flags_);
  if(record == NULL)
    fatal_error("windowdef - OUT OF MEMORY\n");
  if(record->payload == NULL)
    fatal_error("windowdef - BAD BLOCK\n");

  _initialize_window((Window *) record->payload, allocation_byte_count, width_, height_);
  return record;
}

Window *
allocate_window(int32 width_, int32 height_, uint32 memory_flags_)
{
  return (Window *) allocate_window_record(width_, height_, memory_flags_)->payload;
}

void
free_window(Window *window_)
{
  if(window_->screen_group_item != 0)
    DeleteScreenGroup(window_->screen_group_item);
  else if(window_->bitmap_item != 0)
    DeleteItem(window_->bitmap_item);

  free_memory(window_);
}

void
free_window_record(MemoryBlockDescriptor *record_)
{
  free_window((Window *) record_->payload);
}

static int32
_select_window_width(int32 requested_width_)
{
  int32 index;
  int32 width;

  index = 0;
  while((g_WINDOW_WIDTH_QUANTA[index] < requested_width_) && (index < WINDOW_WIDTH_LAST_INDEX))
    {
      index++;
    }

  width = g_WINDOW_WIDTH_QUANTA[index];
  if((width != requested_width_) && (g_DEBUG_OUTPUT_LEVEL >= WINDOW_DEBUG_LEVEL))
    {
      print_debug_message("windowdef - WINDOW WIDTH CHANGED FROM %d TO %d\n",
                          requested_width_,
                          width);
    }
  return width;
}

static void
_initialize_window(Window *window_, int32 allocation_byte_count_, int32 width_, int32 height_)
{
  void *bitmap_buffer;

  fill_bytes((uint8 *) window_, allocation_byte_count_, WINDOW_UNINITIALIZED_BYTE);
  clear_bytes((uint8 *) window_, WINDOW_HEADER_BYTES);
  window_->preamble = window_->preamble_words;
  window_->preamble_words[0] =
    ((window_->preamble_words[0] & ~WINDOW_PREAMBLE_FLAG_MASK) | WINDOW_PREAMBLE_FLAGS);
  window_->preamble_words[1] =
    (((width_ & WINDOW_PREAMBLE_DIMENSION_MASK) << WINDOW_PREAMBLE_WIDTH_SHIFT) |
     (height_ & WINDOW_PREAMBLE_DIMENSION_MASK));
  window_->width                 = width_;
  window_->height                = height_;
  window_->clip_right            = width_;
  window_->clip_bottom           = height_;
  window_->row_byte_count        = (width_ * WINDOW_PIXEL_BYTES);
  window_->buffer_row_byte_count = window_->row_byte_count;
  window_->pixel_byte_shift      = WINDOW_PIXEL_BYTE_SHIFT;

  bitmap_buffer = g_WINDOW_BITMAP_BUFFER_OVERRIDE;
  if(bitmap_buffer == NULL)
    bitmap_buffer = (uint8 *) window_ + WINDOW_HEADER_BYTES;
  window_->bitmap_item = create_window_bitmap(bitmap_buffer, width_, height_);
}
