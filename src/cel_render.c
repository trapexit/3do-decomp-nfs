#include "cel_render.h"

#include "cel_access.h"
#include "diagnostics.h"
#include "display_buffer_state.h"
#include "fixed_math.h"
#include "geometry.h"
#include "integer_division.h"
#include "memory.h"
#include "shape_blit.h"
#include "transparent_bytes.h"

#include "hardware.h"
#include "operamath.h"

#define CEL_HEADER_ORIGIN_WORD 2
#define CEL_HEADER_POSITION_WORD 3
#define CEL_HEADER_DIMENSIONS_WORD 1
#define CEL_HEADER_DIMENSION_BITS 12
#define CEL_HEADER_WIDTH_SHIFT 16
#define CEL_POSITION_SHIFT 16
#define CEL_WORD_BITS 32
#define CEL_QUAD_TOP_LEFT 0
#define CEL_QUAD_TOP_RIGHT 1
#define CEL_QUAD_BOTTOM_RIGHT 2
#define CEL_QUAD_BOTTOM_LEFT 3
#define CEL_QUAD_POINT_COUNT 4
#define CEL_HORIZONTAL_SCALE_ONE 0x100000
#define CEL_VERTICAL_SCALE_ONE 0x10000
#define CEL_SCALE_HORIZONTAL_CONVERSION 16
#define CEL_BACKGROUND_HORIZONTAL_SHIFT 4
#define CEL_BACKGROUND_DDX_SHIFT 12
#define DISPLAY_BUFFER_DESCRIPTOR_CEL_MODE_WORD 9
#define DISPLAY_BUFFER_DESCRIPTOR_ROW_OFFSETS_WORD 10
#define DISPLAY_BUFFER_DESCRIPTOR_BITMAP_BASE_WORD 11
#define TRANSFORMED_CCB_FLAGS_MASK 0x9FFFFFFF
#define TRANSFORMED_CCB_NEXT_OFFSET 0x38
#define TRANSFORMED_CCB_DIMENSION_SCALE 0x10000
#define TRANSFORMED_CCB_HORIZONTAL_FRACTION_SHIFT 4
#define TRANSFORMED_CCB_DDX_FRACTION_SHIFT 12

#define SCALE_CEL_FRAME_WINDOW_ERROR "scalecf - 8 BIT WINDOWS NOT SUPPORTED\n"
#define SCALE_FRAME_WINDOW_ERROR "scalef - 8 BIT WINDOWS NOT SUPPORTED\n"

BitmapCopyCallback g_BITMAP_COPY_CALLBACK = move_bytes;

void
copy_unclipped_cel_header_rows(const uint32 *cel_header_, const uint8 *source_, int32 row_);

static int32
_get_packed_position_x(uint32 position_)
{
  return (int32) position_ >> CEL_POSITION_SHIFT;
}

static int32
_get_packed_position_y(uint32 position_)
{
  return (int32) (int16) position_;
}

static int32
_get_cel_header_width(const uint32 *cel_header_)
{
  uint32 dimensions;

  dimensions = cel_header_[CEL_HEADER_DIMENSIONS_WORD];
  return (int32) (dimensions << (CEL_WORD_BITS - CEL_HEADER_WIDTH_SHIFT -
                                 CEL_HEADER_DIMENSION_BITS)) >>
         (CEL_WORD_BITS - CEL_HEADER_DIMENSION_BITS);
}

static int32
_get_cel_header_height(const uint32 *cel_header_)
{
  uint32 dimensions;

  dimensions = cel_header_[CEL_HEADER_DIMENSIONS_WORD];
  return (int32) (dimensions << (CEL_WORD_BITS - CEL_HEADER_DIMENSION_BITS)) >>
         (CEL_WORD_BITS - CEL_HEADER_DIMENSION_BITS);
}

void
draw_cel_header_pixels_at_embedded_position(const uint32 *cel_header_)
{
  uint32 position;

  position = cel_header_[CEL_HEADER_POSITION_WORD];
  draw_cel_header_pixels(cel_header_,
                         _get_packed_position_x(position),
                         _get_packed_position_y(position));
}

void
draw_cel_header_pixels_relative_to_embedded_origin(const uint32 *cel_header_, int32 x_, int32 y_)
{
  uint32 origin;

  origin = cel_header_[CEL_HEADER_ORIGIN_WORD];
  draw_cel_header_pixels(cel_header_,
                         x_ - _get_packed_position_x(origin),
                         y_ - _get_packed_position_y(origin));
}

void
draw_cel_header_pixels(const uint32 *cel_header_, int32 x_, int32 y_)
{
  RenderCcb *ccb;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_CEL_MODE_WORD] != 0)
    {
      ccb = allocate_cel_header_ccb(cel_header_, x_, y_);
      ccb->fields.flags ^= CCB_BGND;
      ccb->fields.preamble_0 ^= PRE0_BGND;
      finalize_ccb_batch(ccb);
      return;
    }

  /*
   * The original software path interprets its X argument as a source address.
   */
  copy_unclipped_cel_header_rows(cel_header_,
                                 (const uint8 *) memory_address_to_pointer((uint32) x_),
                                 y_);
}

void
copy_unclipped_cel_header_rows(const uint32 *cel_header_, const uint8 *source_, int32 row_)
{
  uint8  *destination;
  uint32 *row_offsets;
  int32   height;
  int32   row_stride;
  int32   width;

  width  = (int32) cel_header_[CEL_HEADER_DIMENSIONS_WORD] >> CEL_HEADER_WIDTH_SHIFT;
  height = (int32) (int16) cel_header_[CEL_HEADER_DIMENSIONS_WORD];
  if((width <= 0) || (height <= 0))
    return;

  row_stride =
    (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_BUFFER_WIDTH_WORD];
  row_offsets =
    (uint32 *)
      g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_ROW_OFFSETS_WORD];
  destination =
    (uint8 *)
      g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_BITMAP_BASE_WORD] +
    row_offsets[row_];

  while(height > 0)
    {
      g_BITMAP_COPY_CALLBACK(source_, destination, (uint32) width);
      source_ += width;
      destination += row_stride;
      height--;
    }
}

void *
draw_cel_header(const uint32 *cel_header_, int32 x_, int32 y_)
{
  RenderCcb *ccb;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_CEL_MODE_WORD] != 0)
    {
      ccb = allocate_cel_header_ccb(cel_header_, x_, y_);
      clip_render_ccb_to_display(ccb);
      return finalize_ccb_batch(ccb);
    }

  g_BITMAP_COPY_CALLBACK = copy_non_transparent_bytes;
  blit_shape_at(cel_header_, x_, y_);
  g_BITMAP_COPY_CALLBACK = move_bytes;
  return (void *) g_BITMAP_COPY_CALLBACK;
}

void *
draw_cel_header_at_embedded_position(const uint32 *cel_header_)
{
  uint32 position;

  position = cel_header_[CEL_HEADER_POSITION_WORD];
  return draw_cel_header(cel_header_,
                         _get_packed_position_x(position),
                         _get_packed_position_y(position));
}

void *
draw_cel_header_relative_to_embedded_origin(const uint32 *cel_header_, int32 x_, int32 y_)
{
  uint32 origin;

  origin = cel_header_[CEL_HEADER_ORIGIN_WORD];
  return draw_cel_header(cel_header_,
                         x_ - _get_packed_position_x(origin),
                         y_ - _get_packed_position_y(origin));
}

void *
draw_cel_header_alias(const uint32 *cel_header_, int32 x_, int32 y_)
{
  return draw_cel_header(cel_header_, x_, y_);
}

void *
draw_cel_header_at_embedded_position_alias(const uint32 *cel_header_)
{
  uint32 position;

  position = cel_header_[CEL_HEADER_POSITION_WORD];
  return draw_cel_header(cel_header_,
                         _get_packed_position_x(position),
                         _get_packed_position_y(position));
}

void *
draw_cel_header_relative_to_embedded_origin_alias(const uint32 *cel_header_, int32 x_, int32 y_)
{
  uint32 origin;

  origin = cel_header_[CEL_HEADER_ORIGIN_WORD];
  return draw_cel_header(cel_header_,
                         x_ - _get_packed_position_x(origin),
                         y_ - _get_packed_position_y(origin));
}

void *
draw_unclipped_cel_header(const uint32 *cel_header_, int32 x_, int32 y_)
{
  RenderCcb *ccb;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_CEL_MODE_WORD] != 0)
    {
      ccb = allocate_cel_header_ccb(cel_header_, x_, y_);
      if(ccb == NULL)
        return NULL;

      finalize_ccb_batch(ccb);
      return ccb;
    }

  g_BITMAP_COPY_CALLBACK = copy_non_transparent_bytes;
  draw_cel_header_pixels(cel_header_, x_, y_);
  g_BITMAP_COPY_CALLBACK = move_bytes;
  return (void *) g_BITMAP_COPY_CALLBACK;
}

void *
draw_unclipped_cel_header_at_embedded_position(const uint32 *cel_header_)
{
  uint32 position;

  position = cel_header_[CEL_HEADER_POSITION_WORD];
  return draw_unclipped_cel_header(cel_header_,
                                   _get_packed_position_x(position),
                                   _get_packed_position_y(position));
}

void *
draw_unclipped_cel_header_relative_to_embedded_origin(const uint32 *cel_header_, int32 x_, int32 y_)
{
  uint32 origin;

  origin = cel_header_[CEL_HEADER_ORIGIN_WORD];
  return draw_unclipped_cel_header(cel_header_,
                                   x_ - _get_packed_position_x(origin),
                                   y_ - _get_packed_position_y(origin));
}

void *
draw_unclipped_cel_header_alias(const uint32 *cel_header_, int32 x_, int32 y_)
{
  return draw_unclipped_cel_header(cel_header_, x_, y_);
}

void *
draw_unclipped_cel_header_at_embedded_position_alias(const uint32 *cel_header_)
{
  uint32 position;

  position = cel_header_[CEL_HEADER_POSITION_WORD];
  return draw_unclipped_cel_header(cel_header_,
                                   _get_packed_position_x(position),
                                   _get_packed_position_y(position));
}

void *
draw_unclipped_cel_header_relative_to_embedded_origin_alias(const uint32 *cel_header_,
                                                            int32         x_,
                                                            int32         y_)
{
  uint32 origin;

  origin = cel_header_[CEL_HEADER_ORIGIN_WORD];
  return draw_unclipped_cel_header(cel_header_,
                                   x_ - _get_packed_position_x(origin),
                                   y_ - _get_packed_position_y(origin));
}

RenderCcb *
draw_scaled_cel_frame(int32 scale_, const uint32 *cel_header_, int32 x_, int32 y_)
{
  RenderCcb *ccb;
  int32      vertical_scale;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_CEL_MODE_WORD] == 0)
    {
      fatal_error(SCALE_CEL_FRAME_WINDOW_ERROR);
      return NULL;
    }

  ccb = allocate_cel_header_ccb(cel_header_, x_, y_);
  if(ccb == NULL)
    return NULL;

  ccb->fields.horizontal_dx = (scale_ * CEL_SCALE_HORIZONTAL_CONVERSION);
  if(scale_ >= 0)
    {
      ccb->fields.vertical_dy = scale_;
    }
  else
    {
      vertical_scale          = -scale_;
      ccb->fields.vertical_dy = vertical_scale;
      ccb->fields.x_position += (vertical_scale * _get_cel_header_width(cel_header_));
    }

  return finalize_ccb_batch(ccb);
}

RenderCcb *
draw_scaled_cel_frame_at_embedded_position(int32 scale_, const uint32 *cel_header_)
{
  uint32 position;

  position = cel_header_[CEL_HEADER_POSITION_WORD];
  return draw_scaled_cel_frame(scale_,
                               cel_header_,
                               _get_packed_position_x(position),
                               _get_packed_position_y(position));
}

RenderCcb *
draw_scaled_cel_frame_relative_to_embedded_origin(int32         scale_,
                                                  const uint32 *cel_header_,
                                                  int32         x_,
                                                  int32         y_)
{
  int32  origin_x;
  int32  origin_y;
  int32  positive_scale;
  uint32 origin;

  origin         = cel_header_[CEL_HEADER_ORIGIN_WORD];
  origin_x       = _get_packed_position_x(origin);
  origin_y       = _get_packed_position_y(origin);
  positive_scale = scale_;
  if(scale_ < 0)
    {
      positive_scale = -scale_;
      origin_x       = (_get_cel_header_width(cel_header_) - origin_x);
    }

  x_ -= multiply_fixed16(origin_x, positive_scale);
  y_ -= multiply_fixed16(origin_y, positive_scale);
  return draw_scaled_cel_frame(scale_, cel_header_, x_, y_);
}

RenderCcb *
draw_scaled_cel_frame_alias(int32 scale_, const uint32 *cel_header_, int32 x_, int32 y_)
{
  return draw_scaled_cel_frame(scale_, cel_header_, x_, y_);
}

RenderCcb *
draw_scaled_cel_frame_at_embedded_position_alias(int32 scale_, const uint32 *cel_header_)
{
  uint32 position;

  position = cel_header_[CEL_HEADER_POSITION_WORD];
  return draw_scaled_cel_frame(scale_,
                               cel_header_,
                               (int32) position >> CEL_POSITION_SHIFT,
                               (int32) (int16) position);
}

RenderCcb *
draw_scaled_cel_frame_relative_to_embedded_origin_alias(int32         scale_,
                                                        const uint32 *cel_header_,
                                                        int32         x_,
                                                        int32         y_)
{
  int32  origin_x;
  int32  origin_y;
  int32  positive_scale;
  uint32 origin;

  origin         = cel_header_[CEL_HEADER_ORIGIN_WORD];
  origin_x       = _get_packed_position_x(origin);
  origin_y       = _get_packed_position_y(origin);
  positive_scale = scale_;
  if(scale_ < 0)
    {
      positive_scale = -scale_;
      origin_x       = (_get_cel_header_width(cel_header_) - origin_x);
    }

  x_ -= multiply_fixed16(origin_x, positive_scale);
  y_ -= multiply_fixed16(origin_y, positive_scale);
  return draw_scaled_cel_frame(scale_, cel_header_, x_, y_);
}

RenderCcb *
draw_scaled_frame(int32 scale_, const uint32 *cel_header_, int32 x_, int32 y_)
{
  RenderCcb *ccb;
  int32      vertical_scale;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_CEL_MODE_WORD] == 0)
    {
      fatal_error(SCALE_FRAME_WINDOW_ERROR);
      return NULL;
    }

  ccb = allocate_cel_header_ccb(cel_header_, x_, y_);
  if(ccb == NULL)
    return NULL;

  ccb->fields.horizontal_dx = (scale_ * CEL_SCALE_HORIZONTAL_CONVERSION);
  if(scale_ >= 0)
    {
      ccb->fields.vertical_dy = scale_;
    }
  else
    {
      vertical_scale          = -scale_;
      ccb->fields.vertical_dy = vertical_scale;
      ccb->fields.x_position += (vertical_scale * _get_cel_header_width(cel_header_));
    }

  return finalize_ccb_batch(ccb);
}

RenderCcb *
draw_scaled_frame_at_embedded_position(int32 scale_, const uint32 *cel_header_)
{
  uint32 position;

  position = cel_header_[CEL_HEADER_POSITION_WORD];
  return draw_scaled_frame(scale_,
                           cel_header_,
                           _get_packed_position_x(position),
                           _get_packed_position_y(position));
}

RenderCcb *
draw_scaled_frame_relative_to_embedded_origin(int32         scale_,
                                              const uint32 *cel_header_,
                                              int32         x_,
                                              int32         y_)
{
  int32  origin_x;
  int32  origin_y;
  int32  positive_scale;
  uint32 origin;

  origin         = cel_header_[CEL_HEADER_ORIGIN_WORD];
  origin_x       = _get_packed_position_x(origin);
  origin_y       = _get_packed_position_y(origin);
  positive_scale = scale_;
  if(scale_ < 0)
    {
      positive_scale = -scale_;
      origin_x       = (_get_cel_header_width(cel_header_) - origin_x);
    }

  x_ -= multiply_fixed16(origin_x, positive_scale);
  y_ -= multiply_fixed16(origin_y, positive_scale);
  return draw_scaled_frame(scale_, cel_header_, x_, y_);
}

RenderCcb *
draw_scaled_frame_alias(int32 scale_, const uint32 *cel_header_, int32 x_, int32 y_)
{
  return draw_scaled_frame(scale_, cel_header_, x_, y_);
}

RenderCcb *
draw_scaled_frame_at_embedded_position_alias(int32 scale_, const uint32 *cel_header_)
{
  uint32 position;

  position = cel_header_[CEL_HEADER_POSITION_WORD];
  return draw_scaled_frame(scale_,
                           cel_header_,
                           (int32) position >> CEL_POSITION_SHIFT,
                           (int32) (int16) position);
}

RenderCcb *
draw_scaled_frame_relative_to_embedded_origin_alias(int32         scale_,
                                                    const uint32 *cel_header_,
                                                    int32         x_,
                                                    int32         y_)
{
  int32  origin_x;
  int32  origin_y;
  int32  positive_scale;
  uint32 origin;

  origin         = cel_header_[CEL_HEADER_ORIGIN_WORD];
  origin_x       = _get_packed_position_x(origin);
  origin_y       = _get_packed_position_y(origin);
  positive_scale = scale_;
  if(scale_ < 0)
    {
      positive_scale = -scale_;
      origin_x       = (_get_cel_header_width(cel_header_) - origin_x);
    }

  x_ -= multiply_fixed16(origin_x, positive_scale);
  y_ -= multiply_fixed16(origin_y, positive_scale);
  return draw_scaled_frame(scale_, cel_header_, x_, y_);
}

void
map_fixed16_quad_to_sized_ccb(RenderCcb *ccb_, const Point2 *quad_)
{
  CCB  *sdk_ccb;
  int32 bottom_horizontal_dx;
  int32 bottom_horizontal_dy;
  int32 horizontal_dx;
  int32 horizontal_dy;
  int32 horizontal_scale;
  int32 vertical_scale;

  sdk_ccb = (CCB *) ccb_;
  vertical_scale =
    (int32) divide_unsigned_integer(TRANSFORMED_CCB_DIMENSION_SCALE, (uint32) sdk_ccb->ccb_Height);
  horizontal_scale =
    (int32) divide_unsigned_integer(TRANSFORMED_CCB_DIMENSION_SCALE, (uint32) sdk_ccb->ccb_Width);

  horizontal_dx = multiply_fixed16(
    (int32) ((uint32) quad_[CEL_QUAD_TOP_RIGHT].x - (uint32) quad_[CEL_QUAD_TOP_LEFT].x),
    horizontal_scale);
  horizontal_dy = multiply_fixed16(
    (int32) ((uint32) quad_[CEL_QUAD_TOP_RIGHT].y - (uint32) quad_[CEL_QUAD_TOP_LEFT].y),
    horizontal_scale);
  bottom_horizontal_dx = multiply_fixed16(
    (int32) ((uint32) quad_[CEL_QUAD_BOTTOM_RIGHT].x - (uint32) quad_[CEL_QUAD_BOTTOM_LEFT].x),
    horizontal_scale);
  bottom_horizontal_dy = multiply_fixed16(
    (int32) ((uint32) quad_[CEL_QUAD_BOTTOM_RIGHT].y - (uint32) quad_[CEL_QUAD_BOTTOM_LEFT].y),
    horizontal_scale);

  ccb_->fields.x_position = quad_[CEL_QUAD_TOP_LEFT].x;
  ccb_->fields.y_position = quad_[CEL_QUAD_TOP_LEFT].y;
  ccb_->fields.horizontal_dx =
    (int32) ((uint32) horizontal_dx << TRANSFORMED_CCB_HORIZONTAL_FRACTION_SHIFT);
  ccb_->fields.horizontal_dy =
    (int32) ((uint32) horizontal_dy << TRANSFORMED_CCB_HORIZONTAL_FRACTION_SHIFT);
  ccb_->fields.vertical_dx = multiply_fixed16(
    (int32) ((uint32) quad_[CEL_QUAD_BOTTOM_LEFT].x - (uint32) quad_[CEL_QUAD_TOP_LEFT].x),
    vertical_scale);
  ccb_->fields.vertical_dy = multiply_fixed16(
    (int32) ((uint32) quad_[CEL_QUAD_BOTTOM_LEFT].y - (uint32) quad_[CEL_QUAD_TOP_LEFT].y),
    vertical_scale);
  ccb_->fields.horizontal_ddx =
    multiply_fixed16((int32) ((uint32) bottom_horizontal_dx - (uint32) horizontal_dx),
                     vertical_scale) >>
    TRANSFORMED_CCB_DDX_FRACTION_SHIFT;
  ccb_->fields.horizontal_ddy =
    multiply_fixed16((int32) ((uint32) bottom_horizontal_dy - (uint32) horizontal_dy),
                     vertical_scale) >>
    TRANSFORMED_CCB_DDX_FRACTION_SHIFT;
}

void
draw_transformed_ccb(CCB  *source_ccb_,
                     int32 x_,
                     int32 y_,
                     int32 half_width_,
                     int32 half_height_,
                     int32 rotation_,
                     int32 horizontal_scale_,
                     int32 vertical_scale_)
{
  RenderCcb *ccb;
  CCB       *sdk_ccb;
  Point2     quad[CEL_QUAD_POINT_COUNT];
  int32      bottom;
  int32      cosine;
  int32      left;
  int32      right;
  int32      sine;
  int32      top;
  int32      transformed_x;
  int32      transformed_y;

  ccb                     = allocate_partial_copy_ccb((const RenderCcb *) source_ccb_);
  sdk_ccb                 = (CCB *) ccb;
  ccb->fields.next_offset = TRANSFORMED_CCB_NEXT_OFFSET;
  ccb->fields.flags       = source_ccb_->ccb_Flags & TRANSFORMED_CCB_FLAGS_MASK;
  sdk_ccb->ccb_PRE0       = source_ccb_->ccb_PRE0;
  sdk_ccb->ccb_PRE1       = source_ccb_->ccb_PRE1;

  /* The original 64-byte render pool temporarily borrows the following word
   * for the SDK CCB height while this mapping routine consumes it. */
  sdk_ccb->ccb_Width  = source_ccb_->ccb_Width;
  sdk_ccb->ccb_Height = source_ccb_->ccb_Height;

  left   = -half_width_;
  top    = -half_height_;
  right  = (int32) ((uint32) source_ccb_->ccb_Width * TRANSFORMED_CCB_DIMENSION_SCALE -
                    (uint32) half_width_);
  bottom = (int32) ((uint32) source_ccb_->ccb_Height * TRANSFORMED_CCB_DIMENSION_SCALE -
                    (uint32) half_height_);
  cosine = CosF16((frac16) rotation_);
  sine   = SinF16((frac16) rotation_);

#define SET_TRANSFORMED_QUAD_POINT(index_, source_x_, source_y_)                                   \
  transformed_x = (int32) ((uint32) multiply_fixed16((source_x_), cosine) -                        \
                           (uint32) multiply_fixed16((source_y_), sine));                          \
  transformed_y = (int32) ((uint32) multiply_fixed16((source_x_), sine) +                          \
                           (uint32) multiply_fixed16((source_y_), cosine));                        \
  quad[(index_)].x =                                                                               \
    (int32) ((uint32) multiply_fixed16(transformed_x, horizontal_scale_) + (uint32) x_);           \
  quad[(index_)].y =                                                                               \
    (int32) ((uint32) multiply_fixed16(transformed_y, vertical_scale_) + (uint32) y_)

  SET_TRANSFORMED_QUAD_POINT(CEL_QUAD_TOP_LEFT, left, top);
  SET_TRANSFORMED_QUAD_POINT(CEL_QUAD_TOP_RIGHT, right, top);
  SET_TRANSFORMED_QUAD_POINT(CEL_QUAD_BOTTOM_RIGHT, right, bottom);
  SET_TRANSFORMED_QUAD_POINT(CEL_QUAD_BOTTOM_LEFT, left, bottom);

#undef SET_TRANSFORMED_QUAD_POINT

  map_fixed16_quad_to_sized_ccb(ccb, quad);
}

RenderCcb *
draw_transformed_cel_header(const uint32 *cel_header_,
                            int32         x_,
                            int32         y_,
                            int32         horizontal_dx_,
                            int32         horizontal_dy_,
                            int32         vertical_dx_,
                            int32         vertical_dy_,
                            int32         horizontal_ddx_,
                            int32         horizontal_ddy_)
{
  RenderCcb *ccb;

  ccb                        = allocate_cel_header_ccb(cel_header_, x_, y_);
  ccb->fields.horizontal_dx  = horizontal_dx_;
  ccb->fields.horizontal_dy  = horizontal_dy_;
  ccb->fields.vertical_dx    = vertical_dx_;
  ccb->fields.vertical_dy    = vertical_dy_;
  ccb->fields.horizontal_ddx = horizontal_ddx_;
  ccb->fields.horizontal_ddy = horizontal_ddy_;
  return finalize_ccb_batch(ccb);
}

RenderCcb *
draw_cel_header_to_quad_coordinates(const uint32 *cel_header_,
                                    int32         top_left_x_,
                                    int32         top_left_y_,
                                    int32         top_right_x_,
                                    int32         top_right_y_,
                                    int32         bottom_right_x_,
                                    int32         bottom_right_y_,
                                    int32         bottom_left_x_,
                                    int32         bottom_left_y_)
{
  int32 height;
  int32 horizontal_ddx;
  int32 horizontal_ddy;
  int32 horizontal_dx;
  int32 horizontal_dy;
  int32 horizontal_scale;
  int32 vertical_dx;
  int32 vertical_dy;
  int32 vertical_scale;
  int32 width;

  height           = _get_cel_header_height(cel_header_);
  width            = _get_cel_header_width(cel_header_);
  vertical_scale   = (CEL_VERTICAL_SCALE_ONE / height);
  horizontal_scale = (CEL_HORIZONTAL_SCALE_ONE / width);
  horizontal_dx    = (horizontal_scale * (top_right_x_ - top_left_x_));
  horizontal_dy    = (horizontal_scale * (top_right_y_ - top_left_y_));
  vertical_dx      = (vertical_scale * (bottom_left_x_ - top_left_x_));
  vertical_dy      = (vertical_scale * (bottom_left_y_ - top_left_y_));
  horizontal_ddx =
    ((horizontal_scale * (bottom_right_x_ - bottom_left_x_) - horizontal_dx) / height);
  horizontal_ddy =
    ((horizontal_scale * (bottom_right_y_ - bottom_left_y_) - horizontal_dy) / height);
  return draw_transformed_cel_header(cel_header_,
                                     top_left_x_,
                                     top_left_y_,
                                     horizontal_dx,
                                     horizontal_dy,
                                     vertical_dx,
                                     vertical_dy,
                                     horizontal_ddx,
                                     horizontal_ddy);
}

RenderCcb *
draw_cel_header_to_quad(const uint32 *cel_header_, const Point *quad_)
{
  int32 height;
  int32 horizontal_ddx;
  int32 horizontal_ddy;
  int32 horizontal_dx;
  int32 horizontal_dy;
  int32 horizontal_scale;
  int32 vertical_dx;
  int32 vertical_dy;
  int32 vertical_scale;
  int32 width;

  height           = _get_cel_header_height(cel_header_);
  width            = _get_cel_header_width(cel_header_);
  vertical_scale   = (CEL_VERTICAL_SCALE_ONE / height);
  horizontal_scale = (CEL_HORIZONTAL_SCALE_ONE / width);
  horizontal_dx =
    (horizontal_scale * (quad_[CEL_QUAD_TOP_RIGHT].pt_X - quad_[CEL_QUAD_TOP_LEFT].pt_X));
  horizontal_dy =
    (horizontal_scale * (quad_[CEL_QUAD_TOP_RIGHT].pt_Y - quad_[CEL_QUAD_TOP_LEFT].pt_Y));
  vertical_dx =
    (vertical_scale * (quad_[CEL_QUAD_BOTTOM_LEFT].pt_X - quad_[CEL_QUAD_TOP_LEFT].pt_X));
  vertical_dy =
    (vertical_scale * (quad_[CEL_QUAD_BOTTOM_LEFT].pt_Y - quad_[CEL_QUAD_TOP_LEFT].pt_Y));
  horizontal_ddx =
    ((horizontal_scale * (quad_[CEL_QUAD_BOTTOM_RIGHT].pt_X - quad_[CEL_QUAD_BOTTOM_LEFT].pt_X) -
      horizontal_dx) /
     height);
  horizontal_ddy =
    ((horizontal_scale * (quad_[CEL_QUAD_BOTTOM_RIGHT].pt_Y - quad_[CEL_QUAD_BOTTOM_LEFT].pt_Y) -
      horizontal_dy) /
     height);
  return draw_transformed_cel_header(cel_header_,
                                     quad_[CEL_QUAD_TOP_LEFT].pt_X,
                                     quad_[CEL_QUAD_TOP_LEFT].pt_Y,
                                     horizontal_dx,
                                     horizontal_dy,
                                     vertical_dx,
                                     vertical_dy,
                                     horizontal_ddx,
                                     horizontal_ddy);
}

void
draw_background_cel_header_to_quad(const uint32 *cel_header_, const Point *quad_)
{
  RenderCcb *ccb;
  int32      bottom_horizontal_dx;
  int32      bottom_horizontal_dy;
  int32      height;
  int32      horizontal_dx;
  int32      horizontal_dy;
  int32      horizontal_scale;
  int32      vertical_scale;
  int32      width;

  ccb = allocate_cel_header_ccb(cel_header_,
                                quad_[CEL_QUAD_TOP_LEFT].pt_X,
                                quad_[CEL_QUAD_TOP_LEFT].pt_Y);
  ccb->fields.flags |= CCB_BGND;

  height         = _get_cel_header_height(cel_header_);
  vertical_scale = (int32) divide_unsigned_integer(CEL_VERTICAL_SCALE_ONE, (uint32) height);
  width          = _get_cel_header_width(cel_header_);
  if(width == height)
    {
      horizontal_scale = vertical_scale;
    }
  else
    {
      horizontal_scale = (int32) divide_unsigned_integer(CEL_VERTICAL_SCALE_ONE, (uint32) width);
    }

  horizontal_dx = (int32) ((uint32) horizontal_scale * (uint32) (quad_[CEL_QUAD_TOP_RIGHT].pt_X -
                                                                 quad_[CEL_QUAD_TOP_LEFT].pt_X));
  horizontal_dy = (int32) ((uint32) horizontal_scale * (uint32) (quad_[CEL_QUAD_TOP_RIGHT].pt_Y -
                                                                 quad_[CEL_QUAD_TOP_LEFT].pt_Y));
  bottom_horizontal_dx =
    (int32) ((uint32) horizontal_scale *
             (uint32) (quad_[CEL_QUAD_BOTTOM_RIGHT].pt_X - quad_[CEL_QUAD_BOTTOM_LEFT].pt_X));
  bottom_horizontal_dy =
    (int32) ((uint32) horizontal_scale *
             (uint32) (quad_[CEL_QUAD_BOTTOM_RIGHT].pt_Y - quad_[CEL_QUAD_BOTTOM_LEFT].pt_Y));

  ccb->fields.horizontal_dx = (int32) ((uint32) horizontal_dx << CEL_BACKGROUND_HORIZONTAL_SHIFT);
  ccb->fields.horizontal_dy = (int32) ((uint32) horizontal_dy << CEL_BACKGROUND_HORIZONTAL_SHIFT);
  ccb->fields.vertical_dx =
    (int32) ((uint32) vertical_scale *
             (uint32) (quad_[CEL_QUAD_BOTTOM_LEFT].pt_X - quad_[CEL_QUAD_TOP_LEFT].pt_X));
  ccb->fields.vertical_dy =
    (int32) ((uint32) vertical_scale *
             (uint32) (quad_[CEL_QUAD_BOTTOM_LEFT].pt_Y - quad_[CEL_QUAD_TOP_LEFT].pt_Y));
  ccb->fields.horizontal_ddx =
    ((int32) ((uint32) (bottom_horizontal_dx - horizontal_dx) * (uint32) vertical_scale)) >>
    CEL_BACKGROUND_DDX_SHIFT;
  ccb->fields.horizontal_ddy =
    ((int32) ((uint32) (bottom_horizontal_dy - horizontal_dy) * (uint32) vertical_scale)) >>
    CEL_BACKGROUND_DDX_SHIFT;

  finalize_ccb_batch(ccb);
}

RenderCcb *
draw_background_cel_header_to_indexed_quad(const uint32 *cel_header_,
                                           const Point  *points_,
                                           const int32  *indices_)
{
  const Point *bottom_left;
  const Point *bottom_right;
  const Point *top_left;
  const Point *top_right;
  RenderCcb   *ccb;
  int32        bottom_horizontal_dx;
  int32        bottom_horizontal_dy;
  int32        height;
  int32        horizontal_dx;
  int32        horizontal_dy;
  int32        horizontal_scale;
  int32        vertical_scale;
  int32        width;

  top_left     = &points_[indices_[CEL_QUAD_TOP_LEFT]];
  top_right    = &points_[indices_[CEL_QUAD_TOP_RIGHT]];
  bottom_right = &points_[indices_[CEL_QUAD_BOTTOM_RIGHT]];
  bottom_left  = &points_[indices_[CEL_QUAD_BOTTOM_LEFT]];
  ccb          = allocate_cel_header_ccb(cel_header_, top_left->pt_X, top_left->pt_Y);
  ccb->fields.flags |= CCB_BGND;

  height         = _get_cel_header_height(cel_header_);
  vertical_scale = (int32) divide_unsigned_integer(CEL_VERTICAL_SCALE_ONE, (uint32) height);
  width          = _get_cel_header_width(cel_header_);
  if(width == height)
    {
      horizontal_scale = vertical_scale;
    }
  else
    {
      horizontal_scale = (int32) divide_unsigned_integer(CEL_VERTICAL_SCALE_ONE, (uint32) width);
    }

  horizontal_dx = (int32) ((uint32) horizontal_scale * (uint32) (top_right->pt_X - top_left->pt_X));
  horizontal_dy = (int32) ((uint32) horizontal_scale * (uint32) (top_right->pt_Y - top_left->pt_Y));
  bottom_horizontal_dx =
    (int32) ((uint32) horizontal_scale * (uint32) (bottom_right->pt_X - bottom_left->pt_X));
  bottom_horizontal_dy =
    (int32) ((uint32) horizontal_scale * (uint32) (bottom_right->pt_Y - bottom_left->pt_Y));

  ccb->fields.horizontal_dx = (int32) ((uint32) horizontal_dx << CEL_BACKGROUND_HORIZONTAL_SHIFT);
  ccb->fields.horizontal_dy = (int32) ((uint32) horizontal_dy << CEL_BACKGROUND_HORIZONTAL_SHIFT);
  ccb->fields.vertical_dx =
    (int32) ((uint32) vertical_scale * (uint32) (bottom_left->pt_X - top_left->pt_X));
  ccb->fields.vertical_dy =
    (int32) ((uint32) vertical_scale * (uint32) (bottom_left->pt_Y - top_left->pt_Y));
  ccb->fields.horizontal_ddx =
    ((int32) ((uint32) (bottom_horizontal_dx - horizontal_dx) * (uint32) vertical_scale)) >>
    CEL_BACKGROUND_DDX_SHIFT;
  ccb->fields.horizontal_ddy =
    ((int32) ((uint32) (bottom_horizontal_dy - horizontal_dy) * (uint32) vertical_scale)) >>
    CEL_BACKGROUND_DDX_SHIFT;

  return finalize_ccb_batch(ccb);
}

RenderCcb *
draw_cel_header_to_indexed_quad(const uint32 *cel_header_,
                                const Point  *points_,
                                const int32  *indices_)
{
  const Point *bottom_left;
  const Point *bottom_right;
  const Point *top_left;
  const Point *top_right;

  top_left     = &points_[indices_[CEL_QUAD_TOP_LEFT]];
  top_right    = &points_[indices_[CEL_QUAD_TOP_RIGHT]];
  bottom_right = &points_[indices_[CEL_QUAD_BOTTOM_RIGHT]];
  bottom_left  = &points_[indices_[CEL_QUAD_BOTTOM_LEFT]];
  return draw_cel_header_to_quad_coordinates(cel_header_,
                                             top_left->pt_X,
                                             top_left->pt_Y,
                                             top_right->pt_X,
                                             top_right->pt_Y,
                                             bottom_right->pt_X,
                                             bottom_right->pt_Y,
                                             bottom_left->pt_X,
                                             bottom_left->pt_Y);
}
