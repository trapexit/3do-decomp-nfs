#pragma once

#include "geometry.h"

#include "types.h"

#define RENDER_DEPTH_SCALE_TABLE_COUNT 1024

typedef struct RenderBounds
{
  uint32 horizontal_half_extent;
  uint32 vertical_half_extent;
  uint32 horizontal_origin;
  uint32 vertical_origin;
  uint32 horizontal_limit;
  uint32 vertical_limit;
  int32  horizontal_projection_scale;
  int32  vertical_projection_scale;
  int32  horizontal_view_angle;
  int32  vertical_view_angle;
} RenderBounds;

typedef struct LegacyProjectionState
{
  int32 projection_scale;
  int32 horizontal_center;
  int32 vertical_center;
} LegacyProjectionState;

extern RenderBounds          g_RENDER_BOUNDS;
extern LegacyProjectionState g_LEGACY_PROJECTION_STATE;
extern uint32                g_RENDER_WORKSPACE[RENDER_DEPTH_SCALE_TABLE_COUNT];
extern uint32               *g_RENDER_WORKSPACE_CURSOR;

void
configure_render_bounds(uint32 horizontal_origin_, uint32 vertical_origin_);
void
configure_render_projection_angles(uint32 horizontal_angle_,
                                   uint32 vertical_angle_,
                                   uint32 width_,
                                   uint32 height_);
void
configure_render_projection_degrees(uint32 horizontal_degrees_,
                                    uint32 vertical_degrees_,
                                    uint32 width_,
                                    uint32 height_);
void
configure_legacy_projection(int32 angle_degrees_, int32 reserved_, int32 projection_extent_);
void
project_legacy_point(const Vector3 *point_, Point2 *projected_);
uint32 *
set_render_workspace_cursor(uint32 *workspace_cursor_);
void
initialize_depth_scale_table(void);
void
project_point_with_division(const Vector3 *point_, Point2 *projected_);
void
project_point_saturating(const Vector3 *point_, Point2 *projected_);
void
project_point_with_depth_table(const Vector3 *point_, Point2 *projected_);
void
project_vector3_array_with_depth_table(int32          vector_count_,
                                       const Vector3 *vectors_,
                                       Point2        *projected_,
                                       int32          projection_shift_);
void
transform_project_vector3_array(int32            vector_count_,
                                const Matrix3x3 *matrix_,
                                const Vector3   *translation_,
                                const Vector3   *source_,
                                Vector3         *workspace_,
                                Point2          *projected_,
                                int32            projection_shift_);
