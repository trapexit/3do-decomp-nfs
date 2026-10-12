#include "rectangle_outline.h"

#include "window_fill.h"

void
draw_rectangle_outline(int32  x_first_,
                       int32  y_first_,
                       int32  x_second_,
                       int32  y_second_,
                       uint32 color_)
{
  int32 bottom;
  int32 left;
  int32 right;
  int32 side_height;
  int32 top;
  int32 width;

  left  = x_first_;
  right = x_second_;
  if(right < left)
    {
      left  = x_second_;
      right = x_first_;
    }
  width = (right - left) + 1;

  top    = y_first_;
  bottom = y_second_;
  if(bottom < top)
    {
      top    = y_second_;
      bottom = y_first_;
    }
  side_height = (bottom - top) - 1;

  fill_clipped_window_rectangle(left, top, width, 1, color_);
  fill_clipped_window_rectangle(right, top + 1, 1, side_height, color_);
  fill_clipped_window_rectangle(left, top + 1, 1, side_height, color_);
  fill_clipped_window_rectangle(left, bottom, width, 1, color_);
}
