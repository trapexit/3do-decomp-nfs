#include "render_bounds.h"

#include "geometry.h"
#include "integer_angle.h"
#include "trig_lookup.h"

#include "operamath.h"

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

#define ANGLE_UNITS_PER_TURN 1024
#define DEGREES_PER_TURN 360
#define DEPTH_SCALE_FRACTION_BITS 16
#define DEPTH_SCALE_COARSE_THRESHOLD 4096
#define DEPTH_SCALE_COARSE_SHIFT 4
#define DEPTH_SCALE_FINE_THRESHOLD 1024
#define DEPTH_SCALE_FINE_SHIFT 2
#define ARRAY_DEPTH_COARSE_THRESHOLD 8192
#define ARRAY_DEPTH_MEDIUM_THRESHOLD 2048
#define ARRAY_DEPTH_FINE_THRESHOLD 1024
#define ARRAY_DEPTH_COARSE_SHIFT 4
#define ARRAY_DEPTH_MEDIUM_SHIFT 2
#define ARRAY_DEPTH_FINE_SHIFT 1
#define PROJECTION_OVERFLOW_THRESHOLD_SHIFT 15
#define PROJECTION_QUOTIENT_BITS 24
#define SIGNED_WORD_SIGN_BIT 0x80000000
#define SIGNED_WORD_MAXIMUM 0x7FFFFFFF
#define LEGACY_PROJECTION_FIXED_SHIFT 16
#define LEGACY_PROJECTION_HORIZONTAL_CENTER 160
#define LEGACY_PROJECTION_VERTICAL_CENTER 120
#define ZERO_SCALE_REDUCTION_SHIFT 3

COMPILE_TIME_SIZE_ASSERT(render_bounds_size, sizeof(RenderBounds) == 40);

RenderBounds          g_RENDER_BOUNDS = { 160, 120, 0, 0, 160, 120, 0, 0, 0, 0 };
LegacyProjectionState g_LEGACY_PROJECTION_STATE;

uint32  g_RENDER_WORKSPACE[RENDER_DEPTH_SCALE_TABLE_COUNT];
uint32 *g_RENDER_WORKSPACE_CURSOR = g_RENDER_WORKSPACE;

static uint32
_divide_projection_scale(uint32 dividend_, uint32 divisor_)
{
  int32  bit_index;
  uint32 quotient;
  uint32 remainder;

  quotient  = 0;
  remainder = dividend_;
  /* The projection entry emits exactly 24 quotient bits, not a full divide. */
  for(bit_index = PROJECTION_QUOTIENT_BITS - 1; bit_index >= 0; bit_index--)
    {
      quotient <<= 1;
      if((remainder >> bit_index) >= divisor_)
        {
          quotient |= 1;
          remainder -= divisor_ << bit_index;
        }
    }

  return quotient;
}

void
configure_legacy_projection(int32 angle_degrees_, int32 reserved_, int32 projection_extent_)
{
  int32  angle;
  int32  cotangent;
  uint32 product;

  angle                                       = angle_degrees_ << LEGACY_PROJECTION_FIXED_SHIFT;
  cotangent                                   = DivSF16(CosF16(angle), SinF16(angle));
  product                                     = (uint32) projection_extent_ * (uint32) cotangent;
  g_LEGACY_PROJECTION_STATE.projection_scale  = (int32) product >> LEGACY_PROJECTION_FIXED_SHIFT;
  g_LEGACY_PROJECTION_STATE.horizontal_center = LEGACY_PROJECTION_HORIZONTAL_CENTER;
  g_LEGACY_PROJECTION_STATE.vertical_center   = LEGACY_PROJECTION_VERTICAL_CENTER;
  (void) reserved_;
}

void
project_legacy_point(const Vector3 *point_, Point2 *projected_)
{
  int32  ratio;
  uint32 product;

  ratio         = DivSF16(point_->x, point_->z);
  product       = (uint32) g_LEGACY_PROJECTION_STATE.projection_scale * (uint32) ratio;
  projected_->x = g_LEGACY_PROJECTION_STATE.horizontal_center +
                  ((int32) product >> LEGACY_PROJECTION_FIXED_SHIFT);

  ratio   = DivSF16(point_->y, point_->z);
  product = (uint32) g_LEGACY_PROJECTION_STATE.projection_scale * (uint32) ratio;
  projected_->y =
    g_LEGACY_PROJECTION_STATE.vertical_center - ((int32) product >> LEGACY_PROJECTION_FIXED_SHIFT);
}

void
configure_render_bounds(uint32 horizontal_origin_, uint32 vertical_origin_)
{
  g_RENDER_BOUNDS.horizontal_origin = horizontal_origin_;
  g_RENDER_BOUNDS.horizontal_limit  = horizontal_origin_ + g_RENDER_BOUNDS.horizontal_half_extent;
  g_RENDER_BOUNDS.vertical_origin   = vertical_origin_;
  g_RENDER_BOUNDS.vertical_limit    = vertical_origin_ + g_RENDER_BOUNDS.vertical_half_extent;
}

void
configure_render_projection_angles(uint32 horizontal_angle_,
                                   uint32 vertical_angle_,
                                   uint32 width_,
                                   uint32 height_)
{
  RenderBounds *derived_bounds;
  int32         cosine;
  uint32        horizontal_half_extent;
  int32         horizontal_projection_scale;
  uint32        reduced_scale;
  int32         sine;
  int32         stored_vertical_angle;
  uint32        vertical_half_extent;

  g_RENDER_BOUNDS.horizontal_view_angle  = (int32) horizontal_angle_;
  g_RENDER_BOUNDS.vertical_view_angle    = (int32) vertical_angle_;
  horizontal_half_extent                 = (width_ >> 1);
  g_RENDER_BOUNDS.horizontal_half_extent = horizontal_half_extent;
  g_RENDER_BOUNDS.horizontal_limit =
    (g_RENDER_BOUNDS.horizontal_origin + horizontal_half_extent);
  vertical_half_extent                 = (height_ >> 1);
  g_RENDER_BOUNDS.vertical_half_extent = vertical_half_extent;
  g_RENDER_BOUNDS.vertical_limit =
    (g_RENDER_BOUNDS.vertical_origin + vertical_half_extent);

  cosine = cosine_1024(g_RENDER_BOUNDS.horizontal_view_angle);
  sine   = sine_1024(g_RENDER_BOUNDS.horizontal_view_angle);
  horizontal_projection_scale =
    (int32) (((uint32) cosine * g_RENDER_BOUNDS.horizontal_half_extent) / (uint32) sine);
  g_RENDER_BOUNDS.horizontal_projection_scale = horizontal_projection_scale;
  stored_vertical_angle                       = g_RENDER_BOUNDS.vertical_view_angle;

  if(horizontal_projection_scale == 0)
    {
      reduced_scale = (uint32) horizontal_projection_scale;
      reduced_scale = (reduced_scale - (reduced_scale >> ZERO_SCALE_REDUCTION_SHIFT));
      reduced_scale = (reduced_scale -
                       ((uint32) horizontal_projection_scale >>
                        ZERO_SCALE_REDUCTION_SHIFT));
      /* RETAIL UB: the zero-scale branch halves the relocated bounds pointer. */
      derived_bounds = (RenderBounds *) (((uint32) &g_RENDER_BOUNDS) >> 1);
      derived_bounds->vertical_projection_scale = (int32) reduced_scale;
      derived_bounds->vertical_view_angle =
        calculate_integer_angle((int32) reduced_scale,
                                (int32) derived_bounds->vertical_half_extent);
      return;
    }

  cosine = cosine_1024(stored_vertical_angle);
  sine   = sine_1024(g_RENDER_BOUNDS.vertical_view_angle);
  g_RENDER_BOUNDS.vertical_projection_scale =
    (int32) (((uint32) cosine * g_RENDER_BOUNDS.vertical_half_extent) / (uint32) sine);
}

void
configure_render_projection_degrees(uint32 horizontal_degrees_,
                                    uint32 vertical_degrees_,
                                    uint32 width_,
                                    uint32 height_)
{
  uint32 horizontal_angle;
  uint32 vertical_angle;

  horizontal_angle = (horizontal_degrees_ * ANGLE_UNITS_PER_TURN) / DEGREES_PER_TURN;
  vertical_angle   = (vertical_degrees_ * ANGLE_UNITS_PER_TURN) / DEGREES_PER_TURN;
  configure_render_projection_angles(horizontal_angle, vertical_angle, width_, height_);
}

uint32 *
set_render_workspace_cursor(uint32 *workspace_cursor_)
{
  uint32 *previous_cursor;

  previous_cursor           = g_RENDER_WORKSPACE_CURSOR;
  g_RENDER_WORKSPACE_CURSOR = workspace_cursor_;
  return previous_cursor;
}

void
initialize_depth_scale_table(void)
{
  uint32  depth;
  uint32  scaled_projection;
  uint32 *workspace_cursor;

  scaled_projection =
    ((uint32) g_RENDER_BOUNDS.horizontal_projection_scale << DEPTH_SCALE_FRACTION_BITS);
  workspace_cursor    = g_RENDER_WORKSPACE_CURSOR;
  workspace_cursor[0] = ~(uint32) 0;
  for(depth = 1; depth < RENDER_DEPTH_SCALE_TABLE_COUNT; depth++)
    {
      workspace_cursor[depth] = (scaled_projection / depth);
    }
}

void
project_point_with_division(const Vector3 *point_, Point2 *projected_)
{
  int32  horizontal_product;
  uint32 projection_scale;
  int32  vertical_product;

  projection_scale   = _divide_projection_scale((uint32) g_RENDER_BOUNDS.horizontal_projection_scale
                                                  << DEPTH_SCALE_FRACTION_BITS,
                                                (uint32) point_->z);
  horizontal_product = (int32) ((uint32) point_->x * projection_scale);
  vertical_product   = (int32) ((uint32) point_->y * projection_scale);
  projected_->x      = (int32) ((uint32) g_RENDER_BOUNDS.horizontal_limit +
                                (uint32) (horizontal_product >> DEPTH_SCALE_FRACTION_BITS));
  projected_->y      = (int32) ((uint32) g_RENDER_BOUNDS.vertical_limit -
                                (uint32) (vertical_product >> DEPTH_SCALE_FRACTION_BITS));
}

void
project_point_saturating(const Vector3 *point_, Point2 *projected_)
{
  int32  component;
  int32  depth;
  uint32 magnitude;
  uint32 origin;
  uint32 product;
  uint32 projection;
  uint32 result;

  depth = point_->z;
  if(depth <= 0)
    {
      projected_->x = (int32) SIGNED_WORD_SIGN_BIT;
      projected_->y = (int32) SIGNED_WORD_SIGN_BIT;
      return;
    }

  component = point_->x;
  if(component < 0)
    magnitude = 0 - (uint32) component;
  else
    magnitude = (uint32) component;
  product = (uint32) g_RENDER_BOUNDS.horizontal_projection_scale * magnitude;
  if(depth <= (int32) (product >> PROJECTION_OVERFLOW_THRESHOLD_SHIFT))
    {
      if(component < 0)
        projected_->x = (int32) SIGNED_WORD_SIGN_BIT;
      else
        projected_->x = SIGNED_WORD_MAXIMUM;
    }
  else
    {
      projection = product / (uint32) depth;
      origin     = g_RENDER_BOUNDS.horizontal_limit;
      if(component < 0)
        {
          result = origin - projection;
          if((((origin ^ projection) & (origin ^ result)) & SIGNED_WORD_SIGN_BIT) != 0)
            {
              if((result & SIGNED_WORD_SIGN_BIT) != 0)
                result = SIGNED_WORD_MAXIMUM;
              else
                result = SIGNED_WORD_SIGN_BIT;
            }
        }
      else
        {
          result = origin + projection;
          if(((~(origin ^ projection) & (origin ^ result)) & SIGNED_WORD_SIGN_BIT) != 0)
            {
              if((result & SIGNED_WORD_SIGN_BIT) != 0)
                result = SIGNED_WORD_MAXIMUM;
              else
                result = SIGNED_WORD_SIGN_BIT;
            }
        }
      projected_->x = (int32) result;
    }

  component = point_->y;
  if(component < 0)
    magnitude = 0 - (uint32) component;
  else
    magnitude = (uint32) component;
  product = (uint32) g_RENDER_BOUNDS.vertical_projection_scale * magnitude;
  if(depth <= (int32) (product >> PROJECTION_OVERFLOW_THRESHOLD_SHIFT))
    {
      if(component < 0)
        projected_->y = (int32) SIGNED_WORD_SIGN_BIT;
      else
        projected_->y = SIGNED_WORD_MAXIMUM;
    }
  else
    {
      projection = product / (uint32) depth;
      origin     = g_RENDER_BOUNDS.vertical_limit;
      if(component < 0)
        {
          result = origin + projection;
          if(((~(origin ^ projection) & (origin ^ result)) & SIGNED_WORD_SIGN_BIT) != 0)
            {
              if((result & SIGNED_WORD_SIGN_BIT) != 0)
                result = SIGNED_WORD_MAXIMUM;
              else
                result = SIGNED_WORD_SIGN_BIT;
            }
        }
      else
        {
          result = origin - projection;
          if((((origin ^ projection) & (origin ^ result)) & SIGNED_WORD_SIGN_BIT) != 0)
            {
              if((result & SIGNED_WORD_SIGN_BIT) != 0)
                result = SIGNED_WORD_MAXIMUM;
              else
                result = SIGNED_WORD_SIGN_BIT;
            }
        }
      projected_->y = (int32) result;
    }
}

void
project_point_with_depth_table(const Vector3 *point_, Point2 *projected_)
{
  int32 depth;
  int32 horizontal_product;
  int32 projection_shift;
  int32 scale;
  int32 vertical_product;

  projection_shift = DEPTH_SCALE_FRACTION_BITS;
  depth            = point_->z;
  if(depth >= DEPTH_SCALE_COARSE_THRESHOLD)
    {
      projection_shift += DEPTH_SCALE_COARSE_SHIFT;
      depth >>= DEPTH_SCALE_COARSE_SHIFT;
    }

  if(depth >= DEPTH_SCALE_FINE_THRESHOLD)
    {
      projection_shift += DEPTH_SCALE_FINE_SHIFT;
      depth >>= DEPTH_SCALE_FINE_SHIFT;
    }

  scale = (int32) g_RENDER_WORKSPACE_CURSOR[depth];
  /* ARM60 signed multiplication wraps to its low word before the arithmetic shift. */
  horizontal_product = point_->x * scale;
  vertical_product   = point_->y * scale;
  projected_->x = (g_RENDER_BOUNDS.horizontal_limit + (horizontal_product >> projection_shift));
  projected_->y = (g_RENDER_BOUNDS.vertical_limit - (vertical_product >> projection_shift));
}

void
project_vector3_array_with_depth_table(int32          vector_count_,
                                       const Vector3 *vectors_,
                                       Point2        *projected_,
                                       int32          projection_shift_)
{
  int32 depth;
  int32 horizontal_product;
  int32 projection_shift;
  int32 scale;
  int32 vertical_product;

  while(vector_count_ > 0)
    {
      depth            = vectors_->z;
      projection_shift = projection_shift_;

      if((uint32) depth >= ARRAY_DEPTH_COARSE_THRESHOLD)
        {
          projection_shift += ARRAY_DEPTH_COARSE_SHIFT;
          depth >>= ARRAY_DEPTH_COARSE_SHIFT;
        }

      if(depth >= ARRAY_DEPTH_MEDIUM_THRESHOLD)
        {
          projection_shift += ARRAY_DEPTH_MEDIUM_SHIFT;
          depth >>= ARRAY_DEPTH_MEDIUM_SHIFT;
        }

      if(depth >= ARRAY_DEPTH_FINE_THRESHOLD)
        {
          projection_shift += ARRAY_DEPTH_FINE_SHIFT;
          depth >>= ARRAY_DEPTH_FINE_SHIFT;
        }

      scale              = (int32) g_RENDER_WORKSPACE_CURSOR[depth];
      horizontal_product = (int32) ((uint32) vectors_->x * (uint32) scale);
      vertical_product   = (int32) ((uint32) vectors_->y * (uint32) scale);
      projected_->x = g_RENDER_BOUNDS.horizontal_limit + (horizontal_product >> projection_shift);
      projected_->y = g_RENDER_BOUNDS.vertical_limit - (vertical_product >> projection_shift);

      vectors_++;
      projected_++;
      vector_count_--;
    }
}

void
transform_project_vector3_array(int32            vector_count_,
                                const Matrix3x3 *matrix_,
                                const Vector3   *translation_,
                                const Vector3   *source_,
                                Vector3         *workspace_,
                                Point2          *projected_,
                                int32            projection_shift_)
{
  multiply_vector3_array_matrix3x3(vector_count_, source_, matrix_, workspace_);
  translate_vector3_array(vector_count_, workspace_, translation_, workspace_);
  project_vector3_array_with_depth_table(vector_count_, workspace_, projected_, projection_shift_);
}
