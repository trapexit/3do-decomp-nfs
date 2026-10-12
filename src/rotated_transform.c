#include "rotated_transform.h"

#include "operamath.h"

#define ROTATED_TRANSFORM_FRACTION_BITS 8
#define ROTATED_TRANSFORM_SCALE_SHIFT 4
#define LOOKUP_ROTATION_COMPONENT_A(angle_) CosF16((frac16) (angle_))
#define LOOKUP_ROTATION_COMPONENT_B(angle_) SinF16((frac16) (angle_))

void
configure_rotated_2d_transform(const Point2       *point_,
                               int32               angle_,
                               int32               x_offset_,
                               int32               y_offset_,
                               RotatedTransform2D *transform_)
{
  int32 component_a;
  int32 component_b;

  component_b                      = LOOKUP_ROTATION_COMPONENT_B(angle_);
  component_a                      = LOOKUP_ROTATION_COMPONENT_A(angle_);
  transform_->transformed_x        = ((component_a >> ROTATED_TRANSFORM_FRACTION_BITS) *
                                      (point_->x >> ROTATED_TRANSFORM_FRACTION_BITS)) +
                                     x_offset_ -
                                     ((component_b >> ROTATED_TRANSFORM_FRACTION_BITS) *
                                      (point_->y >> ROTATED_TRANSFORM_FRACTION_BITS));
  transform_->transformed_y        = ((component_b >> ROTATED_TRANSFORM_FRACTION_BITS) *
                                      (point_->x >> ROTATED_TRANSFORM_FRACTION_BITS)) +
                                     y_offset_ +
                                     ((component_a >> ROTATED_TRANSFORM_FRACTION_BITS) *
                                      (point_->y >> ROTATED_TRANSFORM_FRACTION_BITS));
  transform_->component_a_scaled   = component_a << ROTATED_TRANSFORM_SCALE_SHIFT;
  transform_->component_b_scaled   = component_b << ROTATED_TRANSFORM_SCALE_SHIFT;
  transform_->negative_component_b = -component_b;
  transform_->component_a          = component_a;
}
