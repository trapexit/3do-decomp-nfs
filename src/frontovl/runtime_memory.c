#include "runtime_memory.h"

RuntimeMemoryServices *g_RUNTIME_MEMORY_SERVICES      = NULL;
uint32                 g_RUNTIME_MEMORY_LINKAGE_STATE = 0;

void *
allocate_runtime_memory(const char *name_, uint32 byte_count_, uint32 flags_)
{
  return g_RUNTIME_MEMORY_SERVICES->allocate_memory(name_, byte_count_, flags_);
}

int32
free_runtime_memory(void *memory_)
{
  return g_RUNTIME_MEMORY_SERVICES->free_memory(memory_);
}

void
prepare_runtime_display(void)
{
  g_RUNTIME_MEMORY_SERVICES->prepare_display();
}

void
runtime_debug_printf(const char *format_, ...)
{
  char    message[RUNTIME_PRINT_BUFFER_SIZE];
  va_list arguments;

  va_start(arguments, format_);
  g_RUNTIME_MEMORY_SERVICES->format_string(message, format_, arguments);
  va_end(arguments);
  g_RUNTIME_MEMORY_SERVICES->debug_print(message);
}

void
runtime_format_string(char *destination_, const char *format_, ...)
{
  va_list arguments;

  va_start(arguments, format_);
  g_RUNTIME_MEMORY_SERVICES->format_string(destination_, format_, arguments);
  va_end(arguments);
}

void *
locate_runtime_shape(const void *shape_collection_, const uint32 *shape_name_)
{
  return g_RUNTIME_MEMORY_SERVICES->locate_shape(shape_collection_, shape_name_);
}

void *
draw_runtime_shape(const void *shape_)
{
  return g_RUNTIME_MEMORY_SERVICES->draw_shape(shape_);
}

void
draw_runtime_rectangle_outline(int32  x_first_,
                               int32  y_first_,
                               int32  x_second_,
                               int32  y_second_,
                               uint32 color_)
{
  g_RUNTIME_MEMORY_SERVICES->draw_rectangle_outline(x_first_,
                                                    y_first_,
                                                    x_second_,
                                                    y_second_,
                                                    color_);
}

void *
draw_runtime_transformed_shape(const void *shape_,
                               int32       x_,
                               int32       y_,
                               int32       horizontal_dx_,
                               int32       horizontal_dy_,
                               int32       vertical_dx_,
                               int32       vertical_dy_,
                               int32       horizontal_ddx_,
                               int32       horizontal_ddy_)
{
  return g_RUNTIME_MEMORY_SERVICES->draw_transformed_shape(shape_,
                                                           x_,
                                                           y_,
                                                           horizontal_dx_,
                                                           horizontal_dy_,
                                                           vertical_dx_,
                                                           vertical_dy_,
                                                           horizontal_ddx_,
                                                           horizontal_ddy_);
}

void *
draw_runtime_shape_at(const void *shape_, int32 x_, int32 y_)
{
  return g_RUNTIME_MEMORY_SERVICES->draw_shape_at(shape_, x_, y_);
}

void
draw_runtime_text(const char *text_, int32 x_, int32 y_)
{
  g_RUNTIME_MEMORY_SERVICES->draw_text(text_, x_, y_);
}

void
blit_runtime_shape_at(const void *shape_, int32 x_, int32 y_)
{
  g_RUNTIME_MEMORY_SERVICES->blit_shape_at(shape_, x_, y_);
}

void
blit_runtime_named_shape_at(int32         x_,
                            int32         y_,
                            const void   *shape_collection_,
                            const uint32 *shape_name_)
{
  void *shape;

  shape = locate_runtime_shape(shape_collection_, shape_name_);
  blit_runtime_shape_at(shape, x_, y_);
}

void *
load_runtime_file_data(const char *path_, uint32 flags_)
{
  return g_RUNTIME_MEMORY_SERVICES->load_file_data(path_, flags_);
}

void
initialize_runtime_font(const void *font_data_)
{
  g_RUNTIME_MEMORY_SERVICES->initialize_font(font_data_);
}

void
set_runtime_font_colors(uint32 foreground_color_, uint32 background_color_)
{
  g_RUNTIME_MEMORY_SERVICES->set_font_colors(foreground_color_, background_color_);
}

void
begin_runtime_frame(void)
{
  g_RUNTIME_MEMORY_SERVICES->begin_frame();
}

void
present_runtime_frame(void)
{
  g_RUNTIME_MEMORY_SERVICES->present_frame();
}

void
finish_runtime_frame(void)
{
  g_RUNTIME_MEMORY_SERVICES->finish_frame();
}

void
synchronize_runtime_frame(void)
{
  g_RUNTIME_MEMORY_SERVICES->synchronize_frame();
}

void
delay_runtime_frames(int32 frame_count_)
{
  g_RUNTIME_MEMORY_SERVICES->delay_frames(frame_count_);
}

uint32
get_runtime_animation_tick(void)
{
  return g_RUNTIME_MEMORY_SERVICES->get_animation_tick();
}

int32
get_runtime_elapsed_animation_ticks(uint32 previous_tick_)
{
  return g_RUNTIME_MEMORY_SERVICES->get_elapsed_animation_ticks(previous_tick_);
}

void *
load_runtime_shape_collection(const char *path_, uint32 flags_)
{
  return g_RUNTIME_MEMORY_SERVICES->load_shape_collection(path_, flags_);
}


void
blit_runtime_shape_at_alias(const void *shape_, int32 x_, int32 y_)
{
  g_RUNTIME_MEMORY_SERVICES->blit_shape_at_alias(shape_, x_, y_);
}


void *
draw_runtime_scaled_cel_frame(int32       scale_,
                              const void *cel_header_,
                              int32       x_,
                              int32       y_)
{
  return g_RUNTIME_MEMORY_SERVICES->draw_scaled_cel_frame(scale_, cel_header_, x_, y_);
}


void *
load_runtime_file_with_options(const char *path_, int32 options_)
{
  return g_RUNTIME_MEMORY_SERVICES->load_file_with_options(path_, options_);
}


void
open_runtime_file(const char *path_, int32 *handle_, void **mapped_data_, uint32 *byte_count_)
{
  g_RUNTIME_MEMORY_SERVICES->open_file(path_, handle_, mapped_data_, byte_count_);
}

void
close_runtime_file(int32 handle_)
{
  g_RUNTIME_MEMORY_SERVICES->close_file(handle_);
}

int32
read_runtime_file(int32 handle_, void *destination_, uint32 byte_count_)
{
  return g_RUNTIME_MEMORY_SERVICES->read_file(handle_, destination_, byte_count_);
}

int32
save_runtime_file(const char *path_, const void *source_, uint32 byte_count_)
{
  return g_RUNTIME_MEMORY_SERVICES->save_file(path_, source_, byte_count_);
}

int32
runtime_cosine(int32 angle_)
{
  return g_RUNTIME_MEMORY_SERVICES->cosine(angle_);
}

int32
runtime_sine(int32 angle_)
{
  return g_RUNTIME_MEMORY_SERVICES->sine(angle_);
}

int32
runtime_multiply_fixed16(int32 left_, int32 right_)
{
  return g_RUNTIME_MEMORY_SERVICES->multiply_fixed16(left_, right_);
}


void *
draw_runtime_cel_header_to_quad(const void *cel_header_, const void *quad_)
{
  return g_RUNTIME_MEMORY_SERVICES->draw_cel_header_to_quad(cel_header_, quad_);
}


void *
create_runtime_video_window(uint32 width_, uint32 height_, uint32 flags_)
{
  return g_RUNTIME_MEMORY_SERVICES->create_video_window(width_, height_, flags_);
}

void
set_runtime_video_window(const void *window_)
{
  g_RUNTIME_MEMORY_SERVICES->set_video_window(window_);
}

void
delete_runtime_video_window(void *window_)
{
  g_RUNTIME_MEMORY_SERVICES->delete_video_window(window_);
}


void
fade_runtime_display_clut_from_black(int32 legacy_start_,
                                     int32 legacy_count_,
                                     int32 frame_count_)
{
  g_RUNTIME_MEMORY_SERVICES->fade_display_clut_from_black(legacy_start_,
                                                          legacy_count_,
                                                          frame_count_);
}


void
fade_runtime_display_clut_to_black(int32 legacy_start_,
                                   int32 legacy_count_,
                                   int32 frame_count_)
{
  g_RUNTIME_MEMORY_SERVICES->fade_display_clut_to_black(legacy_start_,
                                                        legacy_count_,
                                                        frame_count_);
}


void
set_runtime_ccb_pixel_control(uint32 pixel_control_)
{
  g_RUNTIME_MEMORY_SERVICES->set_ccb_pixel_control(pixel_control_);
}


int32
runtime_file_exists(const char *path_)
{
  return g_RUNTIME_MEMORY_SERVICES->file_exists(path_);
}

void
fill_runtime_rectangle(int32 x_, int32 y_, int32 width_, int32 height_, uint32 color_)
{
  g_RUNTIME_MEMORY_SERVICES->fill_rectangle(x_, y_, width_, height_, color_);
}

uint32
get_runtime_random(void)
{
  return g_RUNTIME_MEMORY_SERVICES->random();
}

void
shutdown_runtime_loader(void)
{
  g_RUNTIME_MEMORY_SERVICES->shutdown_loader();
}

uint32
get_runtime_file_size(const char *path_)
{
  return g_RUNTIME_MEMORY_SERVICES->get_file_size(path_);
}

void *
load_runtime_pointer_array(const char *path_)
{
  return g_RUNTIME_MEMORY_SERVICES->load_pointer_array(path_);
}

void *
get_runtime_pointer_array_element(const void *array_, int32 index_)
{
  return g_RUNTIME_MEMORY_SERVICES->get_pointer_array_element(array_, index_);
}

void *
load_runtime_cel(const char *path_, void *palette_)
{
  return g_RUNTIME_MEMORY_SERVICES->load_cel(path_, palette_);
}

void
unload_runtime_cel(void *cel_)
{
  g_RUNTIME_MEMORY_SERVICES->unload_cel(cel_);
}

void *
get_runtime_pending_video_window(void)
{
  return g_RUNTIME_MEMORY_SERVICES->get_pending_video_window();
}

void *
get_runtime_displayed_video_window(void)
{
  return g_RUNTIME_MEMORY_SERVICES->get_displayed_video_window();
}

int32
get_runtime_pending_video_window_index(void)
{
  return g_RUNTIME_MEMORY_SERVICES->get_pending_video_window_index();
}

void
get_runtime_video_configuration(void *configuration_)
{
  g_RUNTIME_MEMORY_SERVICES->get_video_configuration(configuration_);
}

void
set_runtime_grayscale_palette(void)
{
  g_RUNTIME_MEMORY_SERVICES->set_grayscale_palette();
}

void
set_runtime_gamma_grayscale_palette(void)
{
  g_RUNTIME_MEMORY_SERVICES->set_gamma_grayscale_palette();
}

uint32
get_runtime_tick_count(void)
{
  return g_RUNTIME_MEMORY_SERVICES->get_tick_count();
}

void
configure_runtime_video_window(int32 left_, int32 top_, int32 right_, int32 bottom_)
{
  g_RUNTIME_MEMORY_SERVICES->configure_video_window(left_, top_, right_, bottom_);
}

void
initialize_runtime_control_calibration(void)
{
  g_RUNTIME_MEMORY_SERVICES->initialize_control_calibration();
}

void
clear_runtime_display(uint32 color_)
{
  g_RUNTIME_MEMORY_SERVICES->clear_display(color_);
}

void
wait_for_runtime_control_release(void)
{
  g_RUNTIME_MEMORY_SERVICES->wait_for_control_release();
}
