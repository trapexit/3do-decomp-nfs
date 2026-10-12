#include "matrix_collision.h"
#include "player_vehicle_state.h"

#include "operamath.h"

#define MATRIX_SURFACE_FIXED8_SHIFT 8
#define MATRIX_SURFACE_POSITION_CORRECTION_SHIFT 1
#define MATRIX_SURFACE_GRAVITY_FIXED 0x9CF5C
#define MATRIX_SURFACE_COMPONENT_COUNT 3
#define MATRIX_SURFACE_MATRIX_ROW_STRIDE 3
#define MATRIX_SURFACE_HALF_EXTENT_WORD_OFFSET 29
#define MATRIX_SURFACE_VECTOR_Z 2
#define MATRIX_SURFACE_DOUBLE_SCALE 2
#define MATRIX_SURFACE_HALF_SHIFT 1
#define MATRIX_SURFACE_FIXED_ONE 0x10000

void
resolve_matrix_controller_surface_collision(MatrixControllerState *controller_,
                                            const Vector3         *normal_,
                                            const Vector3         *surface_point_)
{
  const int32 *matrix;
  const int32 *half_extents;
  Vector3      contact_point;
  Vector3      contact_offset;
  Vector3      contact_velocity;
  Vector3      correction;
  int32        projected_extent[MATRIX_SURFACE_COMPONENT_COUNT];
  int32        projection_sign[MATRIX_SURFACE_COMPONENT_COUNT];
  int32        separation;
  int32        correction_magnitude;
  int32        vertical_speed_squared;
  int32        vertical_speed_limit;
  int32        residual_speed;
  int32        index;

  matrix       = (const int32 *) &controller_->matrix;
  half_extents = ((const int32 *) controller_) + MATRIX_SURFACE_HALF_EXTENT_WORD_OFFSET;
  g_VEHICLE_COLLISION_RESPONSE_MAGNITUDE = 0;

  for(index = 0; index < MATRIX_SURFACE_COMPONENT_COUNT; index++)
    {
      projected_extent[index] =
        (half_extents[index] >> MATRIX_SURFACE_FIXED8_SHIFT) *
        (((matrix[index * MATRIX_SURFACE_MATRIX_ROW_STRIDE] >> MATRIX_SURFACE_FIXED8_SHIFT) *
            (normal_->x >> MATRIX_SURFACE_FIXED8_SHIFT) +
          (matrix[index * MATRIX_SURFACE_MATRIX_ROW_STRIDE + 1] >> MATRIX_SURFACE_FIXED8_SHIFT) *
            (normal_->y >> MATRIX_SURFACE_FIXED8_SHIFT) +
          (matrix[index * MATRIX_SURFACE_MATRIX_ROW_STRIDE + 2] >> MATRIX_SURFACE_FIXED8_SHIFT) *
            (normal_->z >> MATRIX_SURFACE_FIXED8_SHIFT)) >>
         MATRIX_SURFACE_FIXED8_SHIFT);
      if(projected_extent[index] < 0)
        projection_sign[index] = 1;
      else
        projection_sign[index] = -1;
    }

  contact_offset.x = controller_->position.x - surface_point_->x;
  contact_offset.y = controller_->position.y - surface_point_->y;
  contact_offset.z = controller_->position.z - surface_point_->z;
  separation =
    projected_extent[0] * projection_sign[0] + projected_extent[1] * projection_sign[1] +
    projected_extent[MATRIX_SURFACE_VECTOR_Z] * projection_sign[MATRIX_SURFACE_VECTOR_Z] +
    (contact_offset.x >> MATRIX_SURFACE_FIXED8_SHIFT) *
      (normal_->x >> MATRIX_SURFACE_FIXED8_SHIFT) +
    (contact_offset.y >> MATRIX_SURFACE_FIXED8_SHIFT) *
      (normal_->y >> MATRIX_SURFACE_FIXED8_SHIFT) +
    (contact_offset.z >> MATRIX_SURFACE_FIXED8_SHIFT) * (normal_->z >> MATRIX_SURFACE_FIXED8_SHIFT);
  g_VEHICLE_ORIENTATION_STABILITY_VALUE = separation;
  if(separation >= 0)
    return;

  contact_point.x = controller_->position.x;
  contact_point.y = controller_->position.y;
  contact_point.z = controller_->position.z;
  for(index = 0; index < MATRIX_SURFACE_COMPONENT_COUNT; index++)
    {
      contact_point.x += (half_extents[index] >> MATRIX_SURFACE_FIXED8_SHIFT) *
                         projection_sign[index] *
                         (matrix[index * MATRIX_SURFACE_MATRIX_ROW_STRIDE] >>
                          MATRIX_SURFACE_FIXED8_SHIFT);
      contact_point.y +=
        (half_extents[index] >> MATRIX_SURFACE_FIXED8_SHIFT) * projection_sign[index] *
        (matrix[index * MATRIX_SURFACE_MATRIX_ROW_STRIDE + 1] >> MATRIX_SURFACE_FIXED8_SHIFT);
      contact_point.z +=
        (half_extents[index] >> MATRIX_SURFACE_FIXED8_SHIFT) * projection_sign[index] *
        (matrix[index * MATRIX_SURFACE_MATRIX_ROW_STRIDE + 2] >> MATRIX_SURFACE_FIXED8_SHIFT);
    }

  correction_magnitude = -separation;
  correction.x         = 0;
  correction.y         = 0;
  correction.z         = 0;
  if((g_VEHICLE_COLLISION_POSITION_CORRECTION_ENABLED != 0) && (correction_magnitude != 0))
    {
      correction_magnitude >>= MATRIX_SURFACE_POSITION_CORRECTION_SHIFT;
      correction.x = MulSF16(correction_magnitude, normal_->x);
      correction.y = MulSF16(correction_magnitude, normal_->y);
      correction.z = MulSF16(correction_magnitude, normal_->z);
      controller_->position.x += correction.x;
      controller_->position.y += correction.y;
      controller_->position.z += correction.z;
    }

  contact_offset.x   = contact_point.x - controller_->position.x;
  contact_offset.y   = contact_point.y - controller_->position.y;
  contact_offset.z   = contact_point.z - controller_->position.z;
  contact_velocity.x = (contact_offset.z >> MATRIX_SURFACE_FIXED8_SHIFT) *
                         (controller_->angular_velocity.y >> MATRIX_SURFACE_FIXED8_SHIFT) -
                       (contact_offset.y >> MATRIX_SURFACE_FIXED8_SHIFT) *
                         (controller_->angular_velocity.z >> MATRIX_SURFACE_FIXED8_SHIFT) +
                       controller_->velocity.x;
  contact_velocity.y = (contact_offset.x >> MATRIX_SURFACE_FIXED8_SHIFT) *
                         (controller_->angular_velocity.z >> MATRIX_SURFACE_FIXED8_SHIFT) -
                       (contact_offset.z >> MATRIX_SURFACE_FIXED8_SHIFT) *
                         (controller_->angular_velocity.x >> MATRIX_SURFACE_FIXED8_SHIFT) +
                       controller_->velocity.y;
  contact_velocity.z = (contact_offset.y >> MATRIX_SURFACE_FIXED8_SHIFT) *
                         (controller_->angular_velocity.x >> MATRIX_SURFACE_FIXED8_SHIFT) -
                       (contact_offset.x >> MATRIX_SURFACE_FIXED8_SHIFT) *
                         (controller_->angular_velocity.y >> MATRIX_SURFACE_FIXED8_SHIFT) +
                       controller_->velocity.z;

  if(((contact_velocity.x >> MATRIX_SURFACE_FIXED8_SHIFT) *
        (normal_->x >> MATRIX_SURFACE_FIXED8_SHIFT) +
      (contact_velocity.y >> MATRIX_SURFACE_FIXED8_SHIFT) *
        (normal_->y >> MATRIX_SURFACE_FIXED8_SHIFT) +
      (contact_velocity.z >> MATRIX_SURFACE_FIXED8_SHIFT) *
        (normal_->z >> MATRIX_SURFACE_FIXED8_SHIFT)) < 0)
    {
      apply_matrix_surface_collision_response(controller_,
                                              &contact_point,
                                              &contact_velocity,
                                              normal_);
      contact_velocity.x = (contact_offset.z >> MATRIX_SURFACE_FIXED8_SHIFT) *
                             (controller_->angular_velocity.y >> MATRIX_SURFACE_FIXED8_SHIFT) -
                           (contact_offset.y >> MATRIX_SURFACE_FIXED8_SHIFT) *
                             (controller_->angular_velocity.z >> MATRIX_SURFACE_FIXED8_SHIFT) +
                           controller_->velocity.x;
      contact_velocity.y = (contact_offset.x >> MATRIX_SURFACE_FIXED8_SHIFT) *
                             (controller_->angular_velocity.z >> MATRIX_SURFACE_FIXED8_SHIFT) -
                           (contact_offset.z >> MATRIX_SURFACE_FIXED8_SHIFT) *
                             (controller_->angular_velocity.x >> MATRIX_SURFACE_FIXED8_SHIFT) +
                           controller_->velocity.y;
      contact_velocity.z = (contact_offset.y >> MATRIX_SURFACE_FIXED8_SHIFT) *
                             (controller_->angular_velocity.x >> MATRIX_SURFACE_FIXED8_SHIFT) -
                           (contact_offset.x >> MATRIX_SURFACE_FIXED8_SHIFT) *
                             (controller_->angular_velocity.y >> MATRIX_SURFACE_FIXED8_SHIFT) +
                           controller_->velocity.z;
      g_VEHICLE_COLLISION_CONTACT_VELOCITY = contact_velocity;
    }

  if((g_VEHICLE_COLLISION_POSITION_CORRECTION_ENABLED == 0) || (correction_magnitude == 0) ||
     (correction.y <= 0))
    return;

  vertical_speed_squared = MulSF16(controller_->velocity.y, controller_->velocity.y);
  vertical_speed_limit =
    MulSF16(MATRIX_SURFACE_GRAVITY_FIXED, correction.y) * MATRIX_SURFACE_DOUBLE_SCALE;
  if(vertical_speed_squared < vertical_speed_limit)
    {
      controller_->velocity.y = 0;
      return;
    }

  residual_speed = vertical_speed_squared - vertical_speed_limit;
  residual_speed = (int32) SqrtF16(residual_speed);
  if(controller_->velocity.y < 1)
    residual_speed = -residual_speed;
  controller_->velocity.y = residual_speed;
}

static int32
_matrix_surface_product_fixed8(int32 first_, int32 second_)
{
  return (int32) ((uint32) (first_ >> MATRIX_SURFACE_FIXED8_SHIFT) *
                   (uint32) (second_ >> MATRIX_SURFACE_FIXED8_SHIFT));
}

static int32
_matrix_surface_dot_fixed8(const Vector3 *first_, const Vector3 *second_)
{
  uint32 result;

  result = (uint32) _matrix_surface_product_fixed8(first_->x, second_->x);
  result += (uint32) _matrix_surface_product_fixed8(first_->y, second_->y);
  result += (uint32) _matrix_surface_product_fixed8(first_->z, second_->z);
  return (int32) result;
}

static void
_matrix_surface_cross_fixed8(const Vector3 *first_, const Vector3 *second_, Vector3 *result_)
{
  result_->x = (int32) ((uint32) _matrix_surface_product_fixed8(first_->y, second_->z) -
                        (uint32) _matrix_surface_product_fixed8(first_->z, second_->y));
  result_->y = (int32) ((uint32) _matrix_surface_product_fixed8(first_->z, second_->x) -
                        (uint32) _matrix_surface_product_fixed8(first_->x, second_->z));
  result_->z = (int32) ((uint32) _matrix_surface_product_fixed8(first_->x, second_->y) -
                        (uint32) _matrix_surface_product_fixed8(first_->y, second_->x));
}

void
apply_matrix_surface_collision_response(MatrixControllerState *controller_,
                                        const Vector3         *contact_point_,
                                        const Vector3         *contact_velocity_,
                                        const Vector3         *normal_)
{
  Vector3 contact_offset;
  Vector3 normal_torque_axis;
  Vector3 tangent_velocity;
  Vector3 tangent_direction;
  Vector3 impulse;
  Vector3 torque;
  int32   normal_speed;
  int32   normal_rotational_term;
  int32   denominator;
  int32   normal_impulse;
  int32   tangent_speed;
  int32   tangent_scale;
  int32   tangent_impulse;
  int32   tangent_effective_speed;
  int32   impulse_scale;

  contact_offset.x = contact_point_->x - controller_->position.x;
  contact_offset.y = contact_point_->y - controller_->position.y;
  contact_offset.z = contact_point_->z - controller_->position.z;
  _matrix_surface_cross_fixed8(&contact_offset, normal_, &normal_torque_axis);

  normal_speed =
    (int32) ((uint32) _matrix_surface_dot_fixed8(&controller_->velocity, normal_) +
             (uint32) _matrix_surface_dot_fixed8(&controller_->angular_velocity,
                                                &normal_torque_axis));
  normal_rotational_term = MulSF16(calculate_vector3_squared_magnitude_fixed16(&normal_torque_axis),
                                   controller_->second_reciprocal);
  denominator            = (controller_->first_reciprocal >> MATRIX_SURFACE_HALF_SHIFT) +
                           (normal_rotational_term >> MATRIX_SURFACE_HALF_SHIFT);
  normal_impulse         = DivSF16(-normal_speed, denominator);
  normal_impulse         = MulSF16(normal_impulse, g_VEHICLE_SURFACE_COLLISION_NORMAL_SCALE);

  normal_speed = _matrix_surface_dot_fixed8(contact_velocity_, normal_);
  tangent_velocity.x =
    (int32) ((uint32) contact_velocity_->x -
             (uint32) _matrix_surface_product_fixed8(normal_speed, normal_->x));
  tangent_velocity.y =
    (int32) ((uint32) contact_velocity_->y -
             (uint32) _matrix_surface_product_fixed8(normal_speed, normal_->y));
  tangent_velocity.z =
    (int32) ((uint32) contact_velocity_->z -
             (uint32) _matrix_surface_product_fixed8(normal_speed, normal_->z));
  tangent_speed =
    (int32) SqrtF16(_matrix_surface_dot_fixed8(&tangent_velocity, &tangent_velocity));
  tangent_direction.x = 0;
  tangent_direction.y = 0;
  tangent_direction.z = 0;
  if(tangent_speed != 0)
    {
      tangent_scale       = DivSF16(-MATRIX_SURFACE_FIXED_ONE, tangent_speed);
      tangent_direction.x = MulSF16(tangent_scale, tangent_velocity.x);
      tangent_direction.y = MulSF16(tangent_scale, tangent_velocity.y);
      tangent_direction.z = MulSF16(tangent_scale, tangent_velocity.z);
    }

  tangent_impulse = MulSF16(g_VEHICLE_SURFACE_COLLISION_FRICTION_SCALE, normal_impulse);
  impulse_scale   = MulSF16(tangent_impulse, controller_->first_reciprocal);
  impulse.x       = MulSF16(impulse_scale, tangent_direction.x);
  impulse.y       = MulSF16(impulse_scale, tangent_direction.y);
  impulse.z       = MulSF16(impulse_scale, tangent_direction.z);

  /*
   * Retail 0x9D64-0x9E08 evaluates the rotational friction contribution
   * in place: the second cross product consumes its overwritten X and Y.
   */
  torque.y = (int32) ((uint32) _matrix_surface_product_fixed8(contact_offset.x,
                                                             tangent_direction.z) -
                      (uint32) _matrix_surface_product_fixed8(contact_offset.z,
                                                             tangent_direction.x));
  torque.z = (int32) ((uint32) _matrix_surface_product_fixed8(contact_offset.y,
                                                             tangent_direction.x) -
                      (uint32) _matrix_surface_product_fixed8(contact_offset.x,
                                                             tangent_direction.y));
  torque.x = (int32) ((uint32) _matrix_surface_product_fixed8(contact_offset.z, torque.y) -
                      (uint32) _matrix_surface_product_fixed8(contact_offset.y, torque.z));
  torque.y = (int32) ((uint32) _matrix_surface_product_fixed8(contact_offset.x, torque.z) -
                      (uint32) _matrix_surface_product_fixed8(contact_offset.z, torque.x));
  torque.z = (int32) ((uint32) _matrix_surface_product_fixed8(contact_offset.y, torque.x) -
                      (uint32) _matrix_surface_product_fixed8(contact_offset.x, torque.y));
  impulse_scale = MulSF16(tangent_impulse, controller_->second_reciprocal);
  impulse.x = (int32) ((uint32) impulse.x + (uint32) MulSF16(impulse_scale, torque.x));
  impulse.y = (int32) ((uint32) impulse.y + (uint32) MulSF16(impulse_scale, torque.y));
  impulse.z = (int32) ((uint32) impulse.z + (uint32) MulSF16(impulse_scale, torque.z));
  tangent_effective_speed = _matrix_surface_dot_fixed8(&tangent_direction, &impulse);
  if(tangent_speed < tangent_effective_speed)
    {
      impulse_scale   = DivSF16(tangent_speed, tangent_effective_speed);
      tangent_impulse = MulSF16(tangent_impulse, impulse_scale);
    }

  impulse.x = MulSF16(normal_impulse, normal_->x) + MulSF16(tangent_impulse, tangent_direction.x);
  impulse.y = MulSF16(normal_impulse, normal_->y) + MulSF16(tangent_impulse, tangent_direction.y);
  impulse.z = MulSF16(normal_impulse, normal_->z) + MulSF16(tangent_impulse, tangent_direction.z);
  if(normal_impulse <= 0)
    return;

  g_VEHICLE_COLLISION_RESPONSE_MAGNITUDE = normal_impulse;
  controller_->velocity.x += MulSF16(controller_->first_reciprocal, impulse.x);
  controller_->velocity.y += MulSF16(controller_->first_reciprocal, impulse.y);
  controller_->velocity.z += MulSF16(controller_->first_reciprocal, impulse.z);

  _matrix_surface_cross_fixed8(&contact_offset, &impulse, &torque);
  controller_->angular_velocity.x += MulSF16(controller_->second_reciprocal, torque.x);
  controller_->angular_velocity.y += MulSF16(controller_->second_reciprocal, torque.y);
  controller_->angular_velocity.z += MulSF16(controller_->second_reciprocal, torque.z);
}
