#include "matrix_collision.h"

#include "vehicle_pair_collision.h"

#include "operamath.h"

#include "stddef.h"

#define MATRIX_COLLISION_MATRIX_ELEMENT_COUNT 9
#define MATRIX_COLLISION_VECTOR_ELEMENT_COUNT 3
#define MATRIX_COLLISION_CENTER_PROJECTION_COUNT 6
#define MATRIX_COLLISION_FIXED8_SHIFT 8
#define MATRIX_COLLISION_AXIS_X 0
#define MATRIX_COLLISION_AXIS_Y 1
#define MATRIX_COLLISION_AXIS_Z 2
#define MATRIX_COLLISION_POSITIVE_FACE 1
#define MATRIX_COLLISION_NEGATIVE_FACE -1
#define MATRIX_COLLISION_CORRECTION_SHIFT 5
#define MATRIX_COLLISION_NORMALIZED_VELOCITY_SHIFT 4
#define MATRIX_COLLISION_MASS_RATIO_SHIFT 2
#define MATRIX_COLLISION_MASS_PRODUCT_SHIFT 14
#define MATRIX_COLLISION_MAXIMUM_CORRECTIONS 0x20
#define MATRIX_COLLISION_VELOCITY_NORMAL_THRESHOLD 0xF0000
#define MATRIX_COLLISION_RELATIVE_VELOCITY_THRESHOLD 0x140000
#define MATRIX_COLLISION_RESERVED_THRESHOLD 0x320000
#define MATRIX_COLLISION_CENTER_DISTANCE_THRESHOLD 0x3333
#define MATRIX_COLLISION_PLANAR_DISTANCE_THRESHOLD 0x9999
#define MATRIX_COLLISION_ALIGNMENT_THRESHOLD 0xD999
#define MATRIX_COLLISION_SECONDARY_ALIGNMENT_THRESHOLD 0xC000
#define MATRIX_COLLISION_IMPULSE_SCALE_INITIAL 0x8CCC
#define MATRIX_COLLISION_CORNER_AXIS_X_MASK 2
#define MATRIX_COLLISION_CORNER_AXIS_Y_MASK 4
#define MATRIX_COLLISION_CORNER_AXIS_Z_MASK 1
#define MATRIX_COLLISION_CORNER_PPP 0
#define MATRIX_COLLISION_CORNER_PPN 1
#define MATRIX_COLLISION_CORNER_NPP 2
#define MATRIX_COLLISION_CORNER_NPN 3
#define MATRIX_COLLISION_CORNER_PNP 4
#define MATRIX_COLLISION_CORNER_PNN 5
#define MATRIX_COLLISION_CORNER_NNP 6
#define MATRIX_COLLISION_CORNER_NNN 7

typedef struct MatrixCollisionRuntimeHeader
{
  int32 distance_correction_disabled;
  int32 alignment_correction_disabled;
  int32 stationary_correction_enabled;
  int32 grouped_face_test_order;
  int32 repeated_correction_enabled;
  int32 secondary_alignment_enabled;
} MatrixCollisionRuntimeHeader;

typedef struct MatrixCollisionRuntimeThresholds
{
  int32 velocity_normal_selected;
  int32 velocity_normal_enabled;
  int32 force_velocity_normal;
  int32 relative_velocity_threshold;
  int32 reserved_threshold;
  int32 center_distance_threshold;
  int32 planar_distance_threshold;
  int32 alignment_threshold;
  int32 secondary_alignment_threshold;
} MatrixCollisionRuntimeThresholds;

typedef char SizeAssertMatrixCollisionRuntimeHeader
  [(sizeof(MatrixCollisionRuntimeHeader) == 0x18) ? 1 : -1];
typedef char SizeAssertMatrixCollisionRuntimeThresholds
  [(sizeof(MatrixCollisionRuntimeThresholds) == 0x24) ? 1 : -1];
typedef char SizeAssertMatrixControllerCollisionRadiusOffset
  [(offsetof(MatrixControllerState, collision_radius) == 0x70) ? 1 : -1];
typedef char SizeAssertMatrixControllerHalfExtentsOffset
  [(offsetof(MatrixControllerState, half_extents) == 0x74) ? 1 : -1];

MatrixCollisionRuntimeHeader g_MATRIX_COLLISION_RUNTIME_HEADER =
  {
    0,
    0,
    0,
    1,
    0,
    0
  };
MatrixCollisionRuntimeThresholds g_MATRIX_COLLISION_RUNTIME_THRESHOLDS =
  {
    0,
    1,
    0,
    MATRIX_COLLISION_RELATIVE_VELOCITY_THRESHOLD,
    MATRIX_COLLISION_RESERVED_THRESHOLD,
    MATRIX_COLLISION_CENTER_DISTANCE_THRESHOLD,
    MATRIX_COLLISION_PLANAR_DISTANCE_THRESHOLD,
    MATRIX_COLLISION_ALIGNMENT_THRESHOLD,
    MATRIX_COLLISION_SECONDARY_ALIGNMENT_THRESHOLD
  };
typedef char SizeAssertMatrixCollisionRuntimeHeaderObject
  [(sizeof(g_MATRIX_COLLISION_RUNTIME_HEADER) == 0x18) ? 1 : -1];
typedef char SizeAssertMatrixCollisionRuntimeThresholdsObject
  [(sizeof(g_MATRIX_COLLISION_RUNTIME_THRESHOLDS) == 0x24) ? 1 : -1];
int32 g_MATRIX_COLLISION_RUNTIME_RESERVED_MIDDLE = 1;
int32 g_MATRIX_COLLISION_RUNTIME_RESERVED_TAIL   = 1;
int32 g_MATRIX_COLLISION_IMPULSE_SCALE           = MATRIX_COLLISION_IMPULSE_SCALE_INITIAL;
int32 g_MATRIX_COLLISION_ALTERNATE_MODE;

static int32
_matrix_collision_dot_fixed8(const int32 *first_, const int32 *second_)
{
  uint32 result;

  result = (uint32) (first_[0] >> MATRIX_COLLISION_FIXED8_SHIFT) *
           (uint32) (second_[0] >> MATRIX_COLLISION_FIXED8_SHIFT);
  result += (uint32) (first_[1] >> MATRIX_COLLISION_FIXED8_SHIFT) *
            (uint32) (second_[1] >> MATRIX_COLLISION_FIXED8_SHIFT);
  result += (uint32) (first_[MATRIX_COLLISION_AXIS_Z] >> MATRIX_COLLISION_FIXED8_SHIFT) *
            (uint32) (second_[MATRIX_COLLISION_AXIS_Z] >> MATRIX_COLLISION_FIXED8_SHIFT);
  return (int32) result;
}

static void
_project_matrix_collision_center(const MatrixControllerState *first_state_,
                                 const MatrixControllerState *second_state_,
                                 int32                       *center_projection_)
{
  const int32 *first_matrix;
  const int32 *second_matrix;
  int32        center_delta[MATRIX_COLLISION_VECTOR_ELEMENT_COUNT];
  int32        axis;

  first_matrix  = (const int32 *) &first_state_->matrix;
  second_matrix = (const int32 *) &second_state_->matrix;
  center_delta[0] =
    (int32) ((uint32) second_state_->position.x - (uint32) first_state_->position.x);
  center_delta[1] =
    (int32) ((uint32) second_state_->position.y - (uint32) first_state_->position.y);
  center_delta[MATRIX_COLLISION_AXIS_Z] =
    (int32) ((uint32) second_state_->position.z - (uint32) first_state_->position.z);

  for(axis = 0; axis < MATRIX_COLLISION_VECTOR_ELEMENT_COUNT; axis += 1)
    {
      center_projection_[axis] =
        _matrix_collision_dot_fixed8(center_delta,
                                     &first_matrix[axis * MATRIX_COLLISION_VECTOR_ELEMENT_COUNT]);
      center_projection_[axis + MATRIX_COLLISION_VECTOR_ELEMENT_COUNT] =
        _matrix_collision_dot_fixed8(center_delta,
                                     &second_matrix[axis * MATRIX_COLLISION_VECTOR_ELEMENT_COUNT]);
    }
}

static void
_prepare_matrix_collision_extent_projections(const MatrixControllerState *first_state_,
                                             const MatrixControllerState *second_state_,
                                             int32                       *second_extent_projection_,
                                             int32                       *first_extent_projection_)
{
  const int32 *first_matrix;
  const int32 *second_matrix;
  const int32 *first_extents;
  const int32 *second_extents;
  int32        relative_matrix[MATRIX_COLLISION_MATRIX_ELEMENT_COUNT];
  int32        first_axis;
  int32        second_axis;
  int32        element;

  first_matrix   = (const int32 *) &first_state_->matrix;
  second_matrix  = (const int32 *) &second_state_->matrix;
  first_extents  = (const int32 *) &first_state_->half_extents;
  second_extents = (const int32 *) &second_state_->half_extents;

  for(first_axis = 0; first_axis < MATRIX_COLLISION_VECTOR_ELEMENT_COUNT; first_axis += 1)
    {
      for(second_axis = 0; second_axis < MATRIX_COLLISION_VECTOR_ELEMENT_COUNT; second_axis += 1)
        {
          element = first_axis * MATRIX_COLLISION_VECTOR_ELEMENT_COUNT + second_axis;
          relative_matrix[element] = _matrix_collision_dot_fixed8(
            &first_matrix[first_axis * MATRIX_COLLISION_VECTOR_ELEMENT_COUNT],
            &second_matrix[second_axis * MATRIX_COLLISION_VECTOR_ELEMENT_COUNT]);
          second_extent_projection_[element] =
            (int32) ((uint32) (second_extents[second_axis] >> MATRIX_COLLISION_FIXED8_SHIFT) *
                     (uint32) (relative_matrix[element] >> MATRIX_COLLISION_FIXED8_SHIFT));
          first_extent_projection_[element] =
            (int32) ((uint32) (first_extents[first_axis] >> MATRIX_COLLISION_FIXED8_SHIFT) *
                     (uint32) (relative_matrix[element] >> MATRIX_COLLISION_FIXED8_SHIFT));
        }
    }
}

static int32
_matrix_collision_absolute(int32 value_)
{
  if(value_ <= 0)
    return (int32) (0U - (uint32) value_);

  return value_;
}

static int32
_matrix_collision_maximum_absolute(const Vector3 *vector_)
{
  int32 maximum;
  int32 component;

  maximum   = _matrix_collision_absolute(vector_->x);
  component = _matrix_collision_absolute(vector_->y);
  if(component > maximum)
    maximum = component;
  component = _matrix_collision_absolute(vector_->z);
  if(component > maximum)
    maximum = component;

  return maximum;
}

static void
_subtract_matrix_collision_vectors(const Vector3 *first_,
                                   const Vector3 *second_,
                                   Vector3       *difference_)
{
  difference_->x = (int32) ((uint32) first_->x - (uint32) second_->x);
  difference_->y = (int32) ((uint32) first_->y - (uint32) second_->y);
  difference_->z = (int32) ((uint32) first_->z - (uint32) second_->z);
}

static void
_copy_matrix_collision_vector(const Vector3 *source_, Vector3 *destination_)
{
  destination_->x = source_->x;
  destination_->y = source_->y;
  destination_->z = source_->z;
}

static int32
_multiply_matrix_collision_shifted(int32 first_, int32 second_, int32 shift_)
{
  int32 product;

  product = (int32) ((uint32) first_ * (uint32) second_);
  return product >> shift_;
}

static void
_calculate_velocity_distance_correction(const Vector3 *velocity_,
                                        uint32         reciprocal_distance_,
                                        Vector3       *correction_)
{
  int32 scale;

  scale          = (int32) (0U - reciprocal_distance_) >> MATRIX_COLLISION_FIXED8_SHIFT;
  correction_->x = _multiply_matrix_collision_shifted(velocity_->x >> MATRIX_COLLISION_FIXED8_SHIFT,
                                                      scale,
                                                      MATRIX_COLLISION_CORRECTION_SHIFT);
  correction_->y = _multiply_matrix_collision_shifted(velocity_->y >> MATRIX_COLLISION_FIXED8_SHIFT,
                                                      scale,
                                                      MATRIX_COLLISION_CORRECTION_SHIFT);
  correction_->z = _multiply_matrix_collision_shifted(velocity_->z >> MATRIX_COLLISION_FIXED8_SHIFT,
                                                      scale,
                                                      MATRIX_COLLISION_CORRECTION_SHIFT);
}

static void
_calculate_mass_weighted_corrections(const MatrixControllerState *first_state_,
                                     const MatrixControllerState *second_state_,
                                     const Vector3               *center_delta_,
                                     Vector3                     *first_correction_,
                                     Vector3                     *second_correction_)
{
  Vector3 normalized_delta;
  int32   total_value;
  int32   first_ratio;
  int32   second_ratio;

  _copy_matrix_collision_vector(center_delta_, &normalized_delta);
  normalize_vector3_in_place(&normalized_delta);
  total_value = (int32) ((uint32) first_state_->first_value + (uint32) second_state_->first_value);
  first_ratio =
    DivSF16(first_state_->first_value, total_value) >> MATRIX_COLLISION_MASS_RATIO_SHIFT;
  second_ratio =
    DivSF16(second_state_->first_value, total_value) >> MATRIX_COLLISION_MASS_RATIO_SHIFT;

  first_correction_->x  = (int32) (0U - (uint32) _multiply_matrix_collision_shifted(
                                          second_ratio,
                                          normalized_delta.x,
                                          MATRIX_COLLISION_MASS_PRODUCT_SHIFT)) >>
                          MATRIX_COLLISION_CORRECTION_SHIFT;
  first_correction_->y  = (int32) (0U - (uint32) _multiply_matrix_collision_shifted(
                                          second_ratio,
                                          normalized_delta.y,
                                          MATRIX_COLLISION_MASS_PRODUCT_SHIFT)) >>
                          MATRIX_COLLISION_CORRECTION_SHIFT;
  first_correction_->z  = (int32) (0U - (uint32) _multiply_matrix_collision_shifted(
                                          second_ratio,
                                          normalized_delta.z,
                                          MATRIX_COLLISION_MASS_PRODUCT_SHIFT)) >>
                          MATRIX_COLLISION_CORRECTION_SHIFT;
  second_correction_->x = _multiply_matrix_collision_shifted(first_ratio,
                                                             normalized_delta.x,
                                                             MATRIX_COLLISION_MASS_PRODUCT_SHIFT) >>
                          MATRIX_COLLISION_CORRECTION_SHIFT;
  second_correction_->y = _multiply_matrix_collision_shifted(first_ratio,
                                                             normalized_delta.y,
                                                             MATRIX_COLLISION_MASS_PRODUCT_SHIFT) >>
                          MATRIX_COLLISION_CORRECTION_SHIFT;
  second_correction_->z = _multiply_matrix_collision_shifted(first_ratio,
                                                             normalized_delta.z,
                                                             MATRIX_COLLISION_MASS_PRODUCT_SHIFT) >>
                          MATRIX_COLLISION_CORRECTION_SHIFT;
}

static void
_add_matrix_collision_position(Vector3 *position_, const Vector3 *correction_)
{
  position_->x = (int32) ((uint32) position_->x + (uint32) correction_->x);
  position_->y = (int32) ((uint32) position_->y + (uint32) correction_->y);
  position_->z = (int32) ((uint32) position_->z + (uint32) correction_->z);
}

static int32
_matrix_collision_apply_sign(int32 value_, int32 sign_)
{
  if(sign_ < 0)
    return (int32) (0U - (uint32) value_);

  return value_;
}

static int32
_matrix_collision_add_signed(int32 value_, int32 addend_, int32 sign_)
{
  return (int32) ((uint32) value_ + (uint32) _matrix_collision_apply_sign(addend_, sign_));
}

static int32
_matrix_collision_point_inside_extents(const Vector3 *point_, const Vector3 *extents_)
{
  const int32 *point;
  const int32 *extents;
  int32        axis;
  int32        negative_extent;

  point   = (const int32 *) point_;
  extents = (const int32 *) extents_;
  for(axis = 0; axis < MATRIX_COLLISION_VECTOR_ELEMENT_COUNT; axis += 1)
    {
      negative_extent = (int32) (0U - (uint32) extents[axis]);
      if(point[axis] > extents[axis] || point[axis] < negative_extent)
        return 0;
    }

  return 1;
}

static void
_calculate_matrix_collision_face_point(const MatrixControllerState *state_,
                                       int32                        axis_,
                                       int32                        sign_,
                                       Vector3                     *point_)
{
  const int32 *matrix;
  const int32 *position;
  const int32 *extents;
  int32       *point;
  int32        component;
  int32        product;

  matrix   = (const int32 *) &state_->matrix;
  position = (const int32 *) &state_->position;
  extents  = (const int32 *) &state_->half_extents;
  point    = (int32 *) point_;
  for(component = 0; component < MATRIX_COLLISION_VECTOR_ELEMENT_COUNT; component += 1)
    {
      product =
        (int32) ((uint32) (extents[axis_] >> MATRIX_COLLISION_FIXED8_SHIFT) *
                 (uint32) (matrix[axis_ * MATRIX_COLLISION_VECTOR_ELEMENT_COUNT + component] >>
                           MATRIX_COLLISION_FIXED8_SHIFT));
      point[component] = _matrix_collision_add_signed(position[component], product, sign_);
    }
}

static void
_set_matrix_collision_axis_normal(const MatrixControllerState *state_,
                                  int32                        axis_,
                                  int32                        sign_,
                                  Vector3                     *normal_)
{
  const int32 *matrix;
  int32       *normal;
  int32        component;

  matrix = (const int32 *) &state_->matrix;
  normal = (int32 *) normal_;
  for(component = 0; component < MATRIX_COLLISION_VECTOR_ELEMENT_COUNT; component += 1)
    {
      normal[component] = _matrix_collision_apply_sign(
        matrix[axis_ * MATRIX_COLLISION_VECTOR_ELEMENT_COUNT + component],
        sign_);
    }
}

static int32
_test_matrix_collision_face(const MatrixControllerState *first_state_,
                            const MatrixControllerState *second_state_,
                            Vector3                     *contact_point_,
                            Vector3                     *collision_normal_,
                            const int32                 *second_extent_projection_,
                            const int32                 *first_extent_projection_,
                            const int32                 *center_projection_,
                            int32                        use_second_face_,
                            int32                        axis_,
                            int32                        sign_)
{
  Vector3 projected_point;
  int32  *projected;
  int32   projection_axis;
  int32   projection;

  projected = (int32 *) &projected_point;
  for(projection_axis = 0; projection_axis < MATRIX_COLLISION_VECTOR_ELEMENT_COUNT;
      projection_axis += 1)
    {
      if(use_second_face_ != 0)
        {
          projection =
            second_extent_projection_[projection_axis * MATRIX_COLLISION_VECTOR_ELEMENT_COUNT +
                                      axis_];
          projected[projection_axis] =
            _matrix_collision_add_signed(center_projection_[projection_axis], projection, sign_);
        }
      else
        {
          projection = first_extent_projection_[axis_ * MATRIX_COLLISION_VECTOR_ELEMENT_COUNT +
                                                projection_axis];
          projected[projection_axis] = _matrix_collision_add_signed(
            (int32) (0U -
                     (uint32)
                       center_projection_[projection_axis + MATRIX_COLLISION_VECTOR_ELEMENT_COUNT]),
            projection,
            sign_);
        }
    }

  if(use_second_face_ != 0)
    {
      if(_matrix_collision_point_inside_extents(&projected_point, &first_state_->half_extents) == 0)
        return 0;
      _set_matrix_collision_axis_normal(second_state_, axis_, sign_, collision_normal_);
      _calculate_matrix_collision_face_point(second_state_, axis_, sign_, contact_point_);
    }
  else
    {
      if(_matrix_collision_point_inside_extents(&projected_point, &second_state_->half_extents) ==
         0)
        return 0;
      _set_matrix_collision_axis_normal(first_state_, axis_, -sign_, collision_normal_);
      _calculate_matrix_collision_face_point(first_state_, axis_, sign_, contact_point_);
    }

  return 1;
}

static int32
_find_matrix_collision_face_contact(const MatrixControllerState *first_state_,
                                    const MatrixControllerState *second_state_,
                                    Vector3                     *contact_point_,
                                    Vector3                     *collision_normal_,
                                    const int32                 *second_extent_projection_,
                                    const int32                 *first_extent_projection_,
                                    const int32                 *center_projection_)
{
#define TEST_MATRIX_COLLISION_FACE(source_, axis_, sign_)                                          \
  if(_test_matrix_collision_face(first_state_,                                                     \
                                 second_state_,                                                    \
                                 contact_point_,                                                   \
                                 collision_normal_,                                                \
                                 second_extent_projection_,                                        \
                                 first_extent_projection_,                                         \
                                 center_projection_,                                               \
                                 source_,                                                          \
                                 axis_,                                                            \
                                 sign_) != 0)                                                      \
  return 1

  if(g_MATRIX_COLLISION_RUNTIME_HEADER.grouped_face_test_order == 0)
    {
      TEST_MATRIX_COLLISION_FACE(1, MATRIX_COLLISION_AXIS_Z, MATRIX_COLLISION_POSITIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(1, MATRIX_COLLISION_AXIS_Z, MATRIX_COLLISION_NEGATIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(0, MATRIX_COLLISION_AXIS_Z, MATRIX_COLLISION_POSITIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(0, MATRIX_COLLISION_AXIS_Z, MATRIX_COLLISION_NEGATIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(1, MATRIX_COLLISION_AXIS_X, MATRIX_COLLISION_POSITIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(1, MATRIX_COLLISION_AXIS_X, MATRIX_COLLISION_NEGATIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(0, MATRIX_COLLISION_AXIS_X, MATRIX_COLLISION_POSITIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(0, MATRIX_COLLISION_AXIS_X, MATRIX_COLLISION_NEGATIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(1, MATRIX_COLLISION_AXIS_Y, MATRIX_COLLISION_POSITIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(1, MATRIX_COLLISION_AXIS_Y, MATRIX_COLLISION_NEGATIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(0, MATRIX_COLLISION_AXIS_Y, MATRIX_COLLISION_POSITIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(0, MATRIX_COLLISION_AXIS_Y, MATRIX_COLLISION_NEGATIVE_FACE);
    }
  else
    {
      TEST_MATRIX_COLLISION_FACE(1, MATRIX_COLLISION_AXIS_X, MATRIX_COLLISION_POSITIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(1, MATRIX_COLLISION_AXIS_Y, MATRIX_COLLISION_POSITIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(1, MATRIX_COLLISION_AXIS_Z, MATRIX_COLLISION_POSITIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(1, MATRIX_COLLISION_AXIS_X, MATRIX_COLLISION_NEGATIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(1, MATRIX_COLLISION_AXIS_Y, MATRIX_COLLISION_NEGATIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(1, MATRIX_COLLISION_AXIS_Z, MATRIX_COLLISION_NEGATIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(0, MATRIX_COLLISION_AXIS_X, MATRIX_COLLISION_POSITIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(0, MATRIX_COLLISION_AXIS_Y, MATRIX_COLLISION_POSITIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(0, MATRIX_COLLISION_AXIS_Z, MATRIX_COLLISION_POSITIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(0, MATRIX_COLLISION_AXIS_X, MATRIX_COLLISION_NEGATIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(0, MATRIX_COLLISION_AXIS_Y, MATRIX_COLLISION_NEGATIVE_FACE);
      TEST_MATRIX_COLLISION_FACE(0, MATRIX_COLLISION_AXIS_Z, MATRIX_COLLISION_NEGATIVE_FACE);
    }

#undef TEST_MATRIX_COLLISION_FACE
  return 0;
}

static void
_calculate_matrix_collision_corner_point(const MatrixControllerState *state_,
                                         const int32                 *signs_,
                                         Vector3                     *point_)
{
  const int32 *matrix;
  const int32 *position;
  const int32 *extents;
  int32       *point;
  int32        component;
  int32        axis;
  int32        product;

  matrix   = (const int32 *) &state_->matrix;
  position = (const int32 *) &state_->position;
  extents  = (const int32 *) &state_->half_extents;
  point    = (int32 *) point_;
  for(component = 0; component < MATRIX_COLLISION_VECTOR_ELEMENT_COUNT; component += 1)
    {
      point[component] = position[component];
      for(axis = 0; axis < MATRIX_COLLISION_VECTOR_ELEMENT_COUNT; axis += 1)
        {
          product =
            (int32) ((uint32) (extents[axis] >> MATRIX_COLLISION_FIXED8_SHIFT) *
                     (uint32) (matrix[axis * MATRIX_COLLISION_VECTOR_ELEMENT_COUNT + component] >>
                               MATRIX_COLLISION_FIXED8_SHIFT));
          point[component] = _matrix_collision_add_signed(point[component], product, signs_[axis]);
        }
    }
}

static int32
_matrix_collision_corner_normal_sign(int32 projection_, int32 source_is_second_)
{
  if(source_is_second_ != 0)
    return projection_ > 0 ? -1 : 1;

  return projection_ < 0 ? -1 : 1;
}

static void
_select_matrix_collision_corner_normal(const MatrixControllerState *container_state_,
                                       const MatrixControllerState *source_state_,
                                       const Vector3               *projected_point_,
                                       int32                        velocity_normal_selected_,
                                       int32                        source_is_second_,
                                       Vector3                     *collision_normal_)
{
  const int32 *projected;
  const int32 *extents;
  int32        scores[MATRIX_COLLISION_VECTOR_ELEMENT_COUNT];
  Vector3      candidate_normal;
  int32        axis;
  int32        selected_axis;
  int32        sign;

  projected = (const int32 *) projected_point_;
  extents   = (const int32 *) &container_state_->half_extents;
  for(axis = 0; axis < MATRIX_COLLISION_VECTOR_ELEMENT_COUNT; axis += 1)
    {
      sign = _matrix_collision_corner_normal_sign(projected[axis], source_is_second_);
      if(velocity_normal_selected_ != 0)
        {
          _set_matrix_collision_axis_normal(container_state_, axis, sign, &candidate_normal);
          scores[axis] = _matrix_collision_absolute(
            _matrix_collision_dot_fixed8((const int32 *) &source_state_->velocity,
                                         (const int32 *) &candidate_normal));
        }
      else
        {
          scores[axis] =
            (int32) ((uint32) extents[axis] - (uint32) _matrix_collision_absolute(projected[axis]));
        }
    }

  selected_axis = MATRIX_COLLISION_AXIS_Z;
  if(velocity_normal_selected_ != 0)
    {
      if(scores[1] > scores[selected_axis])
        selected_axis = 1;
      if(scores[0] > scores[selected_axis])
        selected_axis = 0;
    }
  else
    {
      if(scores[1] < scores[selected_axis])
        selected_axis = 1;
      if(scores[0] < scores[selected_axis])
        selected_axis = 0;
    }

  sign = _matrix_collision_corner_normal_sign(projected[selected_axis], source_is_second_);
  _set_matrix_collision_axis_normal(container_state_, selected_axis, sign, collision_normal_);
}

#define TEST_MATRIX_COLLISION_CORNER(corner_, source_is_second_)                                   \
  do                                                                                               \
    {                                                                                              \
      signs[0] = ((corner_) & MATRIX_COLLISION_CORNER_AXIS_X_MASK) != 0 ? -1 : 1;                  \
      signs[1] = ((corner_) & MATRIX_COLLISION_CORNER_AXIS_Y_MASK) != 0 ? -1 : 1;                  \
      signs[MATRIX_COLLISION_AXIS_Z] =                                                             \
        ((corner_) & MATRIX_COLLISION_CORNER_AXIS_Z_MASK) != 0 ? -1 : 1;                           \
      for(container_axis = 0; container_axis < MATRIX_COLLISION_VECTOR_ELEMENT_COUNT;              \
          container_axis += 1)                                                                     \
        {                                                                                          \
          if((source_is_second_) != 0)                                                             \
            projected[container_axis] = center_projection_[container_axis];                        \
          else                                                                                     \
            projected[container_axis] =                                                            \
              (int32) (0U - (uint32) center_projection_[container_axis +                           \
                                                        MATRIX_COLLISION_VECTOR_ELEMENT_COUNT]);   \
          for(source_axis = 0; source_axis < MATRIX_COLLISION_VECTOR_ELEMENT_COUNT;                \
              source_axis += 1)                                                                    \
            {                                                                                      \
              if((source_is_second_) != 0)                                                         \
                projection_index =                                                                 \
                  container_axis * MATRIX_COLLISION_VECTOR_ELEMENT_COUNT + source_axis;            \
              else                                                                                 \
                projection_index =                                                                 \
                  source_axis * MATRIX_COLLISION_VECTOR_ELEMENT_COUNT + container_axis;            \
              projected[container_axis] =                                                          \
                _matrix_collision_add_signed(projected[container_axis],                            \
                                             extent_projection_[projection_index],                 \
                                             signs[source_axis]);                                  \
            }                                                                                      \
        }                                                                                          \
      if(_matrix_collision_point_inside_extents(&projected_point,                                  \
                                                &container_state_->half_extents) != 0)             \
        {                                                                                          \
          _calculate_matrix_collision_corner_point(source_state_, signs, contact_point_);          \
          _select_matrix_collision_corner_normal(container_state_,                                 \
                                                 source_state_,                                    \
                                                 &projected_point,                                 \
                                                 velocity_normal_selected,                         \
                                                 (source_is_second_),                              \
                                                 collision_normal_);                               \
          return 1;                                                                                \
        }                                                                                          \
    }                                                                                              \
  while(0)

#define TEST_ALL_MATRIX_COLLISION_CORNERS(source_is_second_)                                       \
  TEST_MATRIX_COLLISION_CORNER(MATRIX_COLLISION_CORNER_PPP, source_is_second_);                    \
  TEST_MATRIX_COLLISION_CORNER(MATRIX_COLLISION_CORNER_PPN, source_is_second_);                    \
  TEST_MATRIX_COLLISION_CORNER(MATRIX_COLLISION_CORNER_NPP, source_is_second_);                    \
  TEST_MATRIX_COLLISION_CORNER(MATRIX_COLLISION_CORNER_NPN, source_is_second_);                    \
  TEST_MATRIX_COLLISION_CORNER(MATRIX_COLLISION_CORNER_PNP, source_is_second_);                    \
  TEST_MATRIX_COLLISION_CORNER(MATRIX_COLLISION_CORNER_PNN, source_is_second_);                    \
  TEST_MATRIX_COLLISION_CORNER(MATRIX_COLLISION_CORNER_NNP, source_is_second_);                    \
  TEST_MATRIX_COLLISION_CORNER(MATRIX_COLLISION_CORNER_NNN, source_is_second_)

static int32
_find_second_matrix_collision_corner_contact(const MatrixControllerState *container_state_,
                                             const MatrixControllerState *source_state_,
                                             Vector3                     *contact_point_,
                                             Vector3                     *collision_normal_,
                                             const int32                 *extent_projection_,
                                             const int32                 *center_projection_)
{
  Vector3 projected_point;
  Vector3 relative_velocity;
  int32  *projected;
  int32   signs[MATRIX_COLLISION_VECTOR_ELEMENT_COUNT];
  int32   container_axis;
  int32   source_axis;
  int32   projection_index;
  int32   velocity_normal_selected;

  /* Retail publishes this choice independently for each corner-source path. */
  g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.velocity_normal_selected = 0;
  if(g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.force_velocity_normal != 0)
    {
      g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.velocity_normal_selected = 1;
    }
  else if(g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.velocity_normal_enabled != 0)
    {
      _subtract_matrix_collision_vectors(&source_state_->velocity,
                                         &container_state_->velocity,
                                         &relative_velocity);
      if(_matrix_collision_maximum_absolute(&relative_velocity) >
         MATRIX_COLLISION_VELOCITY_NORMAL_THRESHOLD)
        {
          g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.velocity_normal_selected = 1;
        }
    }

  velocity_normal_selected =
    g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.velocity_normal_selected;
  projected = (int32 *) &projected_point;
  TEST_ALL_MATRIX_COLLISION_CORNERS(1);
  return 0;
}

static int32
_find_first_matrix_collision_corner_contact(const MatrixControllerState *container_state_,
                                            const MatrixControllerState *source_state_,
                                            Vector3                     *contact_point_,
                                            Vector3                     *collision_normal_,
                                            const int32                 *extent_projection_,
                                            const int32                 *center_projection_)
{
  Vector3 projected_point;
  Vector3 relative_velocity;
  int32  *projected;
  int32   signs[MATRIX_COLLISION_VECTOR_ELEMENT_COUNT];
  int32   container_axis;
  int32   source_axis;
  int32   projection_index;
  int32   velocity_normal_selected;

  /* Retail publishes this choice independently for each corner-source path. */
  g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.velocity_normal_selected = 0;
  if(g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.force_velocity_normal != 0)
    {
      g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.velocity_normal_selected = 1;
    }
  else if(g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.velocity_normal_enabled != 0)
    {
      _subtract_matrix_collision_vectors(&source_state_->velocity,
                                         &container_state_->velocity,
                                         &relative_velocity);
      if(_matrix_collision_maximum_absolute(&relative_velocity) >
         MATRIX_COLLISION_VELOCITY_NORMAL_THRESHOLD)
        {
          g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.velocity_normal_selected = 1;
        }
    }

  velocity_normal_selected =
    g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.velocity_normal_selected;
  projected = (int32 *) &projected_point;
  TEST_ALL_MATRIX_COLLISION_CORNERS(0);
  return 0;
}

#undef TEST_ALL_MATRIX_COLLISION_CORNERS
#undef TEST_MATRIX_COLLISION_CORNER

int32
find_matrix_collision_contact(MatrixControllerState *first_state_,
                              MatrixControllerState *second_state_,
                              Vector3               *contact_point_,
                              Vector3               *collision_normal_,
                              const int32           *second_extent_projection_,
                              const int32           *first_extent_projection_,
                              const int32           *center_projection_)
{
  if(_find_matrix_collision_face_contact(first_state_,
                                         second_state_,
                                         contact_point_,
                                         collision_normal_,
                                         second_extent_projection_,
                                         first_extent_projection_,
                                         center_projection_) != 0)
    return 1;

  if(_find_second_matrix_collision_corner_contact(first_state_,
                                                  second_state_,
                                                  contact_point_,
                                                  collision_normal_,
                                                  second_extent_projection_,
                                                  center_projection_) != 0)
    return 1;

  if(_find_first_matrix_collision_corner_contact(second_state_,
                                                 first_state_,
                                                 contact_point_,
                                                 collision_normal_,
                                                 first_extent_projection_,
                                                 center_projection_) != 0)
    return 1;

  return 0;
}

int32
resolve_alternate_matrix_collision(MatrixControllerState *first_state_,
                                   MatrixControllerState *second_state_,
                                   Vector3               *first_contact_,
                                   Vector3               *second_contact_)
{
  int32   second_extent_projection[MATRIX_COLLISION_MATRIX_ELEMENT_COUNT];
  int32   first_extent_projection[MATRIX_COLLISION_MATRIX_ELEMENT_COUNT];
  int32   center_projection[MATRIX_COLLISION_CENTER_PROJECTION_COUNT];
  Vector3 first_reverse_step;
  Vector3 second_reverse_step;
  Vector3 first_forward_step;
  Vector3 second_forward_step;

  _prepare_matrix_collision_extent_projections(first_state_,
                                               second_state_,
                                               second_extent_projection,
                                               first_extent_projection);

  _project_matrix_collision_center(first_state_, second_state_, center_projection);
  if(find_matrix_collision_contact(first_state_,
                                   second_state_,
                                   first_contact_,
                                   second_contact_,
                                   second_extent_projection,
                                   first_extent_projection,
                                   center_projection) == 0)
    return 0;

  first_reverse_step.x  = (int32) (0U - (uint32) MulSF16(first_state_->velocity.x, 0));
  first_reverse_step.y  = (int32) (0U - (uint32) MulSF16(first_state_->velocity.y, 0));
  first_reverse_step.z  = (int32) (0U - (uint32) MulSF16(first_state_->velocity.z, 0));
  second_reverse_step.x = (int32) (0U - (uint32) MulSF16(second_state_->velocity.x, 0));
  second_reverse_step.y = (int32) (0U - (uint32) MulSF16(second_state_->velocity.y, 0));
  second_reverse_step.z = (int32) (0U - (uint32) MulSF16(second_state_->velocity.z, 0));
  first_state_->position.x =
    (int32) ((uint32) first_state_->position.x + (uint32) first_reverse_step.x);
  first_state_->position.y =
    (int32) ((uint32) first_state_->position.y + (uint32) first_reverse_step.y);
  first_state_->position.z =
    (int32) ((uint32) first_state_->position.z + (uint32) first_reverse_step.z);
  second_state_->position.x =
    (int32) ((uint32) second_state_->position.x + (uint32) second_reverse_step.x);
  second_state_->position.y =
    (int32) ((uint32) second_state_->position.y + (uint32) second_reverse_step.y);
  second_state_->position.z =
    (int32) ((uint32) second_state_->position.z + (uint32) second_reverse_step.z);

  first_forward_step.x  = MulSF16(first_state_->velocity.x, 0);
  first_forward_step.y  = MulSF16(first_state_->velocity.y, 0);
  first_forward_step.z  = MulSF16(first_state_->velocity.z, 0);
  second_forward_step.x = MulSF16(second_state_->velocity.x, 0);
  second_forward_step.y = MulSF16(second_state_->velocity.y, 0);
  second_forward_step.z = MulSF16(second_state_->velocity.z, 0);

  _project_matrix_collision_center(first_state_, second_state_, center_projection);
  while(find_matrix_collision_contact(first_state_,
                                      second_state_,
                                      first_contact_,
                                      second_contact_,
                                      second_extent_projection,
                                      first_extent_projection,
                                      center_projection) == 0)
    {
      first_state_->position.x =
        (int32) ((uint32) first_state_->position.x + (uint32) first_forward_step.x);
      first_state_->position.y =
        (int32) ((uint32) first_state_->position.y + (uint32) first_forward_step.y);
      first_state_->position.z =
        (int32) ((uint32) first_state_->position.z + (uint32) first_forward_step.z);
      second_state_->position.x =
        (int32) ((uint32) second_state_->position.x + (uint32) second_forward_step.x);
      second_state_->position.y =
        (int32) ((uint32) second_state_->position.y + (uint32) second_forward_step.y);
      second_state_->position.z =
        (int32) ((uint32) second_state_->position.z + (uint32) second_forward_step.z);
      _project_matrix_collision_center(first_state_, second_state_, center_projection);
    }
  return 1;
}

static void
_calculate_standard_matrix_collision_corrections(const MatrixControllerState *first_state_,
                                                 const MatrixControllerState *second_state_,
                                                 const Vector3               *center_delta_,
                                                 Vector3                     *first_correction_,
                                                 Vector3                     *second_correction_)
{
  Vector3 relative_velocity;
  Vector3 normalized_velocity;
  Vector3 normalized_center;
  uint32  reciprocal_distance;
  int32   center_maximum;
  int32   planar_minimum;
  int32   alignment;
  int32   use_distance_correction;

  _subtract_matrix_collision_vectors(&first_state_->velocity,
                                     &second_state_->velocity,
                                     &relative_velocity);
  reciprocal_distance     = 0;
  use_distance_correction = 0;
  if(_matrix_collision_maximum_absolute(&relative_velocity) >
     g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.relative_velocity_threshold)
    {
      center_maximum = _matrix_collision_maximum_absolute(center_delta_);
      planar_minimum = _matrix_collision_absolute(center_delta_->x);
      alignment      = _matrix_collision_absolute(center_delta_->z);
      if(alignment < planar_minimum)
        planar_minimum = alignment;

      if((center_maximum < g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.center_distance_threshold) &&
         (g_MATRIX_COLLISION_RUNTIME_HEADER.distance_correction_disabled == 0))
        {
          use_distance_correction = 1;
        }
      else
        {
          _copy_matrix_collision_vector(&relative_velocity, &normalized_velocity);
          normalize_vector3_64_in_place(&normalized_velocity);
          _copy_matrix_collision_vector(center_delta_, &normalized_center);
          normalize_vector3_fixed8_in_place(&normalized_center);
          alignment = _matrix_collision_dot_fixed8((const int32 *) &normalized_velocity,
                                                   (const int32 *) &normalized_center);
          if((alignment > g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.alignment_threshold) &&
             (g_MATRIX_COLLISION_RUNTIME_HEADER.alignment_correction_disabled == 0))
            {
              use_distance_correction = 1;
            }
          else if((g_MATRIX_COLLISION_RUNTIME_HEADER.secondary_alignment_enabled != 0) &&
                  (alignment >
                   g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.secondary_alignment_threshold) &&
                  (planar_minimum <
                   g_MATRIX_COLLISION_RUNTIME_THRESHOLDS.planar_distance_threshold))
            {
              use_distance_correction = 1;
            }
        }

      if(use_distance_correction != 0)
        {
          reciprocal_distance = RecipUF16((ufrac16) calculate_vector3_magnitude_64(center_delta_));
        }
    }

  if((use_distance_correction == 0) &&
     (g_MATRIX_COLLISION_RUNTIME_HEADER.stationary_correction_enabled != 0) &&
     (g_MATRIX_COLLISION_RUNTIME_HEADER.repeated_correction_enabled == 0))
    {
      use_distance_correction = 1;
    }

  if(use_distance_correction != 0)
    {
      _calculate_velocity_distance_correction(&first_state_->velocity,
                                              reciprocal_distance,
                                              first_correction_);
      _calculate_velocity_distance_correction(&second_state_->velocity,
                                              reciprocal_distance,
                                              second_correction_);
    }
  else if(g_MATRIX_COLLISION_RUNTIME_HEADER.repeated_correction_enabled == 0)
    {
      _calculate_mass_weighted_corrections(first_state_,
                                           second_state_,
                                           center_delta_,
                                           first_correction_,
                                           second_correction_);
    }
  else
    {
      _copy_matrix_collision_vector(&first_state_->velocity, first_correction_);
      _copy_matrix_collision_vector(&second_state_->velocity, second_correction_);
      normalize_vector3_64_in_place(first_correction_);
      normalize_vector3_64_in_place(second_correction_);
      first_correction_->x >>= MATRIX_COLLISION_NORMALIZED_VELOCITY_SHIFT;
      first_correction_->y >>= MATRIX_COLLISION_NORMALIZED_VELOCITY_SHIFT;
      first_correction_->z >>= MATRIX_COLLISION_NORMALIZED_VELOCITY_SHIFT;
      second_correction_->x >>= MATRIX_COLLISION_NORMALIZED_VELOCITY_SHIFT;
      second_correction_->y >>= MATRIX_COLLISION_NORMALIZED_VELOCITY_SHIFT;
      second_correction_->z >>= MATRIX_COLLISION_NORMALIZED_VELOCITY_SHIFT;
    }
}

int32
resolve_standard_matrix_collision(MatrixControllerState *first_state_,
                                  MatrixControllerState *second_state_,
                                  Vector3               *first_contact_,
                                  Vector3               *second_contact_)
{
  int32   second_extent_projection[MATRIX_COLLISION_MATRIX_ELEMENT_COUNT];
  int32   first_extent_projection[MATRIX_COLLISION_MATRIX_ELEMENT_COUNT];
  int32   center_projection[MATRIX_COLLISION_CENTER_PROJECTION_COUNT];
  Vector3 center_delta;
  Vector3 first_correction;
  Vector3 second_correction;
  int32   correction_count;
  int32   repeat_count;
  int32   collision_resolved;

  _prepare_matrix_collision_extent_projections(first_state_,
                                               second_state_,
                                               second_extent_projection,
                                               first_extent_projection);
  correction_count   = 0;
  collision_resolved = 0;
  while(correction_count < MATRIX_COLLISION_MAXIMUM_CORRECTIONS)
    {
      _subtract_matrix_collision_vectors(&second_state_->position,
                                         &first_state_->position,
                                         &center_delta);
      _project_matrix_collision_center(first_state_, second_state_, center_projection);
      if(find_matrix_collision_contact(first_state_,
                                       second_state_,
                                       first_contact_,
                                       second_contact_,
                                       second_extent_projection,
                                       first_extent_projection,
                                       center_projection) == 0)
        return collision_resolved;

      if(collision_resolved == 0)
        {
          _calculate_standard_matrix_collision_corrections(first_state_,
                                                           second_state_,
                                                           &center_delta,
                                                           &first_correction,
                                                           &second_correction);
          collision_resolved = 1;
        }

      repeat_count = 1;
      if(g_MATRIX_COLLISION_RUNTIME_HEADER.repeated_correction_enabled != 0)
        repeat_count = (correction_count >> 1) + 1;
      while(repeat_count > 0)
        {
          _add_matrix_collision_position(&first_state_->position, &first_correction);
          _add_matrix_collision_position(&second_state_->position, &second_correction);
          repeat_count -= 1;
        }
      correction_count += 1;
    }

  return collision_resolved;
}

int32
resolve_matrix_controller_collision(MatrixControllerState *first_state_,
                                    MatrixControllerState *second_state_,
                                    Vector3               *first_contact_,
                                    Vector3               *second_contact_)
{
  if(g_MATRIX_COLLISION_ALTERNATE_MODE != 0)
    return resolve_alternate_matrix_collision(first_state_,
                                              second_state_,
                                              first_contact_,
                                              second_contact_);

  return resolve_standard_matrix_collision(first_state_,
                                           second_state_,
                                           first_contact_,
                                           second_contact_);
}

int32
apply_matrix_collision_impulse(MatrixControllerState *first_state_,
                               MatrixControllerState *second_state_,
                               const Vector3         *contact_point_,
                               const Vector3         *collision_normal_)
{
  Vector3 first_contact_offset;
  Vector3 second_contact_offset;
  Vector3 first_tangent;
  Vector3 second_tangent;
  Vector3 impulse_vector;
  Vector3 torque;
  int32   denominator;
  int32   impulse;
  int32   first_rotational_term;
  int32   second_rotational_term;
  uint32  relative_speed;
  uint32  denominator_sum;

  /* ARM60 vector subtraction wraps before the fixed-point folio operations. */
  first_contact_offset.x = (int32) ((uint32) contact_point_->x - (uint32) first_state_->position.x);
  first_contact_offset.y = (int32) ((uint32) contact_point_->y - (uint32) first_state_->position.y);
  first_contact_offset.z = (int32) ((uint32) contact_point_->z - (uint32) first_state_->position.z);
  second_contact_offset.x =
    (int32) ((uint32) contact_point_->x - (uint32) second_state_->position.x);
  second_contact_offset.y =
    (int32) ((uint32) contact_point_->y - (uint32) second_state_->position.y);
  second_contact_offset.z =
    (int32) ((uint32) contact_point_->z - (uint32) second_state_->position.z);

  Cross3_F16((frac16 *) &first_tangent,
             (frac16 *) &first_contact_offset,
             (frac16 *) collision_normal_);
  Cross3_F16((frac16 *) &second_tangent,
             (frac16 *) &second_contact_offset,
             (frac16 *) collision_normal_);
  relative_speed =
    (uint32) Dot3_F16((frac16 *) &second_state_->velocity, (frac16 *) collision_normal_);
  relative_speed -=
    (uint32) Dot3_F16((frac16 *) &first_state_->velocity, (frac16 *) collision_normal_);
  relative_speed -=
    (uint32) Dot3_F16((frac16 *) &first_state_->angular_velocity, (frac16 *) &first_tangent);
  relative_speed +=
    (uint32) Dot3_F16((frac16 *) &second_state_->angular_velocity, (frac16 *) &second_tangent);

  first_rotational_term  = MulSF16(Dot3_F16((frac16 *) &first_tangent, (frac16 *) &first_tangent),
                                   first_state_->second_reciprocal);
  second_rotational_term = MulSF16(Dot3_F16((frac16 *) &second_tangent, (frac16 *) &second_tangent),
                                   second_state_->second_reciprocal);
  denominator_sum        = (uint32) (first_state_->first_reciprocal >> 1);
  denominator_sum += (uint32) (second_state_->first_reciprocal >> 1);
  denominator_sum += (uint32) (first_rotational_term >> 1);
  denominator_sum += (uint32) (second_rotational_term >> 1);
  denominator = (int32) denominator_sum;
  impulse     = DivSF16((int32) relative_speed, denominator);
  if(impulse < 0)
    return 0;

  impulse = MulSF16(g_MATRIX_COLLISION_IMPULSE_SCALE, impulse);
  g_VEHICLE_COLLISION_SOUND_TIME = impulse;
  impulse_vector.x               = MulSF16(impulse, collision_normal_->x);
  impulse_vector.y               = MulSF16(impulse, collision_normal_->y);
  impulse_vector.z               = MulSF16(impulse, collision_normal_->z);

  first_state_->velocity.x =
    (int32) ((uint32) first_state_->velocity.x +
             (uint32) MulSF16(first_state_->first_reciprocal, impulse_vector.x));
  first_state_->velocity.y =
    (int32) ((uint32) first_state_->velocity.y +
             (uint32) MulSF16(first_state_->first_reciprocal, impulse_vector.y));
  first_state_->velocity.z =
    (int32) ((uint32) first_state_->velocity.z +
             (uint32) MulSF16(first_state_->first_reciprocal, impulse_vector.z));
  second_state_->velocity.x =
    (int32) ((uint32) second_state_->velocity.x -
             (uint32) MulSF16(second_state_->first_reciprocal, impulse_vector.x));
  second_state_->velocity.y =
    (int32) ((uint32) second_state_->velocity.y -
             (uint32) MulSF16(second_state_->first_reciprocal, impulse_vector.y));
  second_state_->velocity.z =
    (int32) ((uint32) second_state_->velocity.z -
             (uint32) MulSF16(second_state_->first_reciprocal, impulse_vector.z));

  Cross3_F16((frac16 *) &torque, (frac16 *) &first_contact_offset, (frac16 *) &impulse_vector);
  first_state_->angular_velocity.x =
    (int32) ((uint32) first_state_->angular_velocity.x +
             (uint32) MulSF16(first_state_->second_reciprocal, torque.x));
  first_state_->angular_velocity.y =
    (int32) ((uint32) first_state_->angular_velocity.y +
             (uint32) MulSF16(first_state_->second_reciprocal, torque.y));
  first_state_->angular_velocity.z =
    (int32) ((uint32) first_state_->angular_velocity.z +
             (uint32) MulSF16(first_state_->second_reciprocal, torque.z));

  Cross3_F16((frac16 *) &torque, (frac16 *) &second_contact_offset, (frac16 *) &impulse_vector);
  second_state_->angular_velocity.x =
    (int32) ((uint32) second_state_->angular_velocity.x -
             (uint32) MulSF16(second_state_->second_reciprocal, torque.x));
  second_state_->angular_velocity.y =
    (int32) ((uint32) second_state_->angular_velocity.y -
             (uint32) MulSF16(second_state_->second_reciprocal, torque.y));
  second_state_->angular_velocity.z =
    (int32) ((uint32) second_state_->angular_velocity.z -
             (uint32) MulSF16(second_state_->second_reciprocal, torque.z));

  return 1;
}
