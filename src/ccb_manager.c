#include "ccb_manager.h"

#include "display_buffer_state.h"
#include "display_palette.h"
#include "memory.h"

#include "graphics.h"
#include "hardware.h"

#define CCB_PACKED_FLAG 0x00000200
#define CCB_PACKED_FORMAT_MASK 0x80000017
#define CCB_PACKED_HEADER_WORDS 4
#define CCB_PACKED_FORMAT_SHIFT 24
#define CCB_PREAMBLE_VCNT_SHIFT 6
#define CCB_LOAD_GEOMETRY_FLAGS 0x06000000
#define CCB_ACSC_FLAG 0x00080000
#define CCB_PREAMBLE_LRFORM 0x00000800
#define CCB_PREAMBLE_WOFFSET10 0x00001000
#define CEL_CHUNK_OFFSET_SHIFT 8
#define CEL_PALETTE_CHUNK_TYPE 0x20
#define CEL_CHUNK_PIXC_WORD 3
#define CEL_FORMAT_SPECIAL_PACKED -123
#define CEL_FORMAT_SPECIAL_PIXC 5
#define CEL_FORMAT_LINEAR 30
#define CEL_HEADER_WORDS 4
#define CEL_HEIGHT_MASK 0xFFF
#define CEL_WIDTH_LEFT_SHIFT 4
#define CEL_WIDTH_RIGHT_SHIFT 20
#define CEL_ROW_WORD_BIAS 33
#define CEL_ROW_WORD_SHIFT 5
#define CEL_SMALL_BPP_LIMIT 8
#define CEL_SMALL_BPP_ROW_SHIFT 24
#define CEL_LARGE_BPP_ROW_SHIFT 16
#define CCB_POSITION_SHIFT 16
#define CCB_LAST_FLAG 0x40000000
#define CCB_CHAIN_INITIAL_FLAGS (CCB_SKIP | CCB_LAST | CCB_NPABS)
#define CCB_CHAIN_FINAL_FLAGS (CCB_LAST | CCB_NPABS)
#define CCB_MANAGER_ALLOCATED_BATCH 0x00000001
#define CCB_PIXEL_CONTROL_OPAQUE 0x1F001F00
#define CCB_PIXEL_CONTROL_SPECIAL 0x3F003F00
#define CCB_PREAMBLE_0_DEFAULT 0x00000016
#define CCB_PREAMBLE_1_DEFAULT 0x00001000
#define CCB_RELATIVE_NEXT_OFFSET 0x00000038
#define CCB_RELATIVE_SOURCE_OFFSET 0x00000030
#define CCB_STANDARD_FLAGS 0x1FEE4410
#define CCB_UNIT_HORIZONTAL_SCALE 0x00100000
#define CCB_UNIT_VERTICAL_SCALE 0x00010000
#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

typedef struct CcbPackedTemplate
{
  uint32 flags;
  uint32 next_offset;
  uint32 pixel_control;
} CcbPackedTemplate;

COMPILE_TIME_SIZE_ASSERT(ccb_template_size, sizeof(CcbTemplate) == 60);
COMPILE_TIME_SIZE_ASSERT(ccb_packed_template_size, sizeof(CcbPackedTemplate) == 12);
COMPILE_TIME_SIZE_ASSERT(render_ccb_size, sizeof(RenderCcb) == 64);
COMPILE_TIME_SIZE_ASSERT(ccb_manager_state_size, sizeof(CcbManagerState) == 12);
COMPILE_TIME_SIZE_ASSERT(uint16_size, sizeof(uint16) == 2);

const char        G_CCB_MANAGER_ALLOCATION_NAME[] = "CCB Manager";
CcbManagerState   g_CCB_MANAGER_STATE             = { 0, NULL, NULL };
uint32            g_CCB_TEMPLATE_PIXEL_CONTROL    = CCB_PIXEL_CONTROL_OPAQUE;
uint32            g_CCB_PACKED_TEMPLATE_LAYOUT_ANCHOR;
CcbPackedTemplate g_CCB_PACKED_TEMPLATE    = { CCB_STANDARD_FLAGS | CCB_PACKED_FLAG,
                                               CCB_RELATIVE_NEXT_OFFSET,
                                               CCB_PIXEL_CONTROL_OPAQUE };
uint8             g_CCB_BITS_PER_PIXEL[32] = { 0, 1, 2, 4, 6, 8, 16, 0, 0, 0, 0, 0, 0, 0, 0,  0,
                                               0, 0, 0, 0, 0, 8, 16, 0, 0, 0, 0, 0, 0, 0, 32, 0 };
COMPILE_TIME_SIZE_ASSERT(ccb_bits_per_pixel_size, sizeof(g_CCB_BITS_PER_PIXEL) == 32);
uint32      g_CCB_SPECIAL_PIXEL_CONTROL = CCB_PIXEL_CONTROL_SPECIAL;
CcbTemplate g_CCB_TEMPLATE              = { CCB_STANDARD_FLAGS,
                                            CCB_RELATIVE_NEXT_OFFSET,
                                            CCB_RELATIVE_SOURCE_OFFSET,
                                            G_CCB_DEFAULT_PALETTE,
                                            0,
                                            0,
                                            CCB_UNIT_HORIZONTAL_SCALE,
                                            0,
                                            0,
                                            CCB_UNIT_VERTICAL_SCALE,
                                            0,
                                            0,
                                            CCB_PIXEL_CONTROL_OPAQUE,
                                            CCB_PREAMBLE_0_DEFAULT,
                                            CCB_PREAMBLE_1_DEFAULT };

void
set_ccb_pixel_control(uint32 pixel_control_)
{
  g_CCB_TEMPLATE.pixel_control        = pixel_control_;
  g_CCB_PACKED_TEMPLATE.pixel_control = pixel_control_;
}

uint32
get_ccb_pixel_control(void)
{
  return g_CCB_TEMPLATE.pixel_control;
}

void
set_ccb_special_pixel_control(uint32 pixel_control_)
{
  g_CCB_SPECIAL_PIXEL_CONTROL = pixel_control_;
}

uint32
get_ccb_special_pixel_control(void)
{
  return g_CCB_SPECIAL_PIXEL_CONTROL;
}

void
set_ccb_flags(uint32 flags_)
{
  g_CCB_TEMPLATE.flags        = flags_;
  g_CCB_PACKED_TEMPLATE.flags = flags_ | CCB_PACKED_FLAG;
}

uint32
get_ccb_flags(void)
{
  return g_CCB_TEMPLATE.flags;
}

void
set_ccb_position_offset(int32 x_offset_, int32 y_offset_)
{
  g_CCB_TEMPLATE.x_position = x_offset_;
  g_CCB_TEMPLATE.y_position = y_offset_;
}

void
set_ccb_transform(int32 horizontal_dx_,
                  int32 horizontal_dy_,
                  int32 vertical_dx_,
                  int32 vertical_dy_,
                  int32 horizontal_ddx_,
                  int32 horizontal_ddy_)
{
  g_CCB_TEMPLATE.horizontal_dx  = horizontal_dx_;
  g_CCB_TEMPLATE.horizontal_dy  = horizontal_dy_;
  g_CCB_TEMPLATE.vertical_dx    = vertical_dx_;
  g_CCB_TEMPLATE.vertical_dy    = vertical_dy_;
  g_CCB_TEMPLATE.horizontal_ddx = horizontal_ddx_;
  g_CCB_TEMPLATE.horizontal_ddy = horizontal_ddy_;
}

void
initialize_ccb_manager(RenderCcb *buffer_, uint32 ccb_count_, uint32 mode_)
{
  if(buffer_ == NULL)
    {
      if(g_CCB_MANAGER_STATE.allocation_start != NULL)
        free_memory(g_CCB_MANAGER_STATE.allocation_start);

      buffer_ =
        allocate_named_memory(G_CCB_MANAGER_ALLOCATION_NAME, ccb_count_ * sizeof(RenderCcb), 0);
    }

  g_CCB_MANAGER_STATE.flags             = mode_;
  g_CCB_MANAGER_STATE.allocation_start  = buffer_;
  g_CCB_MANAGER_STATE.allocation_cursor = buffer_;
}

uint32
set_ccb_manager_mode(int32 mode_)
{
  if(mode_ != -1)
    g_CCB_MANAGER_STATE.flags = (uint32) mode_;

  return g_CCB_MANAGER_STATE.flags;
}

void
reset_ccb_chain_for_display(void)
{
  g_CCB_MANAGER_STATE.allocation_cursor = g_CCB_MANAGER_STATE.allocation_start;
  g_CCB_MANAGER_STATE.allocation_start->fields.flags |= CCB_CHAIN_INITIAL_FLAGS;
  store_display_buffer_descriptor_snapshot(g_DISPLAY_BUFFER_DESCRIPTORS);
}

void
finalize_current_ccb_chain(void)
{
  RenderCcb *last_ccb;

  last_ccb = g_CCB_MANAGER_STATE.allocation_cursor - 1;
  last_ccb->fields.flags |= CCB_CHAIN_FINAL_FLAGS;
}

void
finalize_allocated_ccb_batch(void)
{
  Item       bitmap_item;
  RenderCcb *last_ccb;

  if((g_CCB_MANAGER_STATE.flags & CCB_MANAGER_ALLOCATED_BATCH) == 0)
    return;
  if(g_CCB_MANAGER_STATE.allocation_cursor == g_CCB_MANAGER_STATE.allocation_start)
    return;

  last_ccb                              = g_CCB_MANAGER_STATE.allocation_cursor - 1;
  g_CCB_MANAGER_STATE.allocation_cursor = g_CCB_MANAGER_STATE.allocation_start;
  last_ccb->fields.flags |= CCB_LAST_FLAG;

  bitmap_item =
    (Item) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_BITMAP_ITEM_WORD];
  (void) DrawCels(bitmap_item, (CCB *) g_CCB_MANAGER_STATE.allocation_start);
}

RenderCcb *
finalize_ccb_batch(RenderCcb *last_ccb_)
{
  Item bitmap_item;

  if((g_CCB_MANAGER_STATE.flags & CCB_MANAGER_ALLOCATED_BATCH) != 0)
    return last_ccb_;
  if(last_ccb_ == NULL)
    return NULL;

  g_CCB_MANAGER_STATE.allocation_cursor = g_CCB_MANAGER_STATE.allocation_start;
  last_ccb_->fields.flags |= CCB_LAST_FLAG;

  bitmap_item =
    (Item) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_BITMAP_ITEM_WORD];
  return (RenderCcb *) DrawCels(bitmap_item, (CCB *) last_ccb_);
}

RenderCcb *
allocate_template_ccb(void)
{
  RenderCcb *ccb;

  ccb = g_CCB_MANAGER_STATE.allocation_cursor;
  g_CCB_MANAGER_STATE.allocation_cursor++;
  ccb->fields = g_CCB_TEMPLATE;
  return ccb;
}

RenderCcb *
allocate_untransformed_ccb(void)
{
  RenderCcb *ccb;

  ccb = g_CCB_MANAGER_STATE.allocation_cursor;
  g_CCB_MANAGER_STATE.allocation_cursor++;
  ccb->fields.flags         = g_CCB_TEMPLATE.flags;
  ccb->fields.next_offset   = g_CCB_TEMPLATE.next_offset;
  ccb->fields.source_offset = g_CCB_TEMPLATE.source_offset;
  ccb->fields.palette       = g_CCB_TEMPLATE.palette;
  ccb->fields.pixel_control = g_CCB_TEMPLATE.pixel_control;
  ccb->fields.preamble_0    = g_CCB_TEMPLATE.preamble_0;
  ccb->fields.preamble_1    = g_CCB_TEMPLATE.preamble_1;
  return ccb;
}

RenderCcb *
allocate_packed_ccb(const uint32 *cel_header_, uint16 *palette_)
{
  int32      format;
  RenderCcb *ccb;
  uint32     height_minus_one;

  ccb = g_CCB_MANAGER_STATE.allocation_cursor;
  g_CCB_MANAGER_STATE.allocation_cursor++;
  ccb->fields.flags         = g_CCB_PACKED_TEMPLATE.flags;
  ccb->fields.next_offset   = g_CCB_PACKED_TEMPLATE.next_offset;
  ccb->fields.source_offset = (uint32) (cel_header_ + CCB_PACKED_HEADER_WORDS);
  ccb->fields.palette       = palette_;
  ccb->fields.pixel_control = g_CCB_PACKED_TEMPLATE.pixel_control;
  format                    = ((int32) cel_header_[0] >> CCB_PACKED_FORMAT_SHIFT);
  height_minus_one          = (cel_header_[1] - 1);
  ccb->fields.preamble_0 =
    (((uint32) format & CCB_PACKED_FORMAT_MASK) | (height_minus_one << CCB_PREAMBLE_VCNT_SHIFT));
  return ccb;
}

RenderCcb *
allocate_partial_copy_ccb(const RenderCcb *source_ccb_)
{
  RenderCcb *ccb;

  ccb = g_CCB_MANAGER_STATE.allocation_cursor;
  g_CCB_MANAGER_STATE.allocation_cursor++;
  ccb->fields.flags         = source_ccb_->fields.flags;
  ccb->fields.next_offset   = source_ccb_->fields.next_offset;
  ccb->fields.source_offset = source_ccb_->fields.source_offset;
  ccb->fields.palette       = source_ccb_->fields.palette;
  ccb->fields.pixel_control = source_ccb_->fields.pixel_control;
  ccb->fields.preamble_0    = source_ccb_->fields.preamble_0;
  ccb->fields.preamble_1    = source_ccb_->fields.preamble_1;
  return ccb;
}

RenderCcb *
allocate_previous_ccb_prefix(uint16 *palette_, uint32 horizontal_dx_)
{
  RenderCcb *ccb;
  RenderCcb *previous_ccb;

  ccb = g_CCB_MANAGER_STATE.allocation_cursor;
  g_CCB_MANAGER_STATE.allocation_cursor++;
  previous_ccb              = ccb - 1;
  ccb->fields.flags         = (previous_ccb->fields.flags & ~CCB_LOAD_GEOMETRY_FLAGS);
  ccb->fields.next_offset   = previous_ccb->fields.next_offset;
  ccb->fields.source_offset = previous_ccb->fields.source_offset;
  ccb->fields.palette       = palette_;
  ccb->fields.x_position    = previous_ccb->fields.x_position;
  ccb->fields.y_position    = previous_ccb->fields.y_position;
  ccb->fields.horizontal_dx = (int32) horizontal_dx_;
  ccb->fields.horizontal_dy = (int32) previous_ccb->fields.preamble_0;
  ccb->fields.vertical_dx   = (int32) previous_ccb->fields.preamble_1;
  return ccb;
}

RenderCcb *
allocate_cel_header_ccb(const uint32 *cel_header_, int32 x_offset_, int32 y_offset_)
{
  const uint32 *chunk;
  int32         chunk_offset;
  int32         format;
  int32         row_words;
  int32         width;
  RenderCcb    *ccb;
  uint16       *palette;
  uint32        bits_per_pixel;
  uint32        dimensions;
  uint32        flags;
  uint32        height;
  uint32        pixel_control;
  uint32        preamble_1;

  ccb = g_CCB_MANAGER_STATE.allocation_cursor;
  g_CCB_MANAGER_STATE.allocation_cursor++;
  flags         = g_CCB_TEMPLATE.flags;
  palette       = g_CCB_TEMPLATE.palette;
  pixel_control = g_CCB_TEMPLATE.pixel_control;
  chunk_offset  = ((int32) (cel_header_[0] << CEL_CHUNK_OFFSET_SHIFT) >> CEL_CHUNK_OFFSET_SHIFT);
  if(chunk_offset != 0)
    {
      chunk = (const uint32 *) ((const uint8 *) cel_header_ + chunk_offset);
      while(*(const uint8 *) chunk != CEL_PALETTE_CHUNK_TYPE)
        {
          chunk_offset = ((int32) (chunk[0] << CEL_CHUNK_OFFSET_SHIFT) >> CEL_CHUNK_OFFSET_SHIFT);
          if(chunk_offset == 0)
            break;

          chunk = (const uint32 *) ((const uint8 *) chunk + chunk_offset);
        }

      if(*(const uint8 *) chunk == CEL_PALETTE_CHUNK_TYPE)
        {
          if(chunk[CEL_CHUNK_PIXC_WORD] != 0)
            pixel_control = chunk[CEL_CHUNK_PIXC_WORD];

          palette = (uint16 *) (chunk + CEL_HEADER_WORDS);
        }
    }

  format                     = ((int32) cel_header_[0] >> CCB_PACKED_FORMAT_SHIFT);
  dimensions                 = cel_header_[1];
  height                     = (dimensions & CEL_HEIGHT_MASK);
  ccb->fields.horizontal_dx  = g_CCB_TEMPLATE.horizontal_dx;
  ccb->fields.horizontal_dy  = g_CCB_TEMPLATE.horizontal_dy;
  ccb->fields.vertical_dx    = g_CCB_TEMPLATE.vertical_dx;
  ccb->fields.vertical_dy    = g_CCB_TEMPLATE.vertical_dy;
  ccb->fields.horizontal_ddx = g_CCB_TEMPLATE.horizontal_ddx;
  ccb->fields.horizontal_ddy = g_CCB_TEMPLATE.horizontal_ddy;
  ccb->fields.next_offset    = g_CCB_TEMPLATE.next_offset;
  ccb->fields.source_offset  = (uint32) (cel_header_ + CEL_HEADER_WORDS);
  ccb->fields.palette        = palette;
  ccb->fields.x_position     = (g_CCB_TEMPLATE.x_position + (x_offset_ << CCB_POSITION_SHIFT));
  ccb->fields.y_position     = (g_CCB_TEMPLATE.y_position + (y_offset_ << CCB_POSITION_SHIFT));
  if(format < 0)
    {
      if(format == CEL_FORMAT_SPECIAL_PACKED)
        pixel_control = g_CCB_SPECIAL_PIXEL_CONTROL;

      ccb->fields.flags         = (flags | CCB_PACKED_FLAG);
      ccb->fields.pixel_control = pixel_control;
      ccb->fields.preamble_0 =
        (((uint32) format & CCB_PACKED_FORMAT_MASK) | ((height - 1) << CCB_PREAMBLE_VCNT_SHIFT));
      return ccb;
    }

  if(format == CEL_FORMAT_SPECIAL_PIXC)
    pixel_control = g_CCB_SPECIAL_PIXEL_CONTROL;

  width          = ((int32) (dimensions << CEL_WIDTH_LEFT_SHIFT) >> CEL_WIDTH_RIGHT_SHIFT);
  bits_per_pixel = g_CCB_BITS_PER_PIXEL[format];
  row_words      = ((int32) (bits_per_pixel * width) - CEL_ROW_WORD_BIAS);
  if(row_words < 0)
    row_words = 0;

  row_words >>= CEL_ROW_WORD_SHIFT;
  preamble_1 = ((uint32) (width - 1) | CCB_PREAMBLE_WOFFSET10);
  if(bits_per_pixel < CEL_SMALL_BPP_LIMIT)
    preamble_1 |= ((uint32) row_words << CEL_SMALL_BPP_ROW_SHIFT);
  else
    preamble_1 |= ((uint32) row_words << CEL_LARGE_BPP_ROW_SHIFT);

  if(format == CEL_FORMAT_LINEAR)
    {
      flags &= ~CCB_ACSC_FLAG;
      preamble_1 |= CCB_PREAMBLE_LRFORM;
      height >>= 1;
    }

  ccb->fields.flags         = flags;
  ccb->fields.pixel_control = pixel_control;
  ccb->fields.preamble_0 =
    (((uint32) format & CCB_PACKED_FORMAT_MASK) | ((height - 1) << CCB_PREAMBLE_VCNT_SHIFT));
  ccb->fields.preamble_1 = preamble_1;
  return ccb;
}
