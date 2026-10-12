#pragma once

#include "ccb_manager.h"
#include "geometry.h"

#include "graphics.h"

typedef void (*BitmapCopyCallback)(const uint8 *source_, uint8 *destination_, uint32 byte_count_);

extern BitmapCopyCallback g_BITMAP_COPY_CALLBACK;

void
draw_cel_header_pixels_at_embedded_position(const uint32 *cel_header_);
void
draw_cel_header_pixels_relative_to_embedded_origin(const uint32 *cel_header_, int32 x_, int32 y_);
void
draw_cel_header_pixels(const uint32 *cel_header_, int32 x_, int32 y_);
void *
draw_cel_header(const uint32 *cel_header_, int32 x_, int32 y_);
void *
draw_cel_header_at_embedded_position(const uint32 *cel_header_);
void *
draw_cel_header_relative_to_embedded_origin(const uint32 *cel_header_, int32 x_, int32 y_);
void *
draw_cel_header_alias(const uint32 *cel_header_, int32 x_, int32 y_);
void *
draw_cel_header_at_embedded_position_alias(const uint32 *cel_header_);
void *
draw_cel_header_relative_to_embedded_origin_alias(const uint32 *cel_header_, int32 x_, int32 y_);
void *
draw_unclipped_cel_header(const uint32 *cel_header_, int32 x_, int32 y_);
void *
draw_unclipped_cel_header_at_embedded_position(const uint32 *cel_header_);
void *
draw_unclipped_cel_header_relative_to_embedded_origin(const uint32 *cel_header_,
                                                      int32         x_,
                                                      int32         y_);
void *
draw_unclipped_cel_header_alias(const uint32 *cel_header_, int32 x_, int32 y_);
void *
draw_unclipped_cel_header_at_embedded_position_alias(const uint32 *cel_header_);
void *
draw_unclipped_cel_header_relative_to_embedded_origin_alias(const uint32 *cel_header_,
                                                            int32         x_,
                                                            int32         y_);
RenderCcb *
draw_scaled_cel_frame(int32 scale_, const uint32 *cel_header_, int32 x_, int32 y_);
RenderCcb *
draw_scaled_cel_frame_at_embedded_position(int32 scale_, const uint32 *cel_header_);
RenderCcb *
draw_scaled_cel_frame_relative_to_embedded_origin(int32         scale_,
                                                  const uint32 *cel_header_,
                                                  int32         x_,
                                                  int32         y_);
RenderCcb *
draw_scaled_cel_frame_alias(int32 scale_, const uint32 *cel_header_, int32 x_, int32 y_);
RenderCcb *
draw_scaled_cel_frame_at_embedded_position_alias(int32 scale_, const uint32 *cel_header_);
RenderCcb *
draw_scaled_cel_frame_relative_to_embedded_origin_alias(int32         scale_,
                                                        const uint32 *cel_header_,
                                                        int32         x_,
                                                        int32         y_);
RenderCcb *
draw_scaled_frame(int32 scale_, const uint32 *cel_header_, int32 x_, int32 y_);
RenderCcb *
draw_scaled_frame_at_embedded_position(int32 scale_, const uint32 *cel_header_);
RenderCcb *
draw_scaled_frame_relative_to_embedded_origin(int32         scale_,
                                              const uint32 *cel_header_,
                                              int32         x_,
                                              int32         y_);
RenderCcb *
draw_scaled_frame_alias(int32 scale_, const uint32 *cel_header_, int32 x_, int32 y_);
RenderCcb *
draw_scaled_frame_at_embedded_position_alias(int32 scale_, const uint32 *cel_header_);
RenderCcb *
draw_scaled_frame_relative_to_embedded_origin_alias(int32         scale_,
                                                    const uint32 *cel_header_,
                                                    int32         x_,
                                                    int32         y_);
void
map_fixed16_quad_to_sized_ccb(RenderCcb *ccb_, const Point2 *quad_);
void
draw_transformed_ccb(CCB  *source_ccb_,
                     int32 x_,
                     int32 y_,
                     int32 half_width_,
                     int32 half_height_,
                     int32 rotation_,
                     int32 horizontal_scale_,
                     int32 vertical_scale_);
RenderCcb *
draw_transformed_cel_header(const uint32 *cel_header_,
                            int32         x_,
                            int32         y_,
                            int32         horizontal_dx_,
                            int32         horizontal_dy_,
                            int32         vertical_dx_,
                            int32         vertical_dy_,
                            int32         horizontal_ddx_,
                            int32         horizontal_ddy_);
RenderCcb *
draw_cel_header_to_quad_coordinates(const uint32 *cel_header_,
                                    int32         top_left_x_,
                                    int32         top_left_y_,
                                    int32         top_right_x_,
                                    int32         top_right_y_,
                                    int32         bottom_right_x_,
                                    int32         bottom_right_y_,
                                    int32         bottom_left_x_,
                                    int32         bottom_left_y_);
RenderCcb *
draw_cel_header_to_quad(const uint32 *cel_header_, const Point *quad_);
void
draw_background_cel_header_to_quad(const uint32 *cel_header_, const Point *quad_);
RenderCcb *
draw_background_cel_header_to_indexed_quad(const uint32 *cel_header_,
                                           const Point  *points_,
                                           const int32  *indices_);
RenderCcb *
draw_cel_header_to_indexed_quad(const uint32 *cel_header_,
                                const Point  *points_,
                                const int32  *indices_);
