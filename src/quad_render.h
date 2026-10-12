#pragma once

#include "ccb_manager.h"
#include "geometry.h"

#include "types.h"

void
draw_solid_quad(uint32 color_,
                int32  x0_,
                int32  y0_,
                int32  x1_,
                int32  y1_,
                int32  x2_,
                int32  y2_,
                int32  x3_,
                int32  y3_);
RenderCcb *
draw_solid_quad_points(uint32 color_, const Point2 *points_);
RenderCcb *
draw_indexed_solid_quad(uint32 color_, const Point2 *points_, const int32 *indices_);
RenderCcb *
draw_indexed_transparent_quad(uint32        color_,
                              const Point2 *points_,
                              int32         top_left_index_,
                              int32         top_right_index_,
                              int32         bottom_right_index_,
                              int32         bottom_left_index_);
void
draw_solid_polygon(uint32 color_, int32 point_count_, const Point2 *points_);
void
draw_solid_polygon_coordinates(uint32 color_, int32 point_count_, ...);
void
draw_solid_polygon_coordinates_legacy(uint32 color_, int32 point_count_, ...);
void
draw_solid_line_segment(int32  start_x_,
                        int32  start_y_,
                        int32  end_x_,
                        int32  end_y_,
                        uint32 color_,
                        int32  thickness_);
