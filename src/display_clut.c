#include "display_clut.h"

#include "diagnostics.h"
#include "memory.h"

#include "graphics.h"
#include "hardware.h"

#define RAW_VDL_MEMORY_NAME "rawvdl"
#define RAW_VDL_SUBMIT_ERROR_MESSAGE "makesimplevdl - SubmitVDL() failed with error code %ld\n"
#define RAW_VDL_SET_ERROR_MESSAGE "makesimplevdl - SetVDL() failed with error code %d\n"
#define RAW_VDL_DISPLAY_ERROR_MESSAGE "makesimplevdl - DISPLAYSCREEN() FAILED, ERROR=%d\n"
#define DISPLAY_CLUT_TYPE_ERROR_MESSAGE "setclut - MUST BE VIDEO WINDOW\n"

#define RAW_VDL_MEMORY_FLAGS 0x220
#define RAW_VDL_RECORD_COUNT 2
#define RAW_VDL_RECORD_WORD_COUNT 40
#define RAW_VDL_TOTAL_WORD_COUNT (RAW_VDL_RECORD_COUNT * RAW_VDL_RECORD_WORD_COUNT)
#define RAW_VDL_CLUT_ENTRY_COUNT 32
#define RAW_VDL_TRAILING_CONTROL_COUNT 3
#define RAW_VDL_RGB_BYTES 3
#define RAW_VDL_GREEN_BYTE 1
#define RAW_VDL_BLUE_BYTE 2
#define RAW_VDL_NEXT_RECORD_OFFSET_BYTES 0x90
#define RAW_VDL_DMA_COLOR_WORD_COUNT 34
#define RAW_VDL_FIRST_SCAN_LINE_COUNT 239
#define RAW_VDL_FINAL_SCAN_LINE_COUNT 1
#define DISPLAY_CLUT_BITMAP_DATA_OFFSET 16

#define RAW_VDL_FIRST_DMA_CONTROL                                                                  \
  (VDL_ENVIDDMA | VDL_RELSEL | VDL_LDCUR | VDL_LDPREV |                                            \
   (RAW_VDL_DMA_COLOR_WORD_COUNT << VDL_LEN_SHIFT) | RAW_VDL_FIRST_SCAN_LINE_COUNT)
#define RAW_VDL_FINAL_DMA_CONTROL                                                                  \
  (VDL_480RES | VDL_LDCUR | VDL_LDPREV | (RAW_VDL_DMA_COLOR_WORD_COUNT << VDL_LEN_SHIFT) |         \
   RAW_VDL_FINAL_SCAN_LINE_COUNT)
#define RAW_VDL_DISPLAY_CONTROL                                                                    \
  (DEFAULT_DISPCTRL | VDL_CLUTBYPASSEN | VDL_WINVINTEN | VDL_WINHINTEN)

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

typedef struct RawDisplayVdlRecord
{
  uint32 dma_control;
  uint8 *current_buffer;
  uint8 *previous_buffer;
  uint32 next_record_offset;
  uint32 display_control;
  uint32 clut_entries[RAW_VDL_CLUT_ENTRY_COUNT];
  uint32 trailing_controls[RAW_VDL_TRAILING_CONTROL_COUNT];
} RawDisplayVdlRecord;

COMPILE_TIME_SIZE_ASSERT(raw_display_vdl_record_size,
                         sizeof(RawDisplayVdlRecord) ==
                           (RAW_VDL_RECORD_WORD_COUNT * sizeof(uint32)));

static void
_initialize_raw_vdl_record(RawDisplayVdlRecord *record_,
                           uint8               *bitmap_buffer_,
                           uint32               dma_control_,
                           const uint8         *rgb_palette_);

void
install_simple_vdl(Item         screen_item_,
                   uint8       *bitmap_buffer_,
                   const uint8 *rgb_palette_,
                   int32        display_now_)
{
  RawDisplayVdlRecord *records;
  Item                 installed_vdl;
  Item                 previous_vdl;
  int32                submit_type;
  Err                  result;

  records = (RawDisplayVdlRecord *) allocate_named_memory(RAW_VDL_MEMORY_NAME,
                                                          sizeof(RawDisplayVdlRecord) *
                                                            RAW_VDL_RECORD_COUNT,
                                                          RAW_VDL_MEMORY_FLAGS);
  _initialize_raw_vdl_record(&records[0], bitmap_buffer_, RAW_VDL_FIRST_DMA_CONTROL, rgb_palette_);
  _initialize_raw_vdl_record(&records[1], bitmap_buffer_, RAW_VDL_FINAL_DMA_CONTROL, rgb_palette_);

  submit_type = VDLTYPE_FULL;
  if(rgb_palette_ == NULL)
    submit_type = VDLTYPE_SIMPLE;
  installed_vdl = SubmitVDL((VDLEntry *) records, RAW_VDL_TOTAL_WORD_COUNT, submit_type);
  if(installed_vdl < 0)
    fatal_error(RAW_VDL_SUBMIT_ERROR_MESSAGE, installed_vdl);

  previous_vdl = SetVDL(screen_item_, installed_vdl);
  if(previous_vdl < 0)
    fatal_error(RAW_VDL_SET_ERROR_MESSAGE, previous_vdl);

  if(display_now_ != 0)
    {
      result = DisplayScreen(screen_item_, 0);
      if(result < 0)
        fatal_error(RAW_VDL_DISPLAY_ERROR_MESSAGE, result);
    }

  DeleteItem(previous_vdl);
  free_memory(records);
}

void
set_display_clut(const DisplayBufferDescriptor *descriptor_, const uint8 *rgb_palette_)
{
  const DisplayBufferDescriptor *current_descriptor;
  uint8                         *bitmap_buffer;
  Item                           screen_item;
  int32                          display_now;

  if(descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_TYPE_WORD] !=
     DISPLAY_BUFFER_DESCRIPTOR_VIDEO_WINDOW_TYPE)
    {
      fatal_error(DISPLAY_CLUT_TYPE_ERROR_MESSAGE);
    }

  current_descriptor = &g_DISPLAY_BUFFER_DESCRIPTORS[g_CURRENT_DISPLAY_BUFFER_INDEX];
  screen_item   = (Item) descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_DISPLAY_RESOURCE_ITEM_WORD];
  display_now   = current_descriptor->words[DISPLAY_BUFFER_DESCRIPTOR_DISPLAY_RESOURCE_ITEM_WORD] ==
                  (uint32) screen_item;
  bitmap_buffer = ((uint8 *) descriptor_->words[DISPLAY_BUFFER_DESCRIPTOR_BITMAP_HEADER_WORD] +
                   DISPLAY_CLUT_BITMAP_DATA_OFFSET);
  install_simple_vdl(screen_item, bitmap_buffer, rgb_palette_, display_now);
}

static void
_initialize_raw_vdl_record(RawDisplayVdlRecord *record_,
                           uint8               *bitmap_buffer_,
                           uint32               dma_control_,
                           const uint8         *rgb_palette_)
{
  int32 color_offset;
  int32 color_index;
  int32 control_index;

  record_->dma_control        = dma_control_;
  record_->current_buffer     = bitmap_buffer_;
  record_->previous_buffer    = bitmap_buffer_;
  record_->next_record_offset = RAW_VDL_NEXT_RECORD_OFFSET_BYTES;
  record_->display_control    = RAW_VDL_DISPLAY_CONTROL;
  for(color_index = 0; color_index < RAW_VDL_CLUT_ENTRY_COUNT; ++color_index)
    {
      if(rgb_palette_ == NULL)
        record_->clut_entries[color_index] = VDL_NOP;
      else
        {
          color_offset = (color_index * RAW_VDL_RGB_BYTES);
          record_->clut_entries[color_index] =
            MakeCLUTColorEntry(color_index,
                               rgb_palette_[color_offset],
                               rgb_palette_[color_offset + RAW_VDL_GREEN_BYTE],
                               rgb_palette_[color_offset + RAW_VDL_BLUE_BYTE]);
        }
    }

  for(control_index = 0; control_index < RAW_VDL_TRAILING_CONTROL_COUNT; ++control_index)
    {
      record_->trailing_controls[control_index] = VDL_NOP;
    }
}
