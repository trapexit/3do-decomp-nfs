/* Tests primary-road cel quads against the active screen bounds. */

#include "frontend_transition.h"
#include "road_render.h"

#define PRIMARY_ROAD_QUAD_RENDER_TYPE_LIMIT 5
#define PRIMARY_ROAD_SCREEN_TOP 0
#define PRIMARY_ROAD_FIXED16_SHIFT 16
#define PRIMARY_ROAD_QUAD_FIRST_INDEX 0
#define PRIMARY_ROAD_QUAD_SECOND_INDEX 1
#define PRIMARY_ROAD_QUAD_THIRD_INDEX 2
#define PRIMARY_ROAD_QUAD_FOURTH_INDEX 3

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

COMPILE_TIME_SIZE_ASSERT(primary_road_triangle_index_word_size, sizeof(int32) == 4);

int32 g_PRIMARY_ROAD_FIRST_TRIANGLE_INDICES[3] = { 1, 2, 3 };

int32 g_PRIMARY_ROAD_SECOND_TRIANGLE_INDICES[3] = { 3, 0, 1 };

int32 g_PRIMARY_ROAD_WINDING_FIRST_TRIANGLE_INDICES[3] = { 1, 2, 3 };

int32 g_PRIMARY_ROAD_WINDING_SECOND_TRIANGLE_INDICES[3] = { 3, 0, 1 };

COMPILE_TIME_SIZE_ASSERT(primary_road_first_triangle_indices_size,
                         sizeof(g_PRIMARY_ROAD_FIRST_TRIANGLE_INDICES) == 0xC);
COMPILE_TIME_SIZE_ASSERT(primary_road_second_triangle_indices_size,
                         sizeof(g_PRIMARY_ROAD_SECOND_TRIANGLE_INDICES) == 0xC);
COMPILE_TIME_SIZE_ASSERT(primary_road_winding_first_triangle_indices_size,
                         sizeof(g_PRIMARY_ROAD_WINDING_FIRST_TRIANGLE_INDICES) == 0xC);
COMPILE_TIME_SIZE_ASSERT(primary_road_winding_second_triangle_indices_size,
                         sizeof(g_PRIMARY_ROAD_WINDING_SECOND_TRIANGLE_INDICES) == 0xC);

int32
is_primary_road_quad_visible_by_corners(const Point2 *top_left_,
                                        const Point2 *top_right_,
                                        const Point2 *bottom_right_,
                                        const Point2 *bottom_left_,
                                        int32         render_type_)
{
  int32 first_triangle;

  if(render_type_ < PRIMARY_ROAD_QUAD_RENDER_TYPE_LIMIT)
    {
      if((top_left_->x > g_FRONTEND_MENU_COORDINATES.x) &&
         (top_right_->x > g_FRONTEND_MENU_COORDINATES.x) &&
         (bottom_right_->x > g_FRONTEND_MENU_COORDINATES.x) &&
         (bottom_left_->x > g_FRONTEND_MENU_COORDINATES.x))
        return 0;
      if((top_left_->x < PRIMARY_ROAD_SCREEN_TOP) && (top_right_->x < PRIMARY_ROAD_SCREEN_TOP) &&
         (bottom_right_->x < PRIMARY_ROAD_SCREEN_TOP) &&
         (bottom_left_->x < PRIMARY_ROAD_SCREEN_TOP))
        return 0;
      if((top_left_->y > g_FRONTEND_MENU_COORDINATES.y) &&
         (bottom_left_->y > g_FRONTEND_MENU_COORDINATES.y))
        return 0;

      first_triangle = calculate_triangle_cross_product(top_right_,
                                                        bottom_right_,
                                                        bottom_left_,
                                                        PRIMARY_ROAD_FIXED16_SHIFT);
      if(first_triangle < 0)
        return 1;
      if(calculate_triangle_cross_product(bottom_left_,
                                          top_left_,
                                          top_right_,
                                          PRIMARY_ROAD_FIXED16_SHIFT) < 0)
        return 1;
      return 0;
    }

  if((top_left_->x > g_FRONTEND_MENU_COORDINATES.x) &&
     (top_right_->x > g_FRONTEND_MENU_COORDINATES.x) &&
     (bottom_right_->x > g_FRONTEND_MENU_COORDINATES.x) &&
     (bottom_left_->x > g_FRONTEND_MENU_COORDINATES.x))
    return 0;
  if((top_left_->x < PRIMARY_ROAD_SCREEN_TOP) && (top_right_->x < PRIMARY_ROAD_SCREEN_TOP) &&
     (bottom_right_->x < PRIMARY_ROAD_SCREEN_TOP) && (bottom_left_->x < PRIMARY_ROAD_SCREEN_TOP))
    return 0;
  if((top_left_->y > g_FRONTEND_MENU_COORDINATES.y) &&
     (bottom_left_->y > g_FRONTEND_MENU_COORDINATES.y))
    return 0;

  first_triangle = calculate_triangle_cross_product(top_right_,
                                                    bottom_right_,
                                                    bottom_left_,
                                                    PRIMARY_ROAD_FIXED16_SHIFT);
  if(first_triangle > 0)
    return 1;
  if(calculate_triangle_cross_product(bottom_left_,
                                      top_left_,
                                      top_right_,
                                      PRIMARY_ROAD_FIXED16_SHIFT) > 0)
    return 1;
  return 0;
}

int32
is_primary_road_quad_visible(const Point2 *quad_, int32 render_type_)
{
  int32 first_triangle;

  if(render_type_ < PRIMARY_ROAD_QUAD_RENDER_TYPE_LIMIT)
    {
      if((quad_[PRIMARY_ROAD_QUAD_FIRST_INDEX].x > g_FRONTEND_MENU_COORDINATES.x) &&
         (quad_[PRIMARY_ROAD_QUAD_SECOND_INDEX].x > g_FRONTEND_MENU_COORDINATES.x) &&
         (quad_[PRIMARY_ROAD_QUAD_THIRD_INDEX].x > g_FRONTEND_MENU_COORDINATES.x) &&
         (quad_[PRIMARY_ROAD_QUAD_FOURTH_INDEX].x > g_FRONTEND_MENU_COORDINATES.x))
        return 0;
      if((quad_[PRIMARY_ROAD_QUAD_FIRST_INDEX].x < PRIMARY_ROAD_SCREEN_TOP) &&
         (quad_[PRIMARY_ROAD_QUAD_SECOND_INDEX].x < PRIMARY_ROAD_SCREEN_TOP) &&
         (quad_[PRIMARY_ROAD_QUAD_THIRD_INDEX].x < PRIMARY_ROAD_SCREEN_TOP) &&
         (quad_[PRIMARY_ROAD_QUAD_FOURTH_INDEX].x < PRIMARY_ROAD_SCREEN_TOP))
        return 0;
      if((quad_[PRIMARY_ROAD_QUAD_FIRST_INDEX].y > g_FRONTEND_MENU_COORDINATES.y) &&
         (quad_[PRIMARY_ROAD_QUAD_FOURTH_INDEX].y > g_FRONTEND_MENU_COORDINATES.y))
        return 0;

      first_triangle =
        calculate_indexed_triangle_cross_product(quad_,
                                                 g_PRIMARY_ROAD_FIRST_TRIANGLE_INDICES,
                                                 PRIMARY_ROAD_FIXED16_SHIFT);
      if(first_triangle < 0)
        return 1;
      if(calculate_indexed_triangle_cross_product(quad_,
                                                  g_PRIMARY_ROAD_SECOND_TRIANGLE_INDICES,
                                                  PRIMARY_ROAD_FIXED16_SHIFT) < 0)
        return 1;
      return 0;
    }

  if((quad_[PRIMARY_ROAD_QUAD_FIRST_INDEX].x < PRIMARY_ROAD_SCREEN_TOP) &&
     (quad_[PRIMARY_ROAD_QUAD_SECOND_INDEX].x < PRIMARY_ROAD_SCREEN_TOP) &&
     (quad_[PRIMARY_ROAD_QUAD_THIRD_INDEX].x < PRIMARY_ROAD_SCREEN_TOP) &&
     (quad_[PRIMARY_ROAD_QUAD_FOURTH_INDEX].x < PRIMARY_ROAD_SCREEN_TOP))
    return 0;
  if((quad_[PRIMARY_ROAD_QUAD_FIRST_INDEX].x > g_FRONTEND_MENU_COORDINATES.x) &&
     (quad_[PRIMARY_ROAD_QUAD_SECOND_INDEX].x > g_FRONTEND_MENU_COORDINATES.x) &&
     (quad_[PRIMARY_ROAD_QUAD_THIRD_INDEX].x > g_FRONTEND_MENU_COORDINATES.x) &&
     (quad_[PRIMARY_ROAD_QUAD_FOURTH_INDEX].x > g_FRONTEND_MENU_COORDINATES.x))
    return 0;
  if((quad_[PRIMARY_ROAD_QUAD_FIRST_INDEX].y > g_FRONTEND_MENU_COORDINATES.y) &&
     (quad_[PRIMARY_ROAD_QUAD_FOURTH_INDEX].y > g_FRONTEND_MENU_COORDINATES.y))
    return 0;

  first_triangle = calculate_indexed_triangle_cross_product(quad_,
                                                            g_PRIMARY_ROAD_FIRST_TRIANGLE_INDICES,
                                                            PRIMARY_ROAD_FIXED16_SHIFT);
  if(first_triangle > 0)
    return 1;
  if(calculate_indexed_triangle_cross_product(quad_,
                                              g_PRIMARY_ROAD_SECOND_TRIANGLE_INDICES,
                                              PRIMARY_ROAD_FIXED16_SHIFT) > 0)
    return 1;
  return 0;
}

int32
is_primary_road_quad_winding_visible(const Point2 *quad_, int32 render_type_)
{
  int32 first_triangle;

  first_triangle =
    calculate_indexed_triangle_cross_product(quad_,
                                             g_PRIMARY_ROAD_WINDING_FIRST_TRIANGLE_INDICES,
                                             PRIMARY_ROAD_FIXED16_SHIFT);
  if(render_type_ < PRIMARY_ROAD_QUAD_RENDER_TYPE_LIMIT)
    {
      if((first_triangle >= 0) &&
         (calculate_indexed_triangle_cross_product(quad_,
                                                   g_PRIMARY_ROAD_WINDING_SECOND_TRIANGLE_INDICES,
                                                   PRIMARY_ROAD_FIXED16_SHIFT) >= 0))
        return 0;
      return 1;
    }

  if((first_triangle <= 0) &&
     (calculate_indexed_triangle_cross_product(quad_,
                                               g_PRIMARY_ROAD_WINDING_SECOND_TRIANGLE_INDICES,
                                               PRIMARY_ROAD_FIXED16_SHIFT) <= 0))
    return 0;
  return 1;
}
