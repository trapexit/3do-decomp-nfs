#pragma once

#include "types.h"

#include "stdarg.h"

typedef void  *(*RuntimeAllocateMemoryFunction)(const char *name_,
                                                uint32      byte_count_,
                                                uint32      flags_);
typedef int32  (*RuntimeFreeMemoryFunction)(void *memory_);
typedef void   (*RuntimeDebugPrintFunction)(const char *message_);
typedef void   (*RuntimeFormatStringFunction)(char       *destination_,
                                              const char *format_,
                                              va_list     arguments_);
typedef void  *(*RuntimeLocateShapeFunction)(const void   *shape_collection_,
                                             const uint32 *shape_name_);
typedef void  *(*RuntimeDrawShapeFunction)(const void *shape_);
typedef void   (*RuntimeDrawRectangleOutlineFunction)(int32  x_first_,
                                                      int32  y_first_,
                                                      int32  x_second_,
                                                      int32  y_second_,
                                                      uint32 color_);
typedef void  *(*RuntimeDrawTransformedShapeFunction)(const void *shape_,
                                                      int32       x_,
                                                      int32       y_,
                                                      int32       horizontal_dx_,
                                                      int32       horizontal_dy_,
                                                      int32       vertical_dx_,
                                                      int32       vertical_dy_,
                                                      int32       horizontal_ddx_,
                                                      int32       horizontal_ddy_);
typedef void  *(*RuntimeDrawShapeAtFunction)(const void *shape_, int32 x_, int32 y_);
typedef void   (*RuntimeDrawTextFunction)(const char *text_, int32 x_, int32 y_);
typedef void   (*RuntimeBlitShapeAtFunction)(const void *shape_, int32 x_, int32 y_);
typedef void  *(*RuntimeLoadFileDataFunction)(const char *path_, uint32 flags_);
typedef void   (*RuntimeInitializeFontFunction)(const void *font_data_);
typedef void   (*RuntimeSetFontColorsFunction)(uint32 foreground_color_, uint32 background_color_);
typedef void   (*RuntimeFrameFunction)(void);
typedef void   (*RuntimeDelayFramesFunction)(int32 frame_count_);
typedef uint32 (*RuntimeGetAnimationTickFunction)(void);
typedef int32  (*RuntimeGetElapsedAnimationTicksFunction)(uint32 previous_tick_);
typedef void  *(*RuntimeLoadShapeCollectionFunction)(const char *path_, uint32 flags_);
typedef void  *(*RuntimeDrawScaledCelFrameFunction)(int32       scale_,
                                                    const void *cel_header_,
                                                    int32       x_,
                                                    int32       y_);
typedef void  *(*RuntimeLoadFileWithOptionsFunction)(const char *path_, int32 options_);
typedef void   (*RuntimeOpenFileFunction)(const char *path_,
                                          int32      *handle_,
                                          void      **mapped_data_,
                                          uint32     *byte_count_);
typedef void   (*RuntimeCloseFileFunction)(int32 handle_);
typedef int32  (*RuntimeReadFileFunction)(int32 handle_, void *destination_, uint32 byte_count_);
typedef int32  (*RuntimeSaveFileFunction)(const char *path_,
                                          const void *source_,
                                          uint32      byte_count_);
typedef int32  (*RuntimeAngleFunction)(int32 angle_);
typedef int32  (*RuntimeMultiplyFixedFunction)(int32 left_, int32 right_);
typedef void  *(*RuntimeDrawCelHeaderToQuadFunction)(const void *cel_header_, const void *quad_);
typedef void  *(*RuntimeCreateVideoWindowFunction)(uint32 width_, uint32 height_, uint32 flags_);
typedef void   (*RuntimeSetVideoWindowFunction)(const void *window_);
typedef void   (*RuntimeDeleteVideoWindowFunction)(void *window_);
typedef void   (*RuntimeFadeDisplayClutFunction)(int32 legacy_start_,
                                                 int32 legacy_count_,
                                                 int32 frame_count_);
typedef void   (*RuntimeSetCcbPixelControlFunction)(uint32 pixel_control_);
typedef int32  (*RuntimeFileExistsFunction)(const char *path_);
typedef void   (
  *RuntimeFillRectangleFunction)(int32 x_, int32 y_, int32 width_, int32 height_, uint32 color_);
typedef uint32 (*RuntimeRandomFunction)(void);
typedef void   (*RuntimeShutdownLoaderFunction)(void);
typedef uint32 (*RuntimeGetFileSizeFunction)(const char *path_);
typedef void  *(*RuntimeLoadPointerArrayFunction)(const char *path_);
typedef void  *(*RuntimeGetPointerArrayElementFunction)(const void *array_, int32 index_);
typedef void  *(*RuntimeLoadCelFunction)(const char *path_, void *palette_);
typedef void   (*RuntimeUnloadCelFunction)(void *cel_);
typedef void  *(*RuntimeGetVideoWindowFunction)(void);
typedef int32  (*RuntimeGetVideoWindowIndexFunction)(void);
typedef void   (*RuntimeGetVideoConfigurationFunction)(void *configuration_);
typedef void   (*RuntimeSetVideoPaletteFunction)(void);
typedef uint32 (*RuntimeGetTickCountFunction)(void);
typedef void   (*RuntimeConfigureVideoWindowFunction)(int32 left_,
                                                      int32 top_,
                                                      int32 right_,
                                                      int32 bottom_);
typedef void   (*RuntimeInitializeControlCalibrationFunction)(void);
typedef void   (*RuntimeClearDisplayFunction)(uint32 color_);

#define RUNTIME_MEMORY_SERVICE_PREFIX_SIZE 0x44
#define RUNTIME_PRINT_BUFFER_SIZE 256

typedef struct RuntimeMemoryServices
{
  uint8                                       prefix[RUNTIME_MEMORY_SERVICE_PREFIX_SIZE];
  RuntimeAllocateMemoryFunction               allocate_memory;
  RuntimeFreeMemoryFunction                   free_memory;
  RuntimeFrameFunction                        prepare_display;
  RuntimeClearDisplayFunction                 clear_display;
  RuntimeFrameFunction                        wait_for_control_release;
  uint8                                       post_service54_services[0x04];
  RuntimeDebugPrintFunction                   debug_print;
  RuntimeFormatStringFunction                 format_string;
  RuntimeLocateShapeFunction                  locate_shape;
  RuntimeDrawShapeFunction                    draw_shape;
  RuntimeDrawRectangleOutlineFunction         draw_rectangle_outline;
  RuntimeDrawTransformedShapeFunction         draw_transformed_shape;
  RuntimeDrawShapeAtFunction                  draw_shape_at;
  RuntimeDrawTextFunction                     draw_text;
  RuntimeBlitShapeAtFunction                  blit_shape_at;
  RuntimeLoadFileDataFunction                 load_file_data;
  RuntimeInitializeFontFunction               initialize_font;
  RuntimeSetFontColorsFunction                set_font_colors;
  RuntimeFrameFunction                        begin_frame;
  RuntimeFrameFunction                        present_frame;
  RuntimeFrameFunction                        finish_frame;
  RuntimeFrameFunction                        synchronize_frame;
  uint8                                       post_frame_services[0x04];
  RuntimeDelayFramesFunction                  delay_frames;
  RuntimeGetAnimationTickFunction             get_animation_tick;
  RuntimeGetElapsedAnimationTicksFunction     get_elapsed_animation_ticks;
  RuntimeLoadShapeCollectionFunction          load_shape_collection;
  RuntimeBlitShapeAtFunction                  blit_shape_at_alias;
  RuntimeDrawScaledCelFrameFunction           draw_scaled_cel_frame;
  RuntimeLoadFileWithOptionsFunction          load_file_with_options;
  RuntimeOpenFileFunction                     open_file;
  RuntimeCloseFileFunction                    close_file;
  RuntimeReadFileFunction                     read_file;
  RuntimeSaveFileFunction                     save_file;
  RuntimeAngleFunction                        cosine;
  RuntimeAngleFunction                        sine;
  RuntimeMultiplyFixedFunction                multiply_fixed16;
  RuntimeDrawCelHeaderToQuadFunction          draw_cel_header_to_quad;
  RuntimeCreateVideoWindowFunction            create_video_window;
  RuntimeSetVideoWindowFunction               set_video_window;
  RuntimeDeleteVideoWindowFunction            delete_video_window;
  RuntimeFadeDisplayClutFunction              fade_display_clut_from_black;
  RuntimeFadeDisplayClutFunction              fade_display_clut_to_black;
  RuntimeSetCcbPixelControlFunction           set_ccb_pixel_control;
  RuntimeFileExistsFunction                   file_exists;
  RuntimeFillRectangleFunction                fill_rectangle;
  RuntimeRandomFunction                       random;
  RuntimeShutdownLoaderFunction               shutdown_loader;
  RuntimeGetFileSizeFunction                  get_file_size;
  RuntimeLoadPointerArrayFunction             load_pointer_array;
  RuntimeGetPointerArrayElementFunction       get_pointer_array_element;
  RuntimeLoadCelFunction                      load_cel;
  RuntimeUnloadCelFunction                    unload_cel;
  RuntimeGetVideoWindowFunction               get_pending_video_window;
  RuntimeGetVideoWindowFunction               get_displayed_video_window;
  RuntimeGetVideoWindowIndexFunction          get_pending_video_window_index;
  RuntimeGetVideoConfigurationFunction        get_video_configuration;
  RuntimeSetVideoPaletteFunction              set_grayscale_palette;
  RuntimeSetVideoPaletteFunction              set_gamma_grayscale_palette;
  RuntimeGetTickCountFunction                 get_tick_count;
  RuntimeConfigureVideoWindowFunction         configure_video_window;
  RuntimeInitializeControlCalibrationFunction initialize_control_calibration;
} RuntimeMemoryServices;

extern RuntimeMemoryServices *g_RUNTIME_MEMORY_SERVICES;

void *
allocate_runtime_memory(const char *name_, uint32 byte_count_, uint32 flags_);
int32
free_runtime_memory(void *memory_);
void
runtime_debug_printf(const char *format_, ...);
void
runtime_format_string(char *destination_, const char *format_, ...);
void *
locate_runtime_shape(const void *shape_collection_, const uint32 *shape_name_);
void *
draw_runtime_shape(const void *shape_);
void
draw_runtime_rectangle_outline(int32  x_first_,
                               int32  y_first_,
                               int32  x_second_,
                               int32  y_second_,
                               uint32 color_);
void *
draw_runtime_transformed_shape(const void *shape_,
                               int32       x_,
                               int32       y_,
                               int32       horizontal_dx_,
                               int32       horizontal_dy_,
                               int32       vertical_dx_,
                               int32       vertical_dy_,
                               int32       horizontal_ddx_,
                               int32       horizontal_ddy_);
void *
draw_runtime_shape_at(const void *shape_, int32 x_, int32 y_);
void
draw_runtime_text(const char *text_, int32 x_, int32 y_);
void
blit_runtime_shape_at(const void *shape_, int32 x_, int32 y_);
void
blit_runtime_named_shape_at(int32         x_,
                            int32         y_,
                            const void   *shape_collection_,
                            const uint32 *shape_name_);
void *
load_runtime_file_data(const char *path_, uint32 flags_);
void
initialize_runtime_font(const void *font_data_);
void
set_runtime_font_colors(uint32 foreground_color_, uint32 background_color_);
void
begin_runtime_frame(void);
void
present_runtime_frame(void);
void
finish_runtime_frame(void);
void
synchronize_runtime_frame(void);
void
delay_runtime_frames(int32 frame_count_);
uint32
get_runtime_animation_tick(void);
int32
get_runtime_elapsed_animation_ticks(uint32 previous_tick_);
void *
load_runtime_shape_collection(const char *path_, uint32 flags_);
void
blit_runtime_shape_at_alias(const void *shape_, int32 x_, int32 y_);
void *
draw_runtime_scaled_cel_frame(int32       scale_,
                              const void *cel_header_,
                              int32       x_,
                              int32       y_);
void *
load_runtime_file_with_options(const char *path_, int32 options_);
void
open_runtime_file(const char *path_, int32 *handle_, void **mapped_data_, uint32 *byte_count_);
void
close_runtime_file(int32 handle_);
int32
read_runtime_file(int32 handle_, void *destination_, uint32 byte_count_);
int32
save_runtime_file(const char *path_, const void *source_, uint32 byte_count_);
int32
runtime_cosine(int32 angle_);
int32
runtime_sine(int32 angle_);
int32
runtime_multiply_fixed16(int32 left_, int32 right_);
void *
draw_runtime_cel_header_to_quad(const void *cel_header_, const void *quad_);
void *
create_runtime_video_window(uint32 width_, uint32 height_, uint32 flags_);
void
set_runtime_video_window(const void *window_);
void
delete_runtime_video_window(void *window_);
void
fade_runtime_display_clut_from_black(int32 legacy_start_,
                                     int32 legacy_count_,
                                     int32 frame_count_);
void
fade_runtime_display_clut_to_black(int32 legacy_start_,
                                   int32 legacy_count_,
                                   int32 frame_count_);
void
set_runtime_ccb_pixel_control(uint32 pixel_control_);
int32
runtime_file_exists(const char *path_);
void
fill_runtime_rectangle(int32 x_, int32 y_, int32 width_, int32 height_, uint32 color_);
uint32
get_runtime_random(void);
void
shutdown_runtime_loader(void);
uint32
get_runtime_file_size(const char *path_);
void *
load_runtime_pointer_array(const char *path_);
void *
get_runtime_pointer_array_element(const void *array_, int32 index_);
void *
load_runtime_cel(const char *path_, void *palette_);
void
unload_runtime_cel(void *cel_);
void *
get_runtime_pending_video_window(void);
void *
get_runtime_displayed_video_window(void);
int32
get_runtime_pending_video_window_index(void);
void
get_runtime_video_configuration(void *configuration_);
void
set_runtime_grayscale_palette(void);
void
set_runtime_gamma_grayscale_palette(void);
uint32
get_runtime_tick_count(void);
void
configure_runtime_video_window(int32 left_, int32 top_, int32 right_, int32 bottom_);
void
initialize_runtime_control_calibration(void);
void
clear_runtime_display(uint32 color_);
void
wait_for_runtime_control_release(void);
void
prepare_runtime_display(void);
