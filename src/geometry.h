#pragma once

#include "base_types.h"

typedef struct Point2
{
  int32 x;
  int32 y;
} Point2;

typedef struct Vector3
{
  int32 x;
  int32 y;
  int32 z;
} Vector3;

typedef struct ApproximateDistancePoint
{
  uint8 reserved_00[0x54];
  int32 x;
  uint8 reserved_58[4];
  int32 y;
} ApproximateDistancePoint;

typedef struct ReferenceDistanceRecord
{
  uint8   reserved_00[2];
  uint8   negative_road_edge_width;
  uint8   positive_road_edge_width;
  uint8   lane_boundary_offsets;
  uint8   boundary_behavior_codes;
  uint8   surface_index_codes;
  uint8   road_geometry_type;
  Vector3 position;
  uint8   packed_track_direction[6];
  uint8   traffic_event_flags_high;
  uint8   traffic_event_flags_low;
  uint8   reserved_1_c[8];
} ReferenceDistanceRecord;

typedef struct Matrix3x3
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
} Matrix3x3;

typedef struct CoordinateMask2
{
  uint32 x;
  uint32 y;
} CoordinateMask2;

extern CoordinateMask2          g_PROJECTED_POINT_COORDINATE_MASKS;
extern ReferenceDistanceRecord *g_REFERENCE_DISTANCE_RECORDS;
extern Matrix3x3                g_MATRI_X3_X3_IDENTITY;
extern int32                    g_MATRI_X3_X3_ORTHOGONALITY_COEFFICIENTS[10];

int32
normalize_angle24(int32 angle_);
int32
fold_angle24_to_quadrant(int32 angle_);
int32
calculate_angle_from_y_and_negative_x_f16(int32 x_, int32 y_);

typedef struct MatrixControllerState
{
  Matrix3x3 matrix;
  Vector3   position;
  Vector3   velocity;
  Vector3   angular_velocity;
  int32     fields_48[6];
  int32     first_value;
  int32     second_value;
  int32     first_reciprocal;
  int32     second_reciprocal;
  int32     collision_radius;
  Vector3   half_extents;
  int32     stability_measure;
  int32     contact_countdown;
} MatrixControllerState;

void
initialize_matrix3x3_identity(Matrix3x3 *matrix_);
void
initialize_matrix3x3_rotation_z(Matrix3x3 *matrix_, int32 angle_);
void
initialize_matrix3x3_rotation_y(Matrix3x3 *matrix_, int32 angle_);
void
initialize_matrix3x3_rotation_x(Matrix3x3 *matrix_, int32 angle_);
void
initialize_matrix3x3_rotation_x_1024(Matrix3x3 *matrix_, int32 angle_);
void
initialize_matrix3x3_rotation_y_1024(Matrix3x3 *matrix_, int32 angle_);
void
initialize_matrix3x3_rotation_z_1024(Matrix3x3 *matrix_, int32 angle_);
void
initialize_matrix3x3_inverse_rotation_x(Matrix3x3 *matrix_, int32 angle_);
void
initialize_matrix3x3_inverse_rotation_y(Matrix3x3 *matrix_, int32 angle_);
void
initialize_matrix3x3_inverse_rotation_z(Matrix3x3 *matrix_, int32 angle_);
void
initialize_matrix3x3_axis_rotation(Matrix3x3 *matrix_, int32 angle_, const Vector3 *axis_);
Matrix3x3 *
initialize_matrix3x3_axis_rotation_1024(const Vector3 *axis_, int32 angle_, Matrix3x3 *matrix_);
void
refine_matrix3x3_orthogonality(Matrix3x3 *matrix_);
void
extract_matrix3x3_rotation_angles(const Matrix3x3 *matrix_,
                                  int32           *x_angle_,
                                  int32           *y_angle_,
                                  int32           *z_angle_);
void
extract_matrix3x3_rotation_yzx_angles(const Matrix3x3 *matrix_,
                                      int32           *x_angle_,
                                      int32           *y_angle_,
                                      int32           *z_angle_);
void
initialize_matrix3x3_rotation_zxy(Matrix3x3 *matrix_,
                                  int32      z_angle_,
                                  int32      x_angle_,
                                  int32      y_angle_);
void
initialize_matrix3x3_rotation_yxz(Matrix3x3 *matrix_,
                                  int32      y_angle_,
                                  int32      x_angle_,
                                  int32      z_angle_);
void
initialize_matrix3x3_rotation_xzy(Matrix3x3 *matrix_,
                                  int32      x_angle_,
                                  int32      z_angle_,
                                  int32      y_angle_);
void
initialize_matrix_controller_state(int32                  first_value_,
                                   int32                  second_value_,
                                   void                  *context_,
                                   MatrixControllerState *state_);
void
advance_matrix_controller_state(MatrixControllerState *state_);
int32
integer_square_root(int32 value_);
int32
calculate_planar_magnitude(int32 x_, int32 y_);
/* The original helper leaves division by zero undefined. */
uint32
divide_unsigned_doubleword_by_word(uint32 low_word_, uint32 high_word_, uint32 divisor_);
int32
calculate_vector_magnitude(const Vector3 *vector_);
int32
normalize_vector(const Vector3 *source_, Vector3 *destination_);
int32
calculate_triangle_cross_product(const Point2 *first_,
                                 const Point2 *origin_,
                                 const Point2 *second_,
                                 uint32        shift_);
int32
calculate_indexed_triangle_cross_product(const Point2 *points_,
                                         const int32  *indices_,
                                         uint32        shift_);
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
                        int32          third_z_);
void
project_vectors_onto_plane(Vector3       *first_vector_,
                           Vector3       *second_vector_,
                           Vector3       *world_vector_,
                           const Vector3 *plane_first_,
                           const Vector3 *plane_origin_,
                           const Vector3 *plane_second_);
void
transpose_matrix_3x3(const Matrix3x3 *source_, Matrix3x3 *destination_);
Vector3 *
add_vector3(const Vector3 *left_, const Vector3 *right_, Vector3 *destination_);
Vector3 *
subtract_vector3(const Vector3 *left_, const Vector3 *right_, Vector3 *destination_);
int32
dot_vector3(const Vector3 *left_, const Vector3 *right_);
Matrix3x3 *
add_matrix_3x3(const Matrix3x3 *left_, const Matrix3x3 *right_, Matrix3x3 *destination_);
Matrix3x3 *
subtract_matrix_3x3(const Matrix3x3 *left_, const Matrix3x3 *right_, Matrix3x3 *destination_);
Vector3 *
scale_vector3(const Vector3 *source_, int32 scale_, Vector3 *destination_);
void
scale_vector3_in_place(Vector3 *vector_, int32 scale_);
void
normalize_vector3_64_in_place(Vector3 *vector_);
void
normalize_vector3_in_place(Vector3 *vector_);
void
normalize_vector3_fixed8_in_place(Vector3 *vector_);
void
orthonormalize_matrix3x3(Matrix3x3 *matrix_);
void
rebuild_matrix3x3_basis_from_normal(Matrix3x3 *matrix_, const Vector3 *normal_);
uint32
calculate_vector3_magnitude_64(const Vector3 *vector_);
int32
calculate_vector3_squared_magnitude_fixed16(const Vector3 *vector_);
int32
calculate_vector3_planar_squared_distance_fixed8(const Vector3 *first_, const Vector3 *second_);
int32
calculate_vector3_distance(const Vector3 *left_, const Vector3 *right_);
int32
calculate_vector3_planar_distance(const Vector3 *left_, const Vector3 *right_);
int32
calculate_consecutive_reference_distance_remaining(int32 record_index_);
int32
calculate_clamped_approximate_distance_2d(const ApproximateDistancePoint *point_,
                                          int32                           maximum_distance_);
void
rotate_vector3_y_fixed8(const Vector3 *source_, int32 angle_, Vector3 *destination_);
void
rotate_coordinates_fixed16(int32 x_, int32 y_, int32 angle_, int32 *rotated_x_, int32 *rotated_y_);
void
rotate_coordinates_fixed8(int32 x_, int32 y_, int32 angle_, int32 *rotated_x_, int32 *rotated_y_);
Matrix3x3 *
scale_matrix_3x3(const Matrix3x3 *source_, int32 scale_, Matrix3x3 *destination_);
Vector3 *
cross_vector3(const Vector3 *left_, const Vector3 *right_, Vector3 *destination_);
Vector3 *
calculate_normalized_cross_product(const Vector3 *left_,
                                   const Vector3 *right_,
                                   Vector3       *destination_);
void
decompose_vector3_projection(const Vector3 *source_,
                             const Vector3 *axis_,
                             Vector3       *projection_,
                             Vector3       *remainder_);
void
multiply_matrix_3x3(const Matrix3x3 *left_, const Matrix3x3 *right_, Matrix3x3 *destination_);
void
multiply_vector3_matrix3x3(const Vector3 *vector_, const Matrix3x3 *matrix_, Vector3 *destination_);
void
multiply_vector3_array_matrix3x3(int32            vector_count_,
                                 const Vector3   *source_,
                                 const Matrix3x3 *matrix_,
                                 Vector3         *destination_);
void
translate_shifted_vector3_array(int32          vector_count_,
                                const Vector3 *source_,
                                uint32         shift_,
                                const Vector3 *translation_,
                                Vector3       *destination_);
void
translate_vector3_array(int32          vector_count_,
                        const Vector3 *source_,
                        const Vector3 *translation_,
                        Vector3       *destination_);
void
mask_shifted_point2_array(int32                  point_count_,
                          const Point2          *source_,
                          uint32                 shift_,
                          const CoordinateMask2 *masks_,
                          Point2                *destination_);
