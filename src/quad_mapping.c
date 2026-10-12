#include "quad_mapping.h"

#define CCB_HORIZONTAL_FRACTION_EXTRA_BITS 4

enum
{
  QUAD_TOP_LEFT_INDEX     = 0,
  QUAD_TOP_RIGHT_INDEX    = 1,
  QUAD_BOTTOM_RIGHT_INDEX = 2,
  QUAD_BOTTOM_LEFT_INDEX  = 3
};

void
map_fixed16_quad_to_ccb(CcbTemplate  *ccb_,
                        const Point2 *quad_,
                        int32         width_shift_,
                        int32         height_shift_)
{
  int32  horizontal_ddx;
  int32  horizontal_ddy;
  int32  horizontal_dx;
  int32  horizontal_dy;
  int32  horizontal_shift;
  int32  right_horizontal_dx;
  int32  right_horizontal_dy;
  int32  vertical_dx;
  int32  vertical_dy;
  Point2 bottom_left;
  Point2 bottom_right;
  Point2 top_left;
  Point2 top_right;

  top_left.x     = quad_[QUAD_TOP_LEFT_INDEX].x;
  top_left.y     = quad_[QUAD_TOP_LEFT_INDEX].y;
  top_right.x    = quad_[QUAD_TOP_RIGHT_INDEX].x;
  top_right.y    = quad_[QUAD_TOP_RIGHT_INDEX].y;
  bottom_right.x = quad_[QUAD_BOTTOM_RIGHT_INDEX].x;
  bottom_right.y = quad_[QUAD_BOTTOM_RIGHT_INDEX].y;
  bottom_left.x  = quad_[QUAD_BOTTOM_LEFT_INDEX].x;
  bottom_left.y  = quad_[QUAD_BOTTOM_LEFT_INDEX].y;

  horizontal_shift    = width_shift_ - CCB_HORIZONTAL_FRACTION_EXTRA_BITS;
  horizontal_dx       = (int32) ((uint32) top_right.x - (uint32) top_left.x);
  horizontal_dy       = (int32) ((uint32) top_right.y - (uint32) top_left.y);
  right_horizontal_dx = (int32) ((uint32) bottom_right.x - (uint32) bottom_left.x);
  right_horizontal_dy = (int32) ((uint32) bottom_right.y - (uint32) bottom_left.y);

  if(horizontal_shift >= 0)
    {
      horizontal_dx >>= horizontal_shift;
      horizontal_dy >>= horizontal_shift;
      right_horizontal_dx >>= horizontal_shift;
      right_horizontal_dy >>= horizontal_shift;
    }
  else
    {
      horizontal_shift    = (int32) (0 - (uint32) horizontal_shift);
      horizontal_dx       = (int32) ((uint32) horizontal_dx << horizontal_shift);
      horizontal_dy       = (int32) ((uint32) horizontal_dy << horizontal_shift);
      right_horizontal_dx = (int32) ((uint32) right_horizontal_dx << horizontal_shift);
      right_horizontal_dy = (int32) ((uint32) right_horizontal_dy << horizontal_shift);
    }

  vertical_dx = (int32) ((uint32) bottom_left.x - (uint32) top_left.x);
  vertical_dy = (int32) ((uint32) bottom_left.y - (uint32) top_left.y);
  vertical_dx >>= height_shift_;
  vertical_dy >>= height_shift_;
  horizontal_ddx = (int32) ((uint32) right_horizontal_dx - (uint32) horizontal_dx);
  horizontal_ddy = (int32) ((uint32) right_horizontal_dy - (uint32) horizontal_dy);
  horizontal_ddx >>= height_shift_;
  horizontal_ddy >>= height_shift_;

  ccb_->x_position     = top_left.x;
  ccb_->y_position     = top_left.y;
  ccb_->horizontal_dx  = horizontal_dx;
  ccb_->horizontal_dy  = horizontal_dy;
  ccb_->vertical_dx    = vertical_dx;
  ccb_->vertical_dy    = vertical_dy;
  ccb_->horizontal_ddx = horizontal_ddx;
  ccb_->horizontal_ddy = horizontal_ddy;
}
