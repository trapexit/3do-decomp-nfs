#include "menu_layout.h"

#include "track_position.h"
#include "window_line.h"

#include "operamath.h"

#define FIXED16_FRACTIONAL_BITS 16
#define INT32_SIGN_BIT_SHIFT 31
#define MENU_GRID_SIZE_FIXED 0x00FA0000
#define MENU_GRID_LINE_STYLE 0x00005400
#define MENU_VIEWPORT_FIRST_TRACK_SEGMENT 14
#define MENU_VIEWPORT_BOUNDARY_SHIFT 3
#define MENU_VIEWPORT_POSITIVE_LIMIT 0x75300000
#define MENU_VIEWPORT_NEGATIVE_LIMIT (-0x75300000)
#define LOOKUP_ROTATION_COMPONENT_A(angle_) CosF16((frac16) (angle_))
#define LOOKUP_ROTATION_COMPONENT_B(angle_) SinF16((frac16) (angle_))

static int32
_round_fixed16_half(int32 value_)
{
  return (value_ + ((uint32) value_ >> INT32_SIGN_BIT_SHIFT)) >> 1;
}

void
initialize_menu_viewport(MenuViewport *viewport_, int32 first_, int32 second_)
{
  int32 minimum_x;
  int32 minimum_z;
  int32 maximum_x;
  int32 maximum_z;
  int32 segment_index;
  int32 margin_x;
  int32 margin_z;

  viewport_->reserved_00[0] = first_;
  viewport_->reserved_00[1] = second_;
  minimum_x                 = MENU_VIEWPORT_POSITIVE_LIMIT;
  minimum_z                 = MENU_VIEWPORT_POSITIVE_LIMIT;
  maximum_x                 = MENU_VIEWPORT_NEGATIVE_LIMIT;
  maximum_z                 = MENU_VIEWPORT_NEGATIVE_LIMIT;
  for(segment_index = MENU_VIEWPORT_FIRST_TRACK_SEGMENT; segment_index <= first_; segment_index++)
    {
      const Vector3 *position;

      position = &g_TRACK_SEGMENTS[segment_index].surface_reference;
      if(position->x < minimum_x)
        minimum_x = position->x;
      if(position->z < minimum_z)
        minimum_z = position->z;
      if(position->x > maximum_x)
        maximum_x = position->x;
      if(position->z > maximum_z)
        maximum_z = position->z;
    }

  margin_x               = (maximum_x - minimum_x) >> MENU_VIEWPORT_BOUNDARY_SHIFT;
  margin_z               = (maximum_z - minimum_z) >> MENU_VIEWPORT_BOUNDARY_SHIFT;
  viewport_->base_left   = minimum_x - margin_x;
  viewport_->base_top    = minimum_z - margin_z;
  viewport_->base_right  = maximum_x + margin_x;
  viewport_->base_bottom = maximum_z + margin_z;
}

void
update_menu_rectangle_metrics(MenuRectangle *rectangle_)
{
  rectangle_->width  = rectangle_->right - rectangle_->left;
  rectangle_->height = rectangle_->bottom - rectangle_->top;
  rectangle_->area   = DivSF16(rectangle_->height << FIXED16_FRACTIONAL_BITS,
                               rectangle_->width << FIXED16_FRACTIONAL_BITS);
}

void
update_menu_rectangle_transform(MenuRectangle *rectangle_, const MenuScale *scale_)
{
  rectangle_->scaled_width = DivSF16(rectangle_->width << FIXED16_FRACTIONAL_BITS, scale_->width);
  rectangle_->scaled_height =
    DivSF16(rectangle_->height << FIXED16_FRACTIONAL_BITS, scale_->height);
  rectangle_->x_offset = -MulSF16(scale_->left, rectangle_->scaled_width);
  rectangle_->y_offset = -MulSF16(scale_->top, rectangle_->scaled_height);
  rectangle_->x_offset >>= FIXED16_FRACTIONAL_BITS;
  rectangle_->y_offset >>= FIXED16_FRACTIONAL_BITS;
}

void
update_menu_scale_coordinates(const MenuViewport *viewport_, MenuScale *scale_)
{
  scale_->left = viewport_->base_left + MulSF16(scale_->interpolation_factor,
                                                viewport_->bounded_left - viewport_->base_left);
  scale_->top = viewport_->base_top +
                MulSF16(scale_->interpolation_factor, viewport_->bounded_top - viewport_->base_top);
  scale_->right = viewport_->base_right + MulSF16(scale_->interpolation_factor,
                                                  viewport_->bounded_right - viewport_->base_right);
  scale_->bottom =
    viewport_->base_bottom +
    MulSF16(scale_->interpolation_factor, viewport_->bounded_bottom - viewport_->base_bottom);
}

void
fit_menu_scale_to_rectangle(const MenuRectangle *rectangle_, MenuScale *scale_)
{
  int32 adjusted_size;
  int32 half_size;

  scale_->width    = scale_->right - scale_->left;
  scale_->height   = scale_->bottom - scale_->top;
  scale_->area     = DivSF16(scale_->height, scale_->width);
  scale_->center_x = scale_->left + _round_fixed16_half(scale_->width);
  scale_->center_y = scale_->top + _round_fixed16_half(scale_->height);

  if(scale_->area > rectangle_->area)
    {
      adjusted_size    = DivSF16(scale_->height, rectangle_->area);
      half_size        = _round_fixed16_half(adjusted_size);
      scale_->left     = scale_->center_x - half_size;
      scale_->right    = scale_->center_x + half_size;
      scale_->center_x = scale_->left + _round_fixed16_half(adjusted_size);
      scale_->width    = adjusted_size;
      scale_->area     = DivSF16(scale_->height, adjusted_size);
    }
  else
    {
      adjusted_size    = MulSF16(scale_->width, rectangle_->area);
      half_size        = _round_fixed16_half(adjusted_size);
      scale_->top      = scale_->center_y - half_size;
      scale_->bottom   = scale_->center_y + half_size;
      scale_->center_y = scale_->top + _round_fixed16_half(adjusted_size);
      scale_->height   = adjusted_size;
      scale_->area     = DivSF16(adjusted_size, scale_->width);
    }
}

void
draw_menu_rectangle_grid(void *context_, const MenuRectangle *rectangle_, const MenuScale *scale_)
{
  int32 horizontal_step;
  int32 vertical_step;
  int32 coordinate;

  (void) context_;
  (void) scale_;

  horizontal_step =
    DivSF16(MENU_GRID_SIZE_FIXED, rectangle_->scaled_width) >> FIXED16_FRACTIONAL_BITS;
  vertical_step =
    DivSF16(MENU_GRID_SIZE_FIXED, rectangle_->scaled_height) >> FIXED16_FRACTIONAL_BITS;
  if(horizontal_step < rectangle_->width)
    {
      for(coordinate = rectangle_->left; coordinate <= rectangle_->right;
          coordinate += horizontal_step)
        draw_window_line(coordinate,
                         rectangle_->top,
                         coordinate,
                         rectangle_->bottom,
                         MENU_GRID_LINE_STYLE);
    }

  if(vertical_step < rectangle_->height)
    {
      for(coordinate = rectangle_->top; coordinate <= rectangle_->bottom;
          coordinate += vertical_step)
        draw_window_line(rectangle_->left,
                         coordinate,
                         rectangle_->right,
                         coordinate,
                         MENU_GRID_LINE_STYLE);
    }
}

void
calculate_rotated_menu_coordinates(int32 *horizontal_,
                                   int32 *vertical_,
                                   int32  angle_,
                                   int32  horizontal_scale_,
                                   int32  vertical_scale_)
{
  int32 component_a;
  int32 component_b;
  int32 horizontal_scale_fixed;
  int32 vertical_scale_fixed;

  component_a            = LOOKUP_ROTATION_COMPONENT_A(angle_);
  component_b            = LOOKUP_ROTATION_COMPONENT_B(angle_);
  horizontal_scale_fixed = horizontal_scale_ << FIXED16_FRACTIONAL_BITS;
  vertical_scale_fixed   = vertical_scale_ << FIXED16_FRACTIONAL_BITS;

  *horizontal_ =
    MulSF16(component_a, horizontal_scale_fixed) - MulSF16(component_b, vertical_scale_fixed);
  *vertical_ =
    MulSF16(component_b, horizontal_scale_fixed) + MulSF16(component_a, vertical_scale_fixed);
  *horizontal_ >>= FIXED16_FRACTIONAL_BITS;
  *vertical_ >>= FIXED16_FRACTIONAL_BITS;
}
