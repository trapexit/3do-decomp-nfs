#pragma once

#include "geometry.h"

typedef struct RotatedTransform2D
{
  uint8 reserved_00[0x10];
  int32 transformed_x;
  int32 transformed_y;
  int32 component_a_scaled;
  int32 component_b_scaled;
  int32 negative_component_b;
  int32 component_a;
} RotatedTransform2D;

void
configure_rotated_2d_transform(const Point2       *point_,
                               int32               angle_,
                               int32               x_offset_,
                               int32               y_offset_,
                               RotatedTransform2D *transform_);
