#include "shape_blit.h"

#include "ccb_manager.h"
#include "cel_access.h"
#include "cel_render.h"
#include "display_buffer_state.h"

#define CCB_BLIT_FLAG 0x00000020
#define DISPLAY_DESCRIPTOR_ROW_STRIDE_WORD 0
#define DISPLAY_DESCRIPTOR_CEL_MODE_WORD 9
#define DISPLAY_DESCRIPTOR_ROW_OFFSETS_WORD 10
#define DISPLAY_DESCRIPTOR_BITMAP_BASE_WORD 11
#define SHAPE_DIMENSIONS_WORD 1
#define SHAPE_HEADER_WORDS 4
#define SHAPE_ORIGIN_WORD 2
#define SHAPE_POSITION_WORD 3
#define SHAPE_POSITION_SHIFT 16
#define SHAPE_WIDTH_LEFT_SHIFT 4
#define SHAPE_WIDTH_RIGHT_SHIFT 20
#define SHAPE_HEIGHT_LEFT_SHIFT 20
#define SHAPE_HEIGHT_RIGHT_SHIFT 20
#define BLIT_BATCH_ROWS 999
#define BITMAP_BANK_BYTES 0x00010000
#define BITMAP_BANK_BIT 0x00010000
#define BITMAP_BANK_OFFSET_MASK 0x0000FFFF

typedef struct ShapeBlitRegion
{
  const uint8 *source;
  int32        x;
  int32        y;
  int32        width;
  int32        height;
  int32        source_row_skip;
} ShapeBlitRegion;

typedef struct ShapeBlitDestination
{
  uint8 *base;
  uint32 offset;
  int32  row_skip;
} ShapeBlitDestination;

static int32
_get_shape_position_x(uint32 position_);

static int32
_get_shape_position_y(uint32 position_);

static void
_copy_shape_rows(ShapeBlitRegion *region_);

void
blit_shape_at(const uint32 *shape_, int32 x_, int32 y_)
{
  ShapeBlitRegion region;
  RenderCcb      *ccb;
  int32           clipped_pixels;
  uint32          dimensions;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_DESCRIPTOR_CEL_MODE_WORD] != 0)
    {
      ccb = allocate_cel_header_ccb(shape_, x_, y_);
      clip_render_ccb_to_display(ccb);
      ccb->fields.flags |= CCB_BLIT_FLAG;
      finalize_ccb_batch(ccb);
      return;
    }

  dimensions    = shape_[SHAPE_DIMENSIONS_WORD];
  region.source = (const uint8 *) (shape_ + SHAPE_HEADER_WORDS);
  region.x      = x_;
  region.y      = y_;
  region.width  = (int32) (dimensions << SHAPE_WIDTH_LEFT_SHIFT) >> SHAPE_WIDTH_RIGHT_SHIFT;
  region.height = (int32) (dimensions << SHAPE_HEIGHT_LEFT_SHIFT) >> SHAPE_HEIGHT_RIGHT_SHIFT;
  region.source_row_skip = 0;

  clipped_pixels =
    ((int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_Y_WORD] -
     region.y);
  if(clipped_pixels > 0)
    {
      region.source += (clipped_pixels * region.width);
      region.height -= clipped_pixels;
      region.y += clipped_pixels;
    }

  clipped_pixels =
    (region.y + region.height -
     (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_HEIGHT_WORD]);
  if(clipped_pixels > 0)
    region.height -= clipped_pixels;

  clipped_pixels =
    ((int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_X_WORD] -
     region.x);
  if(clipped_pixels > 0)
    {
      region.source += clipped_pixels;
      region.width -= clipped_pixels;
      region.x += clipped_pixels;
      region.source_row_skip = clipped_pixels;
    }

  clipped_pixels =
    (region.x + region.width -
     (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_WORD]);
  if(clipped_pixels > 0)
    {
      region.source_row_skip += clipped_pixels;
      region.width -= clipped_pixels;
    }

  if((region.width <= 0) || (region.height <= 0))
    return;

  _copy_shape_rows(&region);
}

void
blit_shape_at_alias(const uint32 *shape_, int32 x_, int32 y_)
{
  blit_shape_at(shape_, x_, y_);
}

void
blit_shape_at_embedded_position(const uint32 *shape_)
{
  uint32 position;

  position = shape_[SHAPE_POSITION_WORD];
  blit_shape_at(shape_, _get_shape_position_x(position), _get_shape_position_y(position));
}

void
blit_shape_at_embedded_position_alias(const uint32 *shape_)
{
  uint32 position;

  position = shape_[SHAPE_POSITION_WORD];
  blit_shape_at(shape_, _get_shape_position_x(position), _get_shape_position_y(position));
}

void
blit_shape_relative_to_embedded_origin(const uint32 *shape_, int32 x_, int32 y_)
{
  uint32 origin;

  origin = shape_[SHAPE_ORIGIN_WORD];
  blit_shape_at(shape_, x_ - _get_shape_position_x(origin), y_ - _get_shape_position_y(origin));
}

void
blit_shape_relative_to_embedded_origin_alias(const uint32 *shape_, int32 x_, int32 y_)
{
  uint32 origin;

  origin = shape_[SHAPE_ORIGIN_WORD];
  blit_shape_at(shape_, x_ - _get_shape_position_x(origin), y_ - _get_shape_position_y(origin));
}

static int32
_get_shape_position_x(uint32 position_)
{
  return (int32) position_ >> SHAPE_POSITION_SHIFT;
}

static int32
_get_shape_position_y(uint32 position_)
{
  return (int32) (int16) position_;
}

/* Keeps the inlined callback accesses in the helper directly owned by blit_shape_at. */
static void
_copy_shape_rows(ShapeBlitRegion *region_)
{
  ShapeBlitDestination destination;
  uint32              *row_offsets;
  int32                 batch_rows;
  int32                 first_copy_bytes;
  int32                 row_count;
  uint32                row_end;
  uint32                second_copy_bytes;

  destination.base =
    (uint8 *) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_DESCRIPTOR_BITMAP_BASE_WORD];
  row_offsets =
    (uint32 *) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_DESCRIPTOR_ROW_OFFSETS_WORD];
  destination.offset = ((row_offsets[region_->y] + region_->x) & BITMAP_BANK_OFFSET_MASK);
  destination.row_skip =
    ((int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_DESCRIPTOR_ROW_STRIDE_WORD] -
     region_->width);
  while(region_->height > 0)
    {
      batch_rows = region_->height;
      if(batch_rows >= BLIT_BATCH_ROWS)
        batch_rows = BLIT_BATCH_ROWS;

      region_->height -= batch_rows;
      row_count = batch_rows;
      while(row_count > 0)
        {
          g_BITMAP_COPY_CALLBACK(region_->source,
                                 destination.base + destination.offset,
                                 (uint32) region_->width);
          destination.offset += (region_->width + destination.row_skip);
          region_->source += (region_->width + region_->source_row_skip);
          row_count--;
        }

      if(region_->height <= 0)
        return;

      if((destination.offset & BITMAP_BANK_BIT) != 0)
        destination.offset &= BITMAP_BANK_OFFSET_MASK;

      row_end = (destination.offset + region_->width);
      if(((row_end - 1U) & BITMAP_BANK_BIT) == 0)
        {
          g_BITMAP_COPY_CALLBACK(region_->source,
                                 destination.base + destination.offset,
                                 (uint32) region_->width);
          region_->source += region_->width;
          destination.offset = row_end;
        }
      else
        {
          first_copy_bytes = (BITMAP_BANK_BYTES - destination.offset);
          g_BITMAP_COPY_CALLBACK(region_->source,
                                 destination.base + destination.offset,
                                 (uint32) first_copy_bytes);
          region_->source += first_copy_bytes;
          second_copy_bytes = (row_end & BITMAP_BANK_OFFSET_MASK);
          g_BITMAP_COPY_CALLBACK(region_->source, destination.base, second_copy_bytes);
          region_->source += second_copy_bytes;
          destination.offset = second_copy_bytes;
        }

      destination.offset += destination.row_skip;
      region_->source += region_->source_row_skip;
      if((destination.offset & BITMAP_BANK_BIT) != 0)
        destination.offset &= BITMAP_BANK_OFFSET_MASK;

      region_->height--;
    }
}
