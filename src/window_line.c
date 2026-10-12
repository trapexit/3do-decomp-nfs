#include "window_line.h"

#include "display_buffer_state.h"
#include "quad_render.h"
#include "window_pixel.h"

#define WINDOW_LINE_MODE_WORD 9

int32
clip_window_line(int32 *start_x_, int32 *start_y_, int32 *end_x_, int32 *end_y_)
{
  int32 bottom;
  int32 end_x;
  int32 end_y;
  int32 left;
  int32 right;
  int32 start_x;
  int32 start_y;
  int32 temporary;
  int32 top;

  start_x = *start_x_;
  start_y = *start_y_;
  end_x   = *end_x_;
  end_y   = *end_y_;
  left    = (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_X_WORD];
  right = (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_WORD];
  top   = (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_Y_WORD];
  bottom =
    (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_HEIGHT_WORD];

  if((start_x >= left) && (start_x < right) && (start_y >= top) && (start_y < bottom) &&
     (end_x >= left) && (end_x < right) && (end_y >= top) && (end_y < bottom))
    {
      return 1;
    }

  if(((start_x >= right) && (end_x >= right)) || ((start_x < left) && (end_x < left)) ||
     ((start_y >= bottom) && (end_y >= bottom)) || ((start_y < top) && (end_y < top)))
    {
      return 0;
    }

  if(end_x != start_x)
    {
      if(end_x < start_x)
        {
          temporary = start_x;
          start_x   = end_x;
          end_x     = temporary;
          temporary = start_y;
          start_y   = end_y;
          end_y     = temporary;
        }

      if(start_x < left)
        {
          start_y += ((end_y - start_y) * (left - start_x)) / (end_x - start_x);
          start_x = left;
        }
      if(end_x >= right)
        {
          end_y = start_y + ((end_y - start_y) * (right - start_x)) / (end_x - start_x);
          end_x = right - 1;
        }
    }

  if(end_y != start_y)
    {
      if(end_y < start_y)
        {
          temporary = start_x;
          start_x   = end_x;
          end_x     = temporary;
          temporary = start_y;
          start_y   = end_y;
          end_y     = temporary;
        }

      if(start_y < top)
        {
          start_x += ((end_x - start_x) * (top - start_y)) / (end_y - start_y);
          start_y = top;
        }
      if(end_y >= bottom)
        {
          end_x = start_x + ((end_x - start_x) * (bottom - start_y)) / (end_y - start_y);
          end_y = bottom - 1;
        }
    }

  *start_x_ = start_x;
  *start_y_ = start_y;
  *end_x_   = end_x;
  *end_y_   = end_y;

  if((start_x < left) || (start_x >= right) || (start_y < top) || (start_y >= bottom) ||
     (end_x < left) || (end_x >= right) || (end_y < top) || (end_y >= bottom))
    {
      return 0;
    }
  return 1;
}

void
draw_window_line(int32 start_x_, int32 start_y_, int32 end_x_, int32 end_y_, uint32 value_)
{
  int32 delta_x;
  int32 delta_y;
  int32 position;
  int32 temporary;

  if(!clip_window_line(&start_x_, &start_y_, &end_x_, &end_y_))
    return;

  delta_x = end_x_ - start_x_;
  if(delta_x < 0)
    delta_x = -delta_x;
  delta_y = end_y_ - start_y_;
  if(delta_y < 0)
    delta_y = -delta_y;

  if(g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[WINDOW_LINE_MODE_WORD] != 0)
    {
      if(delta_x >= delta_y)
        {
          if(end_x_ < start_x_)
            {
              temporary = start_x_;
              start_x_  = end_x_;
              end_x_    = temporary;
              temporary = start_y_;
              start_y_  = end_y_;
              end_y_    = temporary;
            }
          draw_solid_quad(value_,
                          start_x_,
                          start_y_,
                          end_x_,
                          end_y_,
                          end_x_,
                          end_y_ + 1,
                          start_x_,
                          start_y_ + 1);
        }
      else
        {
          if(end_y_ < start_y_)
            {
              temporary = start_x_;
              start_x_  = end_x_;
              end_x_    = temporary;
              temporary = start_y_;
              start_y_  = end_y_;
              end_y_    = temporary;
            }
          draw_solid_quad(value_,
                          start_x_,
                          start_y_,
                          start_x_ + 1,
                          start_y_,
                          end_x_ + 1,
                          end_y_,
                          end_x_,
                          end_y_);
        }
      return;
    }

  if(delta_y > delta_x)
    {
      if(start_y_ > end_y_)
        {
          temporary = start_x_;
          start_x_  = end_x_;
          end_x_    = temporary;
          temporary = start_y_;
          start_y_  = end_y_;
          end_y_    = temporary;
        }
      if(start_y_ == end_y_)
        return;

      for(position = start_y_; position <= end_y_; position++)
        {
          temporary =
            start_x_ + ((position - start_y_) * (end_x_ - start_x_)) / (end_y_ - start_y_);
          write_window_pixel(temporary, position, value_);
        }
    }
  else
    {
      if(start_x_ > end_x_)
        {
          temporary = start_x_;
          start_x_  = end_x_;
          end_x_    = temporary;
          temporary = start_y_;
          start_y_  = end_y_;
          end_y_    = temporary;
        }
      if(start_x_ == end_x_)
        return;

      for(position = start_x_; position <= end_x_; position++)
        {
          temporary =
            start_y_ + ((position - start_x_) * (end_y_ - start_y_)) / (end_x_ - start_x_);
          write_window_pixel(position, temporary, value_);
        }
    }
}
