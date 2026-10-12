#pragma once

#include "types.h"

typedef struct MenuRectangle
{
  int32 left;
  int32 top;
  int32 right;
  int32 bottom;
  int32 width;
  int32 height;
  int32 area;
  int32 scaled_width;
  int32 scaled_height;
  int32 x_offset;
  int32 y_offset;
} MenuRectangle;

typedef struct MenuScale
{
  int32 left;
  int32 top;
  int32 right;
  int32 bottom;
  int32 width;
  int32 height;
  int32 area;
  int32 center_x;
  int32 center_y;
  int32 interpolation_factor;
} MenuScale;

typedef struct MenuViewport
{
  int32 reserved_00[2];
  int32 base_left;
  int32 base_top;
  int32 base_right;
  int32 base_bottom;
  int32 bounded_left;
  int32 bounded_top;
  int32 bounded_right;
  int32 bounded_bottom;
} MenuViewport;

void
update_menu_rectangle_metrics(MenuRectangle *rectangle_);
void
update_menu_rectangle_transform(MenuRectangle *rectangle_, const MenuScale *scale_);
void
update_menu_scale_coordinates(const MenuViewport *viewport_, MenuScale *scale_);
void
fit_menu_scale_to_rectangle(const MenuRectangle *rectangle_, MenuScale *scale_);
void
initialize_menu_viewport(MenuViewport *viewport_, int32 first_, int32 second_);
void
draw_menu_rectangle_grid(void *context_, const MenuRectangle *rectangle_, const MenuScale *scale_);
void
calculate_rotated_menu_coordinates(int32 *horizontal_,
                                   int32 *vertical_,
                                   int32  angle_,
                                   int32  horizontal_scale_,
                                   int32  vertical_scale_);
