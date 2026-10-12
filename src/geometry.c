#include "geometry.h"

#include "fixed_math.h"
#include "trig_lookup.h"

#include "operamath.h"

#define INTEGER_SQUARE_ROOT_INITIAL_ESTIMATE 48
#define INTEGER_SQUARE_ROOT_INITIAL_ODD 1
#define INTEGER_SQUARE_ROOT_ITERATIONS 5
#define INTEGER_SQUARE_ROOT_LARGE_SHIFT 8
#define INTEGER_SQUARE_ROOT_NEXT_ODD 2
#define INTEGER_SQUARE_ROOT_SIGN_BIT 0x80000000
#define INTEGER_SQUARE_ROOT_SMALL_LIMIT 400
#define INTEGER_SQUARE_ROOT_STEP_SHIFT 1
#define MATRIX_3X3_ACCUMULATOR_SHIFT 14
#define MATRIX_3X3_DIMENSION 3
#define MATRIX_3X3_ELEMENT_COUNT 9
#define MATRIX_3X3_OPERAND_SHIFT 2
#define MATRIX_3X3_SECOND_ROW_OFFSET MATRIX_3X3_DIMENSION
#define MATRIX_3X3_THIRD_ROW_OFFSET (MATRIX_3X3_DIMENSION * 2)
#define MATRIX_ORTHOGONALITY_ITERATION_COUNT 4
#define MATRIX_ORTHOGONALITY_COEFFICIENT_COUNT 10
#define MATRIX_ORTHOGONALITY_COEFFICIENT_0 0x00004000
#define MATRIX_ORTHOGONALITY_COEFFICIENT_1 (-0x00002000)
#define MATRIX_ORTHOGONALITY_COEFFICIENT_2 0x00001800
#define MATRIX_ORTHOGONALITY_COEFFICIENT_3 (-0x00001400)
#define MATRIX_ORTHOGONALITY_COEFFICIENT_4 0x00001180
#define MATRIX_ORTHOGONALITY_COEFFICIENT_5 (-0x00000FC0)
#define MATRIX_ORTHOGONALITY_COEFFICIENT_6 0x00000E70
#define MATRIX_ORTHOGONALITY_COEFFICIENT_7 (-0x00000D68)
#define MATRIX_ORTHOGONALITY_COEFFICIENT_8 0x00000C92
#define MATRIX_ORTHOGONALITY_COEFFICIENT_9 (-0x00000BDF)
#define MATRIX_ZERO 0
#define MATRIX_FIXED_FRACTION_BITS 16
#define MATRIX_NORMALIZATION_HALF_SHIFT 1
#define MATRIX_NORMALIZATION_OPERAND_SHIFT 2
#define MATRIX_NORMALIZATION_PRODUCT_SHIFT 14
#define MATRIX_NORMALIZATION_SUM_BIAS 0x00010000
#define FIXED8_FRACTION_BITS 8
#define FIXED16_ONE 0x00010000
#define APPROXIMATE_DISTANCE_MINOR_SHIFT 2
#define APPROXIMATE_DISTANCE_SCALE_SHIFT 1
#define MATRIX_CONTROLLER_TIME_SCALE 0x00000888
#define MATRIX_CONTROLLER_VELOCITY_DAMPING_SHIFT 6
#define MATRIX_CONTROLLER_VERTICAL_ACCELERATION (-0x000053B6)
#define MATRIX_CONTROLLER_ANGLE_UNITS_PER_RADIAN 0x0028BE63
#define NORMALIZED_VECTOR_LARGE_LIMIT 0x00800000
#define NORMALIZED_VECTOR_LARGE_SHIFT 9
#define NORMALIZED_VECTOR_MEDIUM_LIMIT 0x00008000
#define NORMALIZED_VECTOR_MEDIUM_SHIFT 8
#define NORMALIZED_VECTOR_OUTPUT_SHIFT 12
#define NORMALIZED_VECTOR_RECIPROCAL 0x10000000
#define NORMALIZED_VECTOR_SMALL_LIMIT 0x00000100
#define NORMALIZED_VECTOR_SMALL_SHIFT 4
#define TRIANGLE_SECOND_INDEX 2
#define UNSIGNED_WORD_BIT_COUNT 32
#define UNSIGNED_WORD_HIGH_BIT_SHIFT 31
#define VECTOR_MATRIX_BULK_THRESHOLD 4
#define VECTOR_PROJECTION_DOT_LIMIT 0x0001FFFF
#define VECTOR_Z_INDEX 2
#define PROJECTED_POINT_COORDINATE_MASK 0xFFFFF000
#define REFERENCE_DISTANCE_SPAN 0x60000
#define LOOKUP_ROTATION_COMPONENT_A(angle_) CosF16((frac16) (angle_))
#define LOOKUP_ROTATION_COMPONENT_B(angle_) SinF16((frac16) (angle_))

#define MATRIX_CONTROLLER_VECTOR_COMPONENT_COUNT 9
#define MATRIX_CONTROLLER_ZERO_FIELD_COUNT 15

CoordinateMask2          g_PROJECTED_POINT_COORDINATE_MASKS = { PROJECTED_POINT_COORDINATE_MASK,
                                                                PROJECTED_POINT_COORDINATE_MASK };
ReferenceDistanceRecord *g_REFERENCE_DISTANCE_RECORDS;
Matrix3x3                g_MATRI_X3_X3_IDENTITY = { FIXED16_ONE, MATRIX_ZERO, MATRIX_ZERO,
                                                    MATRIX_ZERO, FIXED16_ONE, MATRIX_ZERO,
                                                    MATRIX_ZERO, MATRIX_ZERO, FIXED16_ONE };
int32                    g_MATRI_X3_X3_IDENTITY_PADDING[3];
int32 g_MATRI_X3_X3_ORTHOGONALITY_COEFFICIENTS[MATRIX_ORTHOGONALITY_COEFFICIENT_COUNT] = {
  MATRIX_ORTHOGONALITY_COEFFICIENT_0, MATRIX_ORTHOGONALITY_COEFFICIENT_1,
  MATRIX_ORTHOGONALITY_COEFFICIENT_2, MATRIX_ORTHOGONALITY_COEFFICIENT_3,
  MATRIX_ORTHOGONALITY_COEFFICIENT_4, MATRIX_ORTHOGONALITY_COEFFICIENT_5,
  MATRIX_ORTHOGONALITY_COEFFICIENT_6, MATRIX_ORTHOGONALITY_COEFFICIENT_7,
  MATRIX_ORTHOGONALITY_COEFFICIENT_8, MATRIX_ORTHOGONALITY_COEFFICIENT_9
};

static void
_multiply_matrix3x3_in_place(Matrix3x3 *matrix_, const Matrix3x3 *right_)
{
  Matrix3x3 product;

  multiply_matrix_3x3(matrix_, right_, &product);
  matrix_->m00 = product.m00;
  matrix_->m01 = product.m01;
  matrix_->m02 = product.m02;
  matrix_->m10 = product.m10;
  matrix_->m11 = product.m11;
  matrix_->m12 = product.m12;
  matrix_->m20 = product.m20;
  matrix_->m21 = product.m21;
  matrix_->m22 = product.m22;
}

int32
calculate_vector3_planar_distance(const Vector3 *left_, const Vector3 *right_)
{
  ufrac16 squared_distance;

  squared_distance = SquareSF16(left_->x - right_->x);
  squared_distance += SquareSF16(left_->z - right_->z);
  return (int32) SqrtF16(squared_distance);
}

int32
calculate_consecutive_reference_distance_remaining(int32 record_index_)
{
  const ReferenceDistanceRecord *record;
  Vector3                        difference;

  record       = &g_REFERENCE_DISTANCE_RECORDS[record_index_];
  difference.x = record[1].position.x - record[0].position.x;
  difference.y = record[1].position.y - record[0].position.y;
  difference.z = record[1].position.z - record[0].position.z;

  return REFERENCE_DISTANCE_SPAN - calculate_vector_magnitude(&difference);
}

static void
_multiply_matrix3x3_fixed16(Matrix3x3       *destination_,
                            const Matrix3x3 *left_,
                            const Matrix3x3 *right_)
{
  MulMat33Mat33_F16((frac16(*)[MATRIX_3X3_DIMENSION]) destination_,
                    (frac16(*)[MATRIX_3X3_DIMENSION]) left_,
                    (frac16(*)[MATRIX_3X3_DIMENSION]) right_);
}

static void
_multiply_vector3_matrix3x3_fixed16(Vector3         *destination_,
                                    const Vector3   *vector_,
                                    const Matrix3x3 *matrix_)
{
  MulVec3Mat33_F16((frac16 *) destination_,
                   (frac16 *) vector_,
                   (frac16(*)[MATRIX_3X3_DIMENSION]) matrix_);
}

static int32
_multiply_matrix_normalization_components(int32 left_, int32 right_)
{
  uint32 product;

  product = (uint32) (left_ >> MATRIX_NORMALIZATION_OPERAND_SHIFT) * (uint32) right_;
  return (int32) product >> MATRIX_NORMALIZATION_PRODUCT_SHIFT;
}

static void
_normalize_matrix3x3_row(Vector3 *row_)
{
  uint32  squared_magnitude;
  ufrac16 scale;

  squared_magnitude = (uint32) _multiply_matrix_normalization_components(row_->x, row_->x);
  squared_magnitude += (uint32) _multiply_matrix_normalization_components(row_->y, row_->y);
  squared_magnitude += (uint32) _multiply_matrix_normalization_components(row_->z, row_->z);
  scale   = RecipUF16((ufrac16) ((int32) (squared_magnitude + MATRIX_NORMALIZATION_SUM_BIAS) >>
                                 MATRIX_NORMALIZATION_HALF_SHIFT));
  row_->x = _multiply_matrix_normalization_components(row_->x, (int32) scale);
  row_->y = _multiply_matrix_normalization_components(row_->y, (int32) scale);
  row_->z = _multiply_matrix_normalization_components(row_->z, (int32) scale);
}

static void
_cross_matrix3x3_rows(const Vector3 *left_, const Vector3 *right_, Vector3 *destination_)
{
  destination_->x =
    (int32) ((uint32) _multiply_matrix_normalization_components(left_->y, right_->z) -
             (uint32) _multiply_matrix_normalization_components(left_->z, right_->y));
  destination_->y =
    (int32) ((uint32) _multiply_matrix_normalization_components(left_->z, right_->x) -
             (uint32) _multiply_matrix_normalization_components(left_->x, right_->z));
  destination_->z =
    (int32) ((uint32) _multiply_matrix_normalization_components(left_->x, right_->y) -
             (uint32) _multiply_matrix_normalization_components(left_->y, right_->x));
}

void
initialize_matrix3x3_identity(Matrix3x3 *matrix_)
{
  matrix_->m00 = FIXED16_ONE;
  matrix_->m01 = 0;
  matrix_->m02 = 0;
  matrix_->m10 = 0;
  matrix_->m11 = FIXED16_ONE;
  matrix_->m12 = 0;
  matrix_->m20 = 0;
  matrix_->m21 = 0;
  matrix_->m22 = FIXED16_ONE;
}

void
rebuild_matrix3x3_basis_from_normal(Matrix3x3 *matrix_, const Vector3 *normal_)
{
  Vector3 *first_row;
  Vector3 *second_row;
  Vector3 *third_row;

  first_row    = (Vector3 *) matrix_;
  second_row   = first_row + 1;
  third_row    = second_row + 1;
  *second_row  = *normal_;
  first_row->x = (third_row->z >> FIXED8_FRACTION_BITS) * (second_row->y >> FIXED8_FRACTION_BITS) -
                 (third_row->y >> FIXED8_FRACTION_BITS) * (second_row->z >> FIXED8_FRACTION_BITS);
  first_row->y = (third_row->x >> FIXED8_FRACTION_BITS) * (second_row->z >> FIXED8_FRACTION_BITS) -
                 (third_row->z >> FIXED8_FRACTION_BITS) * (second_row->x >> FIXED8_FRACTION_BITS);
  first_row->z = (third_row->y >> FIXED8_FRACTION_BITS) * (second_row->x >> FIXED8_FRACTION_BITS) -
                 (third_row->x >> FIXED8_FRACTION_BITS) * (second_row->y >> FIXED8_FRACTION_BITS);
  normalize_vector3_fixed8_in_place(first_row);
  third_row->x = (second_row->z >> FIXED8_FRACTION_BITS) * (first_row->y >> FIXED8_FRACTION_BITS) -
                 (second_row->y >> FIXED8_FRACTION_BITS) * (first_row->z >> FIXED8_FRACTION_BITS);
  third_row->y = (second_row->x >> FIXED8_FRACTION_BITS) * (first_row->z >> FIXED8_FRACTION_BITS) -
                 (second_row->z >> FIXED8_FRACTION_BITS) * (first_row->x >> FIXED8_FRACTION_BITS);
  third_row->z = (second_row->y >> FIXED8_FRACTION_BITS) * (first_row->x >> FIXED8_FRACTION_BITS) -
                 (second_row->x >> FIXED8_FRACTION_BITS) * (first_row->y >> FIXED8_FRACTION_BITS);
}

void
initialize_matrix3x3_rotation_z(Matrix3x3 *matrix_, int32 angle_)
{
  int32 component_a;
  int32 component_b;

  component_a  = LOOKUP_ROTATION_COMPONENT_A(angle_);
  component_b  = LOOKUP_ROTATION_COMPONENT_B(angle_);
  matrix_->m00 = component_a;
  matrix_->m01 = component_b;
  matrix_->m02 = 0;
  matrix_->m10 = -component_b;
  matrix_->m11 = component_a;
  matrix_->m12 = 0;
  matrix_->m20 = 0;
  matrix_->m21 = 0;
  matrix_->m22 = FIXED16_ONE;
}

void
initialize_matrix3x3_rotation_y(Matrix3x3 *matrix_, int32 angle_)
{
  int32 component_a;
  int32 component_b;

  component_a  = LOOKUP_ROTATION_COMPONENT_A(angle_);
  component_b  = LOOKUP_ROTATION_COMPONENT_B(angle_);
  matrix_->m00 = component_a;
  matrix_->m01 = 0;
  matrix_->m02 = -component_b;
  matrix_->m10 = 0;
  matrix_->m11 = FIXED16_ONE;
  matrix_->m12 = 0;
  matrix_->m20 = component_b;
  matrix_->m21 = 0;
  matrix_->m22 = component_a;
}

void
initialize_matrix3x3_rotation_x(Matrix3x3 *matrix_, int32 angle_)
{
  int32 component_a;
  int32 component_b;

  component_a  = LOOKUP_ROTATION_COMPONENT_A(angle_);
  component_b  = LOOKUP_ROTATION_COMPONENT_B(angle_);
  matrix_->m00 = FIXED16_ONE;
  matrix_->m01 = 0;
  matrix_->m02 = 0;
  matrix_->m10 = 0;
  matrix_->m11 = component_a;
  matrix_->m12 = component_b;
  matrix_->m20 = 0;
  matrix_->m21 = -component_b;
  matrix_->m22 = component_a;
}

void
initialize_matrix3x3_rotation_x_1024(Matrix3x3 *matrix_, int32 angle_)
{
  int32 cosine;
  int32 sine;

  calculate_sine_cosine_1024(angle_, &sine, &cosine);
  matrix_->m00 = FIXED16_ONE;
  matrix_->m01 = 0;
  matrix_->m02 = 0;
  matrix_->m10 = 0;
  matrix_->m11 = cosine;
  matrix_->m12 = sine;
  matrix_->m20 = 0;
  matrix_->m21 = -sine;
  matrix_->m22 = cosine;
}

void
initialize_matrix3x3_rotation_y_1024(Matrix3x3 *matrix_, int32 angle_)
{
  int32 cosine;
  int32 sine;

  calculate_sine_cosine_1024(angle_, &sine, &cosine);
  matrix_->m00 = cosine;
  matrix_->m01 = 0;
  matrix_->m02 = -sine;
  matrix_->m10 = 0;
  matrix_->m11 = FIXED16_ONE;
  matrix_->m12 = 0;
  matrix_->m20 = sine;
  matrix_->m21 = 0;
  matrix_->m22 = cosine;
}

void
initialize_matrix3x3_rotation_z_1024(Matrix3x3 *matrix_, int32 angle_)
{
  int32 cosine;
  int32 sine;

  calculate_sine_cosine_1024(angle_, &sine, &cosine);
  matrix_->m00 = cosine;
  matrix_->m01 = sine;
  matrix_->m02 = 0;
  matrix_->m10 = -sine;
  matrix_->m11 = cosine;
  matrix_->m12 = 0;
  matrix_->m20 = 0;
  matrix_->m21 = 0;
  matrix_->m22 = FIXED16_ONE;
}

void
initialize_matrix3x3_inverse_rotation_x(Matrix3x3 *matrix_, int32 angle_)
{
  int32 component_a;
  int32 component_b;

  component_a  = LOOKUP_ROTATION_COMPONENT_A(angle_);
  component_b  = LOOKUP_ROTATION_COMPONENT_B(angle_);
  matrix_->m00 = FIXED16_ONE;
  matrix_->m01 = 0;
  matrix_->m02 = 0;
  matrix_->m10 = 0;
  matrix_->m11 = component_a;
  matrix_->m12 = -component_b;
  matrix_->m20 = 0;
  matrix_->m21 = component_b;
  matrix_->m22 = component_a;
}

void
initialize_matrix3x3_inverse_rotation_y(Matrix3x3 *matrix_, int32 angle_)
{
  int32 component_a;
  int32 component_b;

  component_a  = LOOKUP_ROTATION_COMPONENT_A(angle_);
  component_b  = LOOKUP_ROTATION_COMPONENT_B(angle_);
  matrix_->m00 = component_a;
  matrix_->m01 = 0;
  matrix_->m02 = component_b;
  matrix_->m10 = 0;
  matrix_->m11 = FIXED16_ONE;
  matrix_->m12 = 0;
  matrix_->m20 = -component_b;
  matrix_->m21 = 0;
  matrix_->m22 = component_a;
}

void
initialize_matrix3x3_inverse_rotation_z(Matrix3x3 *matrix_, int32 angle_)
{
  int32 component_a;
  int32 component_b;

  component_a  = LOOKUP_ROTATION_COMPONENT_A(angle_);
  component_b  = LOOKUP_ROTATION_COMPONENT_B(angle_);
  matrix_->m00 = component_a;
  matrix_->m01 = -component_b;
  matrix_->m02 = 0;
  matrix_->m10 = component_b;
  matrix_->m11 = component_a;
  matrix_->m12 = 0;
  matrix_->m20 = 0;
  matrix_->m21 = 0;
  matrix_->m22 = FIXED16_ONE;
}

void
initialize_matrix3x3_axis_rotation(Matrix3x3 *matrix_, int32 angle_, const Vector3 *axis_)
{
  Matrix3x3 axis_alignment;
  Matrix3x3 inverse_axis_alignment;
  Matrix3x3 rotation;
  Vector3   axis_projection;
  int32     horizontal_angle;
  int32     vertical_angle;

  horizontal_angle = Atan2F16(axis_->x, axis_->z);
  initialize_matrix3x3_rotation_y(&axis_alignment, horizontal_angle);

  axis_projection.x = axis_->x;
  axis_projection.y = 0;
  axis_projection.z = axis_->z;
  vertical_angle    = -Atan2F16((int32) calculate_vector3_magnitude_64(&axis_projection), axis_->y);
  initialize_matrix3x3_rotation_z(&rotation, vertical_angle);
  _multiply_matrix3x3_fixed16(&axis_alignment, &axis_alignment, &rotation);

  initialize_matrix3x3_rotation_x(&rotation, angle_);
  transpose_matrix_3x3(&axis_alignment, &inverse_axis_alignment);
  _multiply_matrix3x3_fixed16(matrix_, &axis_alignment, &rotation);
  _multiply_matrix3x3_fixed16(matrix_, matrix_, &inverse_axis_alignment);
}

void
refine_matrix3x3_orthogonality(Matrix3x3 *matrix_)
{
  Matrix3x3 transposed;
  Matrix3x3 gram_matrix;
  Matrix3x3 error;
  Matrix3x3 power;
  Matrix3x3 next_power;
  Matrix3x3 correction;
  Matrix3x3 scaled_power;
  Matrix3x3 original;
  int32     iteration;
  int32     coefficient_index;

  for(iteration = 0; iteration < MATRIX_ORTHOGONALITY_ITERATION_COUNT; iteration++)
    {
      transpose_matrix_3x3(matrix_, &transposed);
      multiply_matrix_3x3(&transposed, matrix_, &gram_matrix);
      subtract_matrix_3x3(&gram_matrix, &g_MATRI_X3_X3_IDENTITY, &error);
      power      = g_MATRI_X3_X3_IDENTITY;
      correction = g_MATRI_X3_X3_IDENTITY;

      for(coefficient_index = 1; coefficient_index < MATRIX_ORTHOGONALITY_COEFFICIENT_COUNT;
          coefficient_index++)
        {
          multiply_matrix_3x3(&power, &error, &next_power);
          power = next_power;
          scale_matrix_3x3(&power,
                           g_MATRI_X3_X3_ORTHOGONALITY_COEFFICIENTS[coefficient_index],
                           &scaled_power);
          add_matrix_3x3(&correction, &scaled_power, &correction);
        }

      original = *matrix_;
      multiply_matrix_3x3(&original, &correction, matrix_);
    }
}

Matrix3x3 *
initialize_matrix3x3_axis_rotation_1024(const Vector3 *axis_, int32 angle_, Matrix3x3 *matrix_)
{
  Vector3 normalized_axis;
  int32   cosine;
  int32   sine;
  int32   inverse_cosine;
  int32   scaled_x;
  int32   scaled_y;
  int32   scaled_z;
  int32   sine_x;
  int32   sine_y;
  int32   sine_z;

  cosine = cosine_1024(angle_);
  sine   = sine_1024(angle_);
  if(angle_ == 0)
    {
      *matrix_ = g_MATRI_X3_X3_IDENTITY;
      return matrix_;
    }

  normalize_vector(axis_, &normalized_axis);
  inverse_cosine = FIXED16_ONE - cosine;
  scaled_x       = multiply_fixed16(inverse_cosine, normalized_axis.x);
  scaled_y       = multiply_fixed16(inverse_cosine, normalized_axis.y);
  scaled_z       = multiply_fixed16(inverse_cosine, normalized_axis.z);
  sine_x         = multiply_fixed16(sine, normalized_axis.x);
  sine_y         = multiply_fixed16(sine, normalized_axis.y);
  sine_z         = multiply_fixed16(sine, normalized_axis.z);

  matrix_->m00 = multiply_fixed16(scaled_x, normalized_axis.x) + cosine;
  matrix_->m01 = multiply_fixed16(scaled_x, normalized_axis.y) + sine_z;
  matrix_->m02 = multiply_fixed16(scaled_x, normalized_axis.z) - sine_y;
  matrix_->m10 = multiply_fixed16(scaled_y, normalized_axis.x) - sine_z;
  matrix_->m11 = multiply_fixed16(scaled_y, normalized_axis.y) + cosine;
  matrix_->m12 = multiply_fixed16(scaled_y, normalized_axis.z) + sine_x;
  matrix_->m20 = multiply_fixed16(scaled_z, normalized_axis.x) + sine_y;
  matrix_->m21 = multiply_fixed16(scaled_z, normalized_axis.y) - sine_x;
  matrix_->m22 = multiply_fixed16(scaled_z, normalized_axis.z) + cosine;

  return matrix_;
}

void
extract_matrix3x3_rotation_angles(const Matrix3x3 *matrix_,
                                  int32           *x_angle_,
                                  int32           *y_angle_,
                                  int32           *z_angle_)
{
  Matrix3x3 axis_alignment;
  Matrix3x3 rotation;
  Vector3   first_row;
  Vector3   transformed_row;
  int32     horizontal_magnitude;
  int32     y_angle;
  int32     z_angle;

  z_angle              = Atan2F16(matrix_->m22, matrix_->m20);
  horizontal_magnitude = (int32) SqrtF16(
    (uint32) (MulSF16(matrix_->m22, matrix_->m22) + MulSF16(matrix_->m20, matrix_->m20)));
  y_angle = Atan2F16(horizontal_magnitude, -matrix_->m21);

  initialize_matrix3x3_rotation_y(&axis_alignment, -z_angle);
  initialize_matrix3x3_rotation_x(&rotation, -y_angle);
  _multiply_matrix3x3_fixed16(&axis_alignment, &axis_alignment, &rotation);

  first_row.x = matrix_->m00;
  first_row.y = matrix_->m01;
  first_row.z = matrix_->m02;
  _multiply_vector3_matrix3x3_fixed16(&transformed_row, &first_row, &axis_alignment);
  *x_angle_ = Atan2F16(transformed_row.x, transformed_row.y);
  *y_angle_ = y_angle;
  *z_angle_ = z_angle;
}

void
extract_matrix3x3_rotation_yzx_angles(const Matrix3x3 *matrix_,
                                      int32           *x_angle_,
                                      int32           *y_angle_,
                                      int32           *z_angle_)
{
  Matrix3x3 axis_alignment;
  Matrix3x3 rotation;
  Vector3   last_row;
  Vector3   transformed_row;
  int32     horizontal_magnitude;
  int32     y_angle;
  int32     z_angle;

  z_angle              = Atan2F16(matrix_->m00, -matrix_->m02);
  horizontal_magnitude = (int32) SqrtF16(
    (uint32) (MulSF16(matrix_->m00, matrix_->m00) + MulSF16(matrix_->m02, matrix_->m02)));
  y_angle = Atan2F16(horizontal_magnitude, matrix_->m01);

  initialize_matrix3x3_rotation_y(&axis_alignment, -z_angle);
  initialize_matrix3x3_rotation_z(&rotation, -y_angle);
  _multiply_matrix3x3_fixed16(&axis_alignment, &axis_alignment, &rotation);

  last_row.x = matrix_->m20;
  last_row.y = matrix_->m21;
  last_row.z = matrix_->m22;
  _multiply_vector3_matrix3x3_fixed16(&transformed_row, &last_row, &axis_alignment);
  *x_angle_ = Atan2F16(transformed_row.z, -transformed_row.y);
  *y_angle_ = y_angle;
  *z_angle_ = z_angle;
}

void
initialize_matrix3x3_rotation_zxy(Matrix3x3 *matrix_,
                                  int32      z_angle_,
                                  int32      x_angle_,
                                  int32      y_angle_)
{
  Matrix3x3 rotation;

  initialize_matrix3x3_rotation_z(matrix_, z_angle_);
  initialize_matrix3x3_rotation_x(&rotation, x_angle_);
  _multiply_matrix3x3_in_place(matrix_, &rotation);
  initialize_matrix3x3_rotation_y(&rotation, y_angle_);
  _multiply_matrix3x3_in_place(matrix_, &rotation);
}

void
initialize_matrix3x3_rotation_yxz(Matrix3x3 *matrix_,
                                  int32      y_angle_,
                                  int32      x_angle_,
                                  int32      z_angle_)
{
  Matrix3x3 rotation;

  initialize_matrix3x3_rotation_y(matrix_, y_angle_);
  initialize_matrix3x3_rotation_x(&rotation, x_angle_);
  _multiply_matrix3x3_in_place(matrix_, &rotation);
  initialize_matrix3x3_rotation_z(&rotation, z_angle_);
  _multiply_matrix3x3_in_place(matrix_, &rotation);
}

void
initialize_matrix3x3_rotation_xzy(Matrix3x3 *matrix_,
                                  int32      x_angle_,
                                  int32      z_angle_,
                                  int32      y_angle_)
{
  Matrix3x3 rotation;

  initialize_matrix3x3_rotation_x(matrix_, x_angle_);
  initialize_matrix3x3_rotation_z(&rotation, z_angle_);
  _multiply_matrix3x3_in_place(matrix_, &rotation);
  initialize_matrix3x3_rotation_y(&rotation, y_angle_);
  _multiply_matrix3x3_in_place(matrix_, &rotation);
}

void
initialize_matrix_controller_state(int32                  first_value_,
                                   int32                  second_value_,
                                   void                  *context_,
                                   MatrixControllerState *state_)
{
  int32 index;

  (void) context_;
  state_->first_value       = first_value_;
  state_->first_reciprocal  = RecipSF16(first_value_);
  state_->second_value      = second_value_;
  state_->second_reciprocal = RecipSF16(second_value_);
  initialize_matrix3x3_identity(&state_->matrix);

  state_->position.x         = 0;
  state_->position.y         = 0;
  state_->position.z         = 0;
  state_->velocity.x         = 0;
  state_->velocity.y         = 0;
  state_->velocity.z         = 0;
  state_->angular_velocity.x = 0;
  state_->angular_velocity.y = 0;
  state_->angular_velocity.z = 0;
  for(index = 0;
      index < MATRIX_CONTROLLER_ZERO_FIELD_COUNT - MATRIX_CONTROLLER_VECTOR_COMPONENT_COUNT;
      index++)
    {
      state_->fields_48[index] = 0;
    }
  state_->stability_measure = 0;
}

void
advance_matrix_controller_state(MatrixControllerState *state_)
{
  Matrix3x3 rotation;
  int32     rotation_angle;

  state_->position.x += MulSF16(MATRIX_CONTROLLER_TIME_SCALE, state_->velocity.x);
  state_->position.y += MulSF16(MATRIX_CONTROLLER_TIME_SCALE, state_->velocity.y);
  state_->position.z += MulSF16(MATRIX_CONTROLLER_TIME_SCALE, state_->velocity.z);
  state_->velocity.x -= state_->velocity.x >> MATRIX_CONTROLLER_VELOCITY_DAMPING_SHIFT;
  state_->velocity.y -= state_->velocity.y >> MATRIX_CONTROLLER_VELOCITY_DAMPING_SHIFT;
  state_->velocity.z -= state_->velocity.z >> MATRIX_CONTROLLER_VELOCITY_DAMPING_SHIFT;
  state_->velocity.y += MATRIX_CONTROLLER_VERTICAL_ACCELERATION;

  rotation_angle =
    MulSF16(MulSF16(MATRIX_CONTROLLER_TIME_SCALE,
                    (int32) calculate_vector3_magnitude_64(&state_->angular_velocity)),
            MATRIX_CONTROLLER_ANGLE_UNITS_PER_RADIAN);
  initialize_matrix3x3_axis_rotation(&rotation, rotation_angle, &state_->angular_velocity);
  _multiply_matrix3x3_fixed16(&state_->matrix, &state_->matrix, &rotation);
}

int32
integer_square_root(int32 value_)
{
  uint32 estimate;
  uint32 iteration;
  uint32 odd;
  uint32 remainder;
  uint32 value;

  if(value_ <= INTEGER_SQUARE_ROOT_SMALL_LIMIT)
    {
      odd       = INTEGER_SQUARE_ROOT_INITIAL_ODD;
      remainder = (uint32) value_;
      do
        {
          remainder -= odd;
          odd += INTEGER_SQUARE_ROOT_NEXT_ODD;
        }
      while((remainder & INTEGER_SQUARE_ROOT_SIGN_BIT) == 0);

      return (int32) ((odd >> INTEGER_SQUARE_ROOT_STEP_SHIFT) - 1);
    }

  value    = (uint32) value_;
  estimate = INTEGER_SQUARE_ROOT_INITIAL_ESTIMATE + (value >> INTEGER_SQUARE_ROOT_LARGE_SHIFT);
  for(iteration = 0; iteration < INTEGER_SQUARE_ROOT_ITERATIONS; ++iteration)
    {
      estimate = ((value / estimate) + estimate) >> INTEGER_SQUARE_ROOT_STEP_SHIFT;
    }

  return (int32) estimate;
}

int32
calculate_planar_magnitude(int32 x_, int32 y_)
{
  uint32 squared_magnitude;

  squared_magnitude = ((uint32) x_ * (uint32) x_) + ((uint32) y_ * (uint32) y_);

  return integer_square_root((int32) squared_magnitude);
}

uint32
divide_unsigned_doubleword_by_word(uint32 low_word_, uint32 high_word_, uint32 divisor_)
{
  uint32 bit_index;
  uint32 carry;
  uint32 quotient;
  uint32 remainder;

  remainder = (high_word_ % divisor_);
  quotient  = 0;
  for(bit_index = 0; bit_index < UNSIGNED_WORD_BIT_COUNT; bit_index++)
    {
      carry     = (remainder >> UNSIGNED_WORD_HIGH_BIT_SHIFT);
      remainder = ((remainder << 1) | (low_word_ >> UNSIGNED_WORD_HIGH_BIT_SHIFT));
      low_word_ = (low_word_ << 1);
      quotient  = (quotient << 1);
      if((carry != 0) || (remainder >= divisor_))
        {
          remainder = (remainder - divisor_);
          quotient |= 1;
        }
    }

  return quotient;
}

int32
calculate_vector_magnitude(const Vector3 *vector_)
{
  int32 planar_magnitude;

  planar_magnitude = calculate_planar_magnitude(vector_->x, vector_->y);
  return calculate_planar_magnitude(planar_magnitude, vector_->z);
}

int32
normalize_vector(const Vector3 *source_, Vector3 *destination_)
{
  int32  magnitude;
  int32  measure;
  int32  reciprocal;
  int32  x;
  int32  y;
  int32  z;
  uint32 x_magnitude;
  uint32 y_magnitude;
  uint32 z_magnitude;

  x           = source_->x;
  y           = source_->y;
  z           = source_->z;
  x_magnitude = (x < 0) ? 0U - (uint32) x : (uint32) x;
  y_magnitude = (y < 0) ? 0U - (uint32) y : (uint32) y;
  z_magnitude = (z < 0) ? 0U - (uint32) z : (uint32) z;
  measure     = (int32) (x_magnitude + y_magnitude + z_magnitude);

  if(measure < NORMALIZED_VECTOR_SMALL_LIMIT)
    {
      x = (int32) ((uint32) x << NORMALIZED_VECTOR_SMALL_SHIFT);
      y = (int32) ((uint32) y << NORMALIZED_VECTOR_SMALL_SHIFT);
      z = (int32) ((uint32) z << NORMALIZED_VECTOR_SMALL_SHIFT);
    }
  if(measure >= NORMALIZED_VECTOR_MEDIUM_LIMIT)
    {
      x >>= NORMALIZED_VECTOR_MEDIUM_SHIFT;
      y >>= NORMALIZED_VECTOR_MEDIUM_SHIFT;
      z >>= NORMALIZED_VECTOR_MEDIUM_SHIFT;
    }
  if(measure >= NORMALIZED_VECTOR_LARGE_LIMIT)
    {
      x >>= NORMALIZED_VECTOR_LARGE_SHIFT;
      y >>= NORMALIZED_VECTOR_LARGE_SHIFT;
      z >>= NORMALIZED_VECTOR_LARGE_SHIFT;
    }

  destination_->x = x;
  destination_->y = y;
  destination_->z = z;
  magnitude       = calculate_vector_magnitude(destination_);
  if(magnitude == 0)
    reciprocal = -1;
  else
    reciprocal = NORMALIZED_VECTOR_RECIPROCAL / magnitude;

  destination_->x = ((int32) ((uint32) reciprocal * (uint32) x)) >> NORMALIZED_VECTOR_OUTPUT_SHIFT;
  destination_->y = ((int32) ((uint32) reciprocal * (uint32) y)) >> NORMALIZED_VECTOR_OUTPUT_SHIFT;
  destination_->z = ((int32) ((uint32) reciprocal * (uint32) z)) >> NORMALIZED_VECTOR_OUTPUT_SHIFT;

  return reciprocal;
}

int32
calculate_triangle_cross_product(const Point2 *first_,
                                 const Point2 *origin_,
                                 const Point2 *second_,
                                 uint32        shift_)
{
  int32 first_x;
  int32 first_y;
  int32 second_x;
  int32 second_y;

  first_x  = ((first_->x - origin_->x) >> shift_);
  first_y  = ((first_->y - origin_->y) >> shift_);
  second_x = ((second_->x - origin_->x) >> shift_);
  second_y = ((second_->y - origin_->y) >> shift_);

  return ((first_x * second_y) - (first_y * second_x));
}

int32
calculate_indexed_triangle_cross_product(const Point2 *points_,
                                         const int32  *indices_,
                                         uint32        shift_)
{
  const Point2 *first;
  const Point2 *origin;
  const Point2 *second;
  int32         first_x;
  int32         first_y;
  int32         second_x;
  int32         second_y;

  first    = &points_[indices_[0]];
  origin   = &points_[indices_[1]];
  second   = &points_[indices_[TRIANGLE_SECOND_INDEX]];
  first_x  = ((first->x - origin->x) >> shift_);
  first_y  = ((first->y - origin->y) >> shift_);
  second_x = ((second->x - origin->x) >> shift_);
  second_y = ((second->y - origin->y) >> shift_);

  return ((first_x * second_y) - (first_y * second_x));
}

int32
evaluate_triangle_plane(const Vector3 *point_,
                        int32          first_x_,
                        int32          first_y_,
                        int32          first_z_,
                        int32          second_x_,
                        int32          second_y_,
                        int32          second_z_,
                        int32          third_x_,
                        int32          third_y_,
                        int32          third_z_)
{
  Vector3 first_edge;
  Vector3 normal;
  Vector3 second_edge;
  int32   plane_height;
  int32   point_x_offset;
  int32   point_z_offset;

  first_edge.x  = (int32) ((uint32) first_x_ - (uint32) second_x_);
  first_edge.y  = (int32) ((uint32) first_y_ - (uint32) second_y_);
  first_edge.z  = (int32) ((uint32) first_z_ - (uint32) second_z_);
  second_edge.x = (int32) ((uint32) third_x_ - (uint32) second_x_);
  second_edge.y = (int32) ((uint32) third_y_ - (uint32) second_y_);
  second_edge.z = (int32) ((uint32) third_z_ - (uint32) second_z_);
  Cross3_F16((frac16 *) &normal, (frac16 *) &first_edge, (frac16 *) &second_edge);

  point_x_offset = (int32) ((uint32) point_->x - (uint32) first_x_);
  point_z_offset = (int32) ((uint32) point_->z - (uint32) first_z_);
  plane_height   = (int32) (0U - (uint32) MulSF16(normal.x, point_x_offset));
  plane_height   = (int32) ((uint32) plane_height - (uint32) MulSF16(normal.z, point_z_offset));
  plane_height   = DivSF16(plane_height, normal.y);
  plane_height   = (int32) ((uint32) plane_height + (uint32) first_y_);

  if((normal.x == 0) && (normal.y == 0) && (normal.z == 0))
    plane_height = (int32) ((uint32) first_y_ + FIXED16_ONE);

  /* The oracle discards every nonzero plane result. */
  if(plane_height != 0)
    plane_height = 0;

  return plane_height;
}

void
project_vectors_onto_plane(Vector3       *first_vector_,
                           Vector3       *second_vector_,
                           Vector3       *world_vector_,
                           const Vector3 *plane_first_,
                           const Vector3 *plane_origin_,
                           const Vector3 *plane_second_)
{
  Vector3 first_edge;
  Vector3 second_edge;
  Vector3 normal;
  int32   normal_x;
  int32   normal_z;
  int32   plane_height;
  int32   reciprocal_y;
  int32   relative_x;
  int32   relative_z;

  first_edge.x  = (int32) ((uint32) plane_first_->x - (uint32) plane_origin_->x);
  first_edge.y  = (int32) ((uint32) plane_first_->y - (uint32) plane_origin_->y);
  first_edge.z  = (int32) ((uint32) plane_first_->z - (uint32) plane_origin_->z);
  second_edge.x = (int32) ((uint32) plane_second_->x - (uint32) plane_origin_->x);
  second_edge.y = (int32) ((uint32) plane_second_->y - (uint32) plane_origin_->y);
  second_edge.z = (int32) ((uint32) plane_second_->z - (uint32) plane_origin_->z);
  Cross3_F16((frac16 *) &normal, (frac16 *) &first_edge, (frac16 *) &second_edge);

  reciprocal_y = (DivSF16(FIXED16_ONE, normal.y) >> FIXED8_FRACTION_BITS);
  normal_x     = (normal.x >> FIXED8_FRACTION_BITS);
  normal_z     = (normal.z >> FIXED8_FRACTION_BITS);
  plane_height =
    (int32) (0U - ((uint32) (first_vector_->x >> FIXED8_FRACTION_BITS) * (uint32) normal_x));
  plane_height =
    (int32) ((uint32) plane_height -
             ((uint32) (first_vector_->z >> FIXED8_FRACTION_BITS) * (uint32) normal_z));
  first_vector_->y =
    (int32) ((uint32) reciprocal_y * (uint32) (plane_height >> FIXED8_FRACTION_BITS));

  plane_height =
    (int32) (0U - ((uint32) (second_vector_->x >> FIXED8_FRACTION_BITS) * (uint32) normal_x));
  plane_height =
    (int32) ((uint32) plane_height -
             ((uint32) (second_vector_->z >> FIXED8_FRACTION_BITS) * (uint32) normal_z));
  second_vector_->y =
    (int32) ((uint32) reciprocal_y * (uint32) (plane_height >> FIXED8_FRACTION_BITS));

  relative_x   = (int32) ((uint32) world_vector_->x - (uint32) plane_origin_->x);
  relative_z   = (int32) ((uint32) world_vector_->z - (uint32) plane_origin_->z);
  plane_height = (int32) (0U - ((uint32) (relative_x >> FIXED8_FRACTION_BITS) * (uint32) normal_x));
  plane_height = (int32) ((uint32) plane_height -
                          ((uint32) (relative_z >> FIXED8_FRACTION_BITS) * (uint32) normal_z));
  world_vector_->y =
    (int32) ((uint32) plane_origin_->y +
             ((uint32) reciprocal_y * (uint32) (plane_height >> FIXED8_FRACTION_BITS)));
}

void
transpose_matrix_3x3(const Matrix3x3 *source_, Matrix3x3 *destination_)
{
  int32 m00;
  int32 m01;
  int32 m02;
  int32 m10;
  int32 m11;
  int32 m12;
  int32 m20;
  int32 m21;
  int32 m22;

  m00 = source_->m00;
  m01 = source_->m01;
  m02 = source_->m02;
  m10 = source_->m10;
  m11 = source_->m11;
  m12 = source_->m12;
  m20 = source_->m20;
  m21 = source_->m21;
  m22 = source_->m22;

  destination_->m00 = m00;
  destination_->m01 = m10;
  destination_->m02 = m20;
  destination_->m10 = m01;
  destination_->m11 = m11;
  destination_->m12 = m21;
  destination_->m20 = m02;
  destination_->m21 = m12;
  destination_->m22 = m22;
}

Vector3 *
add_vector3(const Vector3 *left_, const Vector3 *right_, Vector3 *destination_)
{
  destination_->x = (int32) ((uint32) left_->x + (uint32) right_->x);
  destination_->y = (int32) ((uint32) left_->y + (uint32) right_->y);
  destination_->z = (int32) ((uint32) left_->z + (uint32) right_->z);

  return destination_;
}

Vector3 *
subtract_vector3(const Vector3 *left_, const Vector3 *right_, Vector3 *destination_)
{
  destination_->x = (int32) ((uint32) left_->x - (uint32) right_->x);
  destination_->y = (int32) ((uint32) left_->y - (uint32) right_->y);
  destination_->z = (int32) ((uint32) left_->z - (uint32) right_->z);

  return destination_;
}

int32
dot_vector3(const Vector3 *left_, const Vector3 *right_)
{
  uint32 product;

  product = ((uint32) left_->x * (uint32) right_->x);
  product += ((uint32) left_->y * (uint32) right_->y);
  product += ((uint32) left_->z * (uint32) right_->z);

  return (int32) product;
}

Matrix3x3 *
add_matrix_3x3(const Matrix3x3 *left_, const Matrix3x3 *right_, Matrix3x3 *destination_)
{
  const int32 *left_values;
  const int32 *right_values;
  int32       *destination_values;
  int32        index;

  left_values        = (const int32 *) left_;
  right_values       = (const int32 *) right_;
  destination_values = (int32 *) destination_;
  for(index = 0; index < MATRIX_3X3_ELEMENT_COUNT; index++)
    {
      destination_values[index] =
        (int32) ((uint32) left_values[index] + (uint32) right_values[index]);
    }

  return destination_;
}

Matrix3x3 *
subtract_matrix_3x3(const Matrix3x3 *left_, const Matrix3x3 *right_, Matrix3x3 *destination_)
{
  const int32 *left_values;
  const int32 *right_values;
  int32       *destination_values;
  int32        index;

  left_values        = (const int32 *) left_;
  right_values       = (const int32 *) right_;
  destination_values = (int32 *) destination_;
  for(index = 0; index < MATRIX_3X3_ELEMENT_COUNT; index++)
    {
      destination_values[index] =
        (int32) ((uint32) left_values[index] - (uint32) right_values[index]);
    }

  return destination_;
}

Vector3 *
scale_vector3(const Vector3 *source_, int32 scale_, Vector3 *destination_)
{
  destination_->x = multiply_fixed16(source_->x, scale_);
  destination_->y = multiply_fixed16(source_->y, scale_);
  destination_->z = multiply_fixed16(source_->z, scale_);

  return destination_;
}

void
scale_vector3_in_place(Vector3 *vector_, int32 scale_)
{
  vector_->x = multiply_fixed16(scale_, vector_->x);
  vector_->y = multiply_fixed16(scale_, vector_->y);
  vector_->z = multiply_fixed16(scale_, vector_->z);
}

void
normalize_vector3_64_in_place(Vector3 *vector_)
{
  uint32 magnitude;

  magnitude = calculate_vector3_magnitude_64(vector_);
  if(magnitude == 0)
    {
      return;
    }

  vector_->x = DivSF16(vector_->x, (frac16) magnitude);
  vector_->y = DivSF16(vector_->y, (frac16) magnitude);
  vector_->z = DivSF16(vector_->z, (frac16) magnitude);
}

void
normalize_vector3_in_place(Vector3 *vector_)
{
  ufrac16 magnitude;
  uint32  squared_magnitude;
  int32   scale;

  squared_magnitude = (uint32) MulSF16(vector_->x, vector_->x);
  squared_magnitude += (uint32) MulSF16(vector_->y, vector_->y);
  squared_magnitude += (uint32) MulSF16(vector_->z, vector_->z);
  magnitude = SqrtF16((ufrac16) squared_magnitude);
  if(magnitude == 0)
    {
      return;
    }

  scale      = DivSF16(FIXED16_ONE, (frac16) magnitude);
  vector_->x = MulSF16(vector_->x, scale);
  vector_->y = MulSF16(vector_->y, scale);
  vector_->z = MulSF16(vector_->z, scale);
}

void
normalize_vector3_fixed8_in_place(Vector3 *vector_)
{
  ufrac16 magnitude;
  int32   scale;
  uint32  squared_magnitude;
  int32   x;
  int32   y;
  int32   z;

  x                 = vector_->x >> FIXED8_FRACTION_BITS;
  y                 = vector_->y >> FIXED8_FRACTION_BITS;
  z                 = vector_->z >> FIXED8_FRACTION_BITS;
  squared_magnitude = (uint32) x * (uint32) x;
  squared_magnitude += (uint32) y * (uint32) y;
  squared_magnitude += (uint32) z * (uint32) z;
  magnitude = SqrtF16((ufrac16) squared_magnitude);
  if(magnitude == 0)
    {
      return;
    }

  scale      = DivSF16(FIXED16_ONE, (frac16) magnitude) >> FIXED8_FRACTION_BITS;
  vector_->x = (int32) ((uint32) x * (uint32) scale);
  vector_->y = (int32) ((uint32) y * (uint32) scale);
  vector_->z = (int32) ((uint32) z * (uint32) scale);
}

void
orthonormalize_matrix3x3(Matrix3x3 *matrix_)
{
  Vector3 first_row;
  Vector3 second_row;
  Vector3 third_row;

  first_row.x  = matrix_->m00;
  first_row.y  = matrix_->m01;
  first_row.z  = matrix_->m02;
  second_row.x = matrix_->m10;
  second_row.y = matrix_->m11;
  second_row.z = matrix_->m12;

  _normalize_matrix3x3_row(&first_row);
  _cross_matrix3x3_rows(&first_row, &second_row, &third_row);
  _normalize_matrix3x3_row(&third_row);
  _cross_matrix3x3_rows(&third_row, &first_row, &second_row);

  matrix_->m00 = first_row.x;
  matrix_->m01 = first_row.y;
  matrix_->m02 = first_row.z;
  matrix_->m10 = second_row.x;
  matrix_->m11 = second_row.y;
  matrix_->m12 = second_row.z;
  matrix_->m20 = third_row.x;
  matrix_->m21 = third_row.y;
  matrix_->m22 = third_row.z;
}

uint32
calculate_vector3_magnitude_64(const Vector3 *vector_)
{
  int64 component_square;
  int64 squared_magnitude;

  MulS32_64(&squared_magnitude, vector_->x, vector_->x);
  MulS32_64(&component_square, vector_->y, vector_->y);
  Add64(&squared_magnitude, &squared_magnitude, &component_square);
  MulS32_64(&component_square, vector_->z, vector_->z);
  Add64(&squared_magnitude, &squared_magnitude, &component_square);

  return Sqrt64_32((uint64 *) &squared_magnitude);
}

int32
calculate_vector3_squared_magnitude_fixed16(const Vector3 *vector_)
{
  uint32 squared_magnitude;

  squared_magnitude = (uint32) MulSF16(vector_->x, vector_->x);
  squared_magnitude += (uint32) MulSF16(vector_->y, vector_->y);
  squared_magnitude += (uint32) MulSF16(vector_->z, vector_->z);

  return (int32) squared_magnitude;
}

int32
calculate_vector3_planar_squared_distance_fixed8(const Vector3 *first_, const Vector3 *second_)
{
  int32  x_difference;
  int32  z_difference;
  uint32 squared_distance;

  x_difference     = (second_->x - first_->x) >> FIXED8_FRACTION_BITS;
  z_difference     = (second_->z - first_->z) >> FIXED8_FRACTION_BITS;
  squared_distance = ((uint32) x_difference * (uint32) x_difference) +
                     ((uint32) z_difference * (uint32) z_difference);

  return (int32) squared_distance;
}

int32
calculate_vector3_distance(const Vector3 *left_, const Vector3 *right_)
{
  Vector3 difference;

  subtract_vector3(left_, right_, &difference);
  return calculate_vector_magnitude(&difference);
}

int32
calculate_clamped_approximate_distance_2d(const ApproximateDistancePoint *point_,
                                          int32                           maximum_distance_)
{
  int32 x_distance;
  int32 y_distance;
  int32 distance;

  x_distance = point_->x;
  if(x_distance <= 0)
    x_distance = -x_distance;
  y_distance = point_->y;
  if(y_distance <= 0)
    y_distance = -y_distance;

  if(y_distance <= x_distance)
    distance = x_distance + (y_distance >> APPROXIMATE_DISTANCE_MINOR_SHIFT);
  else
    distance = y_distance + (x_distance >> APPROXIMATE_DISTANCE_MINOR_SHIFT);

  distance <<= APPROXIMATE_DISTANCE_SCALE_SHIFT;
  if(distance > maximum_distance_)
    distance = maximum_distance_;

  return distance;
}

void
rotate_vector3_y_fixed8(const Vector3 *source_, int32 angle_, Vector3 *destination_)
{
  int32 component_a;
  int32 component_b;
  int32 x_product;
  int32 z_product;

  component_a     = LOOKUP_ROTATION_COMPONENT_A(angle_);
  x_product       = (int32) ((uint32) (source_->x >> FIXED8_FRACTION_BITS) * (uint32) component_a);
  component_b     = LOOKUP_ROTATION_COMPONENT_B(angle_);
  z_product       = (int32) ((uint32) (source_->z >> FIXED8_FRACTION_BITS) * (uint32) component_b);
  destination_->x = (int32) ((uint32) x_product + (uint32) z_product) >> FIXED8_FRACTION_BITS;
  destination_->y = source_->y;

  z_product       = (int32) ((uint32) (source_->z >> FIXED8_FRACTION_BITS) * (uint32) component_a);
  x_product       = (int32) ((uint32) (source_->x >> FIXED8_FRACTION_BITS) * (uint32) component_b);
  destination_->z = (int32) ((uint32) z_product - (uint32) x_product) >> FIXED8_FRACTION_BITS;
}

void
rotate_coordinates_fixed16(int32 x_, int32 y_, int32 angle_, int32 *rotated_x_, int32 *rotated_y_)
{
  int32 component_a;
  int32 component_b;
  int32 x_component;
  int32 y_component;

  component_a = LOOKUP_ROTATION_COMPONENT_A(angle_);
  component_b = LOOKUP_ROTATION_COMPONENT_B(angle_);
  x_component = MulSF16(component_a, x_);
  y_component = MulSF16(component_b, y_);
  *rotated_x_ = (int32) ((uint32) x_component - (uint32) y_component);
  x_component = MulSF16(component_b, x_);
  y_component = MulSF16(component_a, y_);
  *rotated_y_ = (int32) ((uint32) x_component + (uint32) y_component);
}

void
rotate_coordinates_fixed8(int32 x_, int32 y_, int32 angle_, int32 *rotated_x_, int32 *rotated_y_)
{
  int32 component_a;
  int32 component_b;
  int32 x_component;
  int32 y_component;

  component_a = LOOKUP_ROTATION_COMPONENT_A(angle_) >> FIXED8_FRACTION_BITS;
  component_b = LOOKUP_ROTATION_COMPONENT_B(angle_) >> FIXED8_FRACTION_BITS;
  x_component = (x_ >> FIXED8_FRACTION_BITS) * component_a;
  y_component = (y_ >> FIXED8_FRACTION_BITS) * component_b;
  *rotated_x_ = (int32) ((uint32) x_component - (uint32) y_component);
  x_component = (x_ >> FIXED8_FRACTION_BITS) * component_b;
  y_component = (y_ >> FIXED8_FRACTION_BITS) * component_a;
  *rotated_y_ = (int32) ((uint32) x_component + (uint32) y_component);
}

Matrix3x3 *
scale_matrix_3x3(const Matrix3x3 *source_, int32 scale_, Matrix3x3 *destination_)
{
  const int32 *source_values;
  int32       *destination_values;
  int32        index;

  source_values      = (const int32 *) source_;
  destination_values = (int32 *) destination_;
  for(index = 0; index < MATRIX_3X3_ELEMENT_COUNT; index++)
    {
      destination_values[index] = multiply_fixed16(source_values[index], scale_);
    }

  return destination_;
}

Vector3 *
cross_vector3(const Vector3 *left_, const Vector3 *right_, Vector3 *destination_)
{
  int32 left_product;
  int32 right_product;

  left_product    = multiply_fixed16(left_->y, right_->z);
  right_product   = multiply_fixed16(left_->z, right_->y);
  destination_->x = (int32) ((uint32) left_product - (uint32) right_product);
  left_product    = multiply_fixed16(left_->z, right_->x);
  right_product   = multiply_fixed16(left_->x, right_->z);
  destination_->y = (int32) ((uint32) left_product - (uint32) right_product);
  left_product    = multiply_fixed16(left_->x, right_->y);
  right_product   = multiply_fixed16(left_->y, right_->x);
  destination_->z = (int32) ((uint32) left_product - (uint32) right_product);

  return destination_;
}

Vector3 *
calculate_normalized_cross_product(const Vector3 *left_,
                                   const Vector3 *right_,
                                   Vector3       *destination_)
{
  cross_vector3(left_, right_, destination_);
  normalize_vector(destination_, destination_);

  return destination_;
}

void
decompose_vector3_projection(const Vector3 *source_,
                             const Vector3 *axis_,
                             Vector3       *projection_,
                             Vector3       *remainder_)
{
  int32 axis_squared_magnitude;
  int32 projection_dot_product;
  int32 projection_scale;

  projection_dot_product = dot_vector3(source_, axis_);
  axis_squared_magnitude = dot_vector3(axis_, axis_);

  while(projection_dot_product > VECTOR_PROJECTION_DOT_LIMIT)
    {
      projection_dot_product >>= 1;
      axis_squared_magnitude >>= 1;
    }
  while(axis_squared_magnitude > VECTOR_PROJECTION_DOT_LIMIT)
    {
      projection_dot_product >>= 1;
      axis_squared_magnitude >>= 1;
    }

  projection_scale = divide_fixed16_or_one(projection_dot_product, axis_squared_magnitude);
  projection_->x   = multiply_fixed16(projection_scale, axis_->x);
  projection_->y   = multiply_fixed16(projection_scale, axis_->y);
  projection_->z   = multiply_fixed16(projection_scale, axis_->z);

  remainder_->x = (int32) ((uint32) source_->x - (uint32) projection_->x);
  remainder_->y = (int32) ((uint32) source_->y - (uint32) projection_->y);
  remainder_->z = (int32) ((uint32) source_->z - (uint32) projection_->z);
}

void
multiply_matrix_3x3(const Matrix3x3 *left_, const Matrix3x3 *right_, Matrix3x3 *destination_)
{
  const int32 *left_values;
  const int32 *right_values;
  int32       *destination_values;
  int32        column;
  int32        left_x;
  int32        left_y;
  int32        left_z;
  int32        row;
  uint32       product_sum;

  left_values        = (const int32 *) left_;
  right_values       = (const int32 *) right_;
  destination_values = (int32 *) destination_;
  for(row = 0; row < MATRIX_3X3_DIMENSION; row++)
    {
      left_x = (left_values[0] >> MATRIX_3X3_OPERAND_SHIFT);
      left_y = (left_values[1] >> MATRIX_3X3_OPERAND_SHIFT);
      left_z = (left_values[VECTOR_Z_INDEX] >> MATRIX_3X3_OPERAND_SHIFT);
      for(column = 0; column < MATRIX_3X3_DIMENSION; column++)
        {
          product_sum = ((uint32) left_x * (uint32) right_values[column]);
          product_sum +=
            ((uint32) left_y * (uint32) right_values[MATRIX_3X3_SECOND_ROW_OFFSET + column]);
          product_sum +=
            ((uint32) left_z * (uint32) right_values[MATRIX_3X3_THIRD_ROW_OFFSET + column]);
          *destination_values++ = ((int32) product_sum >> MATRIX_3X3_ACCUMULATOR_SHIFT);
        }
      left_values += MATRIX_3X3_DIMENSION;
    }
}

void
multiply_vector3_matrix3x3(const Vector3 *vector_, const Matrix3x3 *matrix_, Vector3 *destination_)
{
  int32  x;
  int32  y;
  int32  z;
  uint32 product_sum;

  x           = vector_->x;
  y           = vector_->y;
  z           = vector_->z;
  product_sum = ((uint32) x * (uint32) matrix_->m00);
  product_sum += ((uint32) y * (uint32) matrix_->m10);
  product_sum += ((uint32) z * (uint32) matrix_->m20);
  destination_->x = ((int32) product_sum >> MATRIX_FIXED_FRACTION_BITS);
  product_sum     = ((uint32) x * (uint32) matrix_->m01);
  product_sum += ((uint32) y * (uint32) matrix_->m11);
  product_sum += ((uint32) z * (uint32) matrix_->m21);
  destination_->y = ((int32) product_sum >> MATRIX_FIXED_FRACTION_BITS);
  product_sum     = ((uint32) x * (uint32) matrix_->m02);
  product_sum += ((uint32) y * (uint32) matrix_->m12);
  product_sum += ((uint32) z * (uint32) matrix_->m22);
  destination_->z = ((int32) product_sum >> MATRIX_FIXED_FRACTION_BITS);
}

void
multiply_vector3_array_matrix3x3(int32            vector_count_,
                                 const Vector3   *source_,
                                 const Matrix3x3 *matrix_,
                                 Vector3         *destination_)
{
  if(vector_count_ > VECTOR_MATRIX_BULK_THRESHOLD)
    {
      MulManyVec3Mat33_F16((vec3f16 *) destination_,
                           (vec3f16 *) source_,
                           *(mat33f16 *) matrix_,
                           vector_count_);
      return;
    }

  if(vector_count_ == 0)
    return;
  do
    {
      multiply_vector3_matrix3x3(source_, matrix_, destination_);
      source_++;
      destination_++;
      vector_count_--;
    }
  while(vector_count_ != 0);
}

void
translate_shifted_vector3_array(int32          vector_count_,
                                const Vector3 *source_,
                                uint32         shift_,
                                const Vector3 *translation_,
                                Vector3       *destination_)
{
  /* ARM SDT implements each signed right shift as an arithmetic shift. */
  while(vector_count_ > 0)
    {
      destination_->x = (int32) ((uint32) (source_->x >> shift_) + (uint32) translation_->x);
      destination_->y = (int32) ((uint32) (source_->y >> shift_) + (uint32) translation_->y);
      destination_->z = (int32) ((uint32) (source_->z >> shift_) + (uint32) translation_->z);
      source_++;
      destination_++;
      vector_count_--;
    }
}

void
translate_vector3_array(int32          vector_count_,
                        const Vector3 *source_,
                        const Vector3 *translation_,
                        Vector3       *destination_)
{
  while(vector_count_ > 0)
    {
      destination_->x = (int32) ((uint32) source_->x + (uint32) translation_->x);
      destination_->y = (int32) ((uint32) source_->y + (uint32) translation_->y);
      destination_->z = (int32) ((uint32) source_->z + (uint32) translation_->z);
      source_++;
      destination_++;
      vector_count_--;
    }
}

void
mask_shifted_point2_array(int32                  point_count_,
                          const Point2          *source_,
                          uint32                 shift_,
                          const CoordinateMask2 *masks_,
                          Point2                *destination_)
{
  while(point_count_ > 0)
    {
      destination_->x = (int32) (masks_->x & (uint32) (source_->x >> shift_));
      destination_->y = (int32) (masks_->y & (uint32) (source_->y >> shift_));
      source_++;
      destination_++;
      point_count_--;
    }
}
