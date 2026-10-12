#include "quad_render.h"

#include "ccb_manager.h"
#include "diagnostics.h"
#include "display_buffer_state.h"
#include "fixed_math.h"
#include "integer_angle.h"
#include "trig_lookup.h"

#include "hardware.h"

#include "stdarg.h"

#define QUAD_WINDOW_MODE_WORD 9
#define QUAD_COLOR_SHIFT 16
#define QUAD_POSITION_SCALE 0x10000
#define QUAD_HORIZONTAL_DELTA_SCALE 0x100000
#define QUAD_VERTICAL_DELTA_SCALE 0x10000
#define POLYGON_FAN_STEP 2
#define LINE_QUAD_POINT_COUNT 4
#define LINE_ASPECT_QUARTER_SHIFT 2
#define LINE_ASPECT_SIXTEENTH_SHIFT 4
#define LINE_SPAN_HALF_SHIFT 1
#define LINE_END_NEGATIVE_INDEX 2
#define LINE_START_NEGATIVE_INDEX 3

enum
{
  SOLID_QUAD_TOP_LEFT_INDEX     = 0,
  SOLID_QUAD_TOP_RIGHT_INDEX    = 1,
  SOLID_QUAD_BOTTOM_RIGHT_INDEX = 2,
  SOLID_QUAD_BOTTOM_LEFT_INDEX  = 3
};

void
draw_solid_quad(uint32 color_,
                int32  x0_,
                int32  y0_,
                int32  x1_,
                int32  y1_,
                int32  x2_,
                int32  y2_,
                int32  x3_,
                int32  y3_)
{
  RenderCcb *ccb;
  int32      position_offset;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[QUAD_WINDOW_MODE_WORD] == 0)
    fatal_error("drawquad - 8 BIT WINDOWS NOT SUPPORTED\n");

  ccb               = allocate_untransformed_ccb();
  ccb->fields.flags = ((ccb->fields.flags & ~CCB_SPABS) | CCB_BGND | CCB_NOBLK);
  ccb->fields.preamble_0 |= PRE0_BGND;
  ccb->width      = (int32) (color_ << QUAD_COLOR_SHIFT);
  position_offset = g_CCB_TEMPLATE.x_position;
  ccb->fields.x_position     = ((position_offset + x0_) * QUAD_POSITION_SCALE);
  ccb->fields.y_position     = ((position_offset + y0_) * QUAD_POSITION_SCALE);
  ccb->fields.horizontal_dx  = ((x1_ - x0_) * QUAD_HORIZONTAL_DELTA_SCALE);
  ccb->fields.horizontal_dy  = ((y1_ - y0_) * QUAD_HORIZONTAL_DELTA_SCALE);
  ccb->fields.vertical_dx    = ((x3_ - x0_) * QUAD_VERTICAL_DELTA_SCALE);
  ccb->fields.vertical_dy    = ((y3_ - y0_) * QUAD_VERTICAL_DELTA_SCALE);
  ccb->fields.horizontal_ddx = (((x2_ - x3_) - (x1_ - x0_)) * QUAD_HORIZONTAL_DELTA_SCALE);
  ccb->fields.horizontal_ddy = (((y2_ - y3_) - (y1_ - y0_)) * QUAD_HORIZONTAL_DELTA_SCALE);
  finalize_ccb_batch(ccb);
}

RenderCcb *
draw_solid_quad_points(uint32 color_, const Point2 *points_)
{
  RenderCcb *ccb;
  int32      position_offset;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[QUAD_WINDOW_MODE_WORD] == 0)
    fatal_error("drawquad - 8 BIT WINDOWS NOT SUPPORTED\n");

  ccb               = allocate_untransformed_ccb();
  ccb->fields.flags = ((ccb->fields.flags & ~CCB_SPABS) | CCB_BGND);
  ccb->fields.preamble_0 |= PRE0_BGND;
  ccb->width      = (int32) (color_ << QUAD_COLOR_SHIFT);
  position_offset = g_CCB_TEMPLATE.x_position;
  ccb->fields.x_position =
    ((position_offset + points_[SOLID_QUAD_TOP_LEFT_INDEX].x) * QUAD_POSITION_SCALE);
  ccb->fields.y_position =
    ((position_offset + points_[SOLID_QUAD_TOP_LEFT_INDEX].y) * QUAD_POSITION_SCALE);
  ccb->fields.horizontal_dx =
    ((points_[SOLID_QUAD_TOP_RIGHT_INDEX].x - points_[SOLID_QUAD_TOP_LEFT_INDEX].x) *
     QUAD_HORIZONTAL_DELTA_SCALE);
  ccb->fields.horizontal_dy =
    ((points_[SOLID_QUAD_TOP_RIGHT_INDEX].y - points_[SOLID_QUAD_TOP_LEFT_INDEX].y) *
     QUAD_HORIZONTAL_DELTA_SCALE);
  ccb->fields.vertical_dx =
    ((points_[SOLID_QUAD_BOTTOM_LEFT_INDEX].x - points_[SOLID_QUAD_TOP_LEFT_INDEX].x) *
     QUAD_VERTICAL_DELTA_SCALE);
  ccb->fields.vertical_dy =
    ((points_[SOLID_QUAD_BOTTOM_LEFT_INDEX].y - points_[SOLID_QUAD_TOP_LEFT_INDEX].y) *
     QUAD_VERTICAL_DELTA_SCALE);
  ccb->fields.horizontal_ddx =
    (((points_[SOLID_QUAD_BOTTOM_RIGHT_INDEX].x - points_[SOLID_QUAD_BOTTOM_LEFT_INDEX].x) -
      (points_[SOLID_QUAD_TOP_RIGHT_INDEX].x - points_[SOLID_QUAD_TOP_LEFT_INDEX].x)) *
     QUAD_HORIZONTAL_DELTA_SCALE);
  ccb->fields.horizontal_ddy =
    (((points_[SOLID_QUAD_BOTTOM_RIGHT_INDEX].y - points_[SOLID_QUAD_BOTTOM_LEFT_INDEX].y) -
      (points_[SOLID_QUAD_TOP_RIGHT_INDEX].y - points_[SOLID_QUAD_TOP_LEFT_INDEX].y)) *
     QUAD_HORIZONTAL_DELTA_SCALE);
  return finalize_ccb_batch(ccb);
}

RenderCcb *
draw_indexed_solid_quad(uint32 color_, const Point2 *points_, const int32 *indices_)
{
  const Point2 *top_left;
  const Point2 *top_right;
  const Point2 *bottom_right;
  const Point2 *bottom_left;
  RenderCcb    *ccb;
  int32         position_offset;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[QUAD_WINDOW_MODE_WORD] == 0)
    fatal_error("drawquad - 8 BIT WINDOWS NOT SUPPORTED\n");

  top_left          = &points_[indices_[SOLID_QUAD_TOP_LEFT_INDEX]];
  top_right         = &points_[indices_[SOLID_QUAD_TOP_RIGHT_INDEX]];
  bottom_right      = &points_[indices_[SOLID_QUAD_BOTTOM_RIGHT_INDEX]];
  bottom_left       = &points_[indices_[SOLID_QUAD_BOTTOM_LEFT_INDEX]];
  ccb               = allocate_untransformed_ccb();
  ccb->fields.flags = ((ccb->fields.flags & ~CCB_SPABS) | CCB_BGND);
  ccb->fields.preamble_0 |= PRE0_BGND;
  ccb->width      = (int32) (color_ << QUAD_COLOR_SHIFT);
  position_offset = g_CCB_TEMPLATE.x_position;
  ccb->fields.x_position    = ((position_offset + top_left->x) * QUAD_POSITION_SCALE);
  ccb->fields.y_position    = ((position_offset + top_left->y) * QUAD_POSITION_SCALE);
  ccb->fields.horizontal_dx = ((top_right->x - top_left->x) * QUAD_HORIZONTAL_DELTA_SCALE);
  ccb->fields.horizontal_dy = ((top_right->y - top_left->y) * QUAD_HORIZONTAL_DELTA_SCALE);
  ccb->fields.vertical_dx   = ((bottom_left->x - top_left->x) * QUAD_VERTICAL_DELTA_SCALE);
  ccb->fields.vertical_dy   = ((bottom_left->y - top_left->y) * QUAD_VERTICAL_DELTA_SCALE);
  ccb->fields.horizontal_ddx =
    (((bottom_right->x - bottom_left->x) - (top_right->x - top_left->x)) *
     QUAD_HORIZONTAL_DELTA_SCALE);
  ccb->fields.horizontal_ddy =
    (((bottom_right->y - bottom_left->y) - (top_right->y - top_left->y)) *
     QUAD_HORIZONTAL_DELTA_SCALE);

  return finalize_ccb_batch(ccb);
}

RenderCcb *
draw_indexed_transparent_quad(uint32        color_,
                              const Point2 *points_,
                              int32         top_left_index_,
                              int32         top_right_index_,
                              int32         bottom_right_index_,
                              int32         bottom_left_index_)
{
  const Point2 *top_left;
  const Point2 *top_right;
  const Point2 *bottom_right;
  const Point2 *bottom_left;
  RenderCcb    *ccb;
  int32         position_offset;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[QUAD_WINDOW_MODE_WORD] == 0)
    fatal_error("drawquad - 8 BIT WINDOWS NOT SUPPORTED\n");

  top_left     = &points_[top_left_index_];
  top_right    = &points_[top_right_index_];
  bottom_right = &points_[bottom_right_index_];
  bottom_left  = &points_[bottom_left_index_];
  ccb          = allocate_untransformed_ccb();
  ccb->fields.flags &= ~CCB_SPABS;
  ccb->fields.preamble_0 |= PRE0_BGND;
  ccb->width      = (int32) (color_ << QUAD_COLOR_SHIFT);
  position_offset = g_CCB_TEMPLATE.x_position;
  ccb->fields.x_position    = ((position_offset + top_left->x) * QUAD_POSITION_SCALE);
  ccb->fields.y_position    = ((position_offset + top_left->y) * QUAD_POSITION_SCALE);
  ccb->fields.horizontal_dx = ((top_right->x - top_left->x) * QUAD_HORIZONTAL_DELTA_SCALE);
  ccb->fields.horizontal_dy = ((top_right->y - top_left->y) * QUAD_HORIZONTAL_DELTA_SCALE);
  ccb->fields.vertical_dx   = ((bottom_left->x - top_left->x) * QUAD_VERTICAL_DELTA_SCALE);
  ccb->fields.vertical_dy   = ((bottom_left->y - top_left->y) * QUAD_VERTICAL_DELTA_SCALE);
  ccb->fields.horizontal_ddx =
    (((bottom_right->x - bottom_left->x) - (top_right->x - top_left->x)) *
     QUAD_HORIZONTAL_DELTA_SCALE);
  ccb->fields.horizontal_ddy =
    (((bottom_right->y - bottom_left->y) - (top_right->y - top_left->y)) *
     QUAD_HORIZONTAL_DELTA_SCALE);

  return finalize_ccb_batch(ccb);
}

void
draw_solid_polygon(uint32 color_, int32 point_count_, const Point2 *points_)
{
  int32 point_index;

  point_index = 1;
  while((point_index + POLYGON_FAN_STEP) < point_count_)
    {
      draw_solid_quad(color_,
                      points_[0].x,
                      points_[0].y,
                      points_[point_index].x,
                      points_[point_index].y,
                      points_[point_index + 1].x,
                      points_[point_index + 1].y,
                      points_[point_index + POLYGON_FAN_STEP].x,
                      points_[point_index + POLYGON_FAN_STEP].y);
      point_index += POLYGON_FAN_STEP;
    }

  if((point_index + POLYGON_FAN_STEP) == point_count_)
    {
      draw_solid_quad(color_,
                      points_[0].x,
                      points_[0].y,
                      points_[0].x,
                      points_[0].y,
                      points_[point_index].x,
                      points_[point_index].y,
                      points_[point_index + 1].x,
                      points_[point_index + 1].y);
    }
}

void
draw_solid_polygon_coordinates(uint32 color_, int32 point_count_, ...)
{
  va_list points;

  va_start(points, point_count_);
  draw_solid_polygon(color_, point_count_, (const Point2 *) points);
  va_end(points);
}

void
draw_solid_polygon_coordinates_legacy(uint32 color_, int32 point_count_, ...)
{
  va_list points;

  va_start(points, point_count_);
  draw_solid_polygon(color_, point_count_, (const Point2 *) points);
  va_end(points);
}

void
draw_solid_line_segment(int32  start_x_,
                        int32  start_y_,
                        int32  end_x_,
                        int32  end_y_,
                        uint32 color_,
                        int32  thickness_)
{
  int32  angle;
  int32  horizontal_half;
  int32  horizontal_remainder;
  int32  horizontal_span;
  Point2 points[LINE_QUAD_POINT_COUNT];
  int32  vertical_half;
  int32  vertical_remainder;
  int32  vertical_span;

  angle           = calculate_integer_angle(end_x_ - start_x_,
                                            (end_y_ - start_y_) +
                                              ((end_y_ - start_y_) >> LINE_ASPECT_QUARTER_SHIFT));
  horizontal_span = multiply_fixed16(thickness_, -cosine_1024(angle));
  vertical_span   = multiply_fixed16(thickness_, sine_1024(angle));
  vertical_span   = vertical_span - (vertical_span >> LINE_ASPECT_QUARTER_SHIFT) +
                    (vertical_span >> LINE_ASPECT_SIXTEENTH_SHIFT);

  horizontal_half      = horizontal_span >> LINE_SPAN_HALF_SHIFT;
  horizontal_remainder = horizontal_span - horizontal_half;
  vertical_half        = vertical_span >> LINE_SPAN_HALF_SHIFT;
  vertical_remainder   = vertical_span - vertical_half;

  points[0].x                         = start_x_ + horizontal_half;
  points[0].y                         = start_y_ + vertical_half;
  points[1].x                         = end_x_ + horizontal_half;
  points[1].y                         = end_y_ + vertical_half;
  points[LINE_END_NEGATIVE_INDEX].x   = end_x_ - horizontal_remainder;
  points[LINE_END_NEGATIVE_INDEX].y   = end_y_ - vertical_remainder;
  points[LINE_START_NEGATIVE_INDEX].x = start_x_ - horizontal_remainder;
  points[LINE_START_NEGATIVE_INDEX].y = start_y_ - vertical_remainder;

  draw_solid_polygon(color_, LINE_QUAD_POINT_COUNT, points);
}
