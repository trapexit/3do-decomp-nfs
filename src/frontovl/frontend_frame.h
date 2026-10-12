#pragma once

#include "types.h"

extern int32 g_FRONTEND_DISPLAY_X;
extern int32 g_FRONTEND_DISPLAY_Y;
extern void *g_FRONTEND_DISPLAY_RESOURCE;
extern int32 g_FRONTEND_PRESENTATION_ENABLED;
extern int32 g_FRONTEND_GAMMA_PALETTE_ENABLED;

void
present_frontend_frame(int32 display_mode_);
void
present_frontend_host_frame(int32 display_mode_);
void
present_frontend_attract_frame(int32 display_mode_);
void
present_frontend_overlay_frame(void *foreground_shape_);
void
draw_frontend_shape_at_display_position(void *shape_);
void
release_frontend_display_resource(void);
void
release_frontend_display(void);
void
initialize_frontend_stamp(void);
void
draw_frontend_stamp(void);
uint32
get_frontend_elapsed_ticks(void);
void
initialize_frontend_display(void);
int32
present_frontend_scene(int32              shape_count_,
                       const char *const *shape_names_,
                       int32              flags_,
                       const char        *fallback_path_,
                       const char        *fallback_shape_,
                       int32              suspend_music_,
                       int32              movie_mode_,
                       ...);
int32
present_frontend_host(const char *cue_code_,
                      void        (*frame_callback_)(int32),
                      const char *shape_path_,
                      const char *background_shape_,
                      int32       suspend_music_,
                      int32       skip_host_shapes_);
void *
display_frontend_shape(const char *shape_path_, const char *shape_name_);
int32
display_frontend_splash(const char *shape_path_, const char *shape_name_);
int32
display_frontend_title(const char *shape_path_, const char *shape_name_, int32 duration_);
int32
run_frontend_credits(int32 restart_music_);
int32
draw_finish_menu_frame(int32 selection_index_);
int32
run_finish_menu(int32 timeout_seconds_);
