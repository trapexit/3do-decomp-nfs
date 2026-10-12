#pragma once

#include "display_buffer_state.h"
#include "menu_layout.h"

#include "types.h"

typedef struct PauseGameOptionSelector
{
  uint8 reserved_00[0x24];
  int32 selected_value;
  int32 selected_index;
  int32 options[3];
} PauseGameOptionSelector;

extern PauseGameOptionSelector  g_PAUSE_GAME_OPTION_SELECTORS[1];
extern MenuRectangle            g_PAUSE_GAME_MENU_RECTANGLES[1];
extern int32                    g_PAUSE_GAME_MENU_GRID_FLAGS[1];
extern MenuViewport             g_PAUSE_GAME_MENU_VIEWPORT;
extern DisplayBufferDescriptor *g_PAUSE_GAME_DISPLAY_DESCRIPTOR;
extern DisplayBufferDescriptor *g_PAUSE_GAME_SECONDARY_DISPLAY_DESCRIPTOR;
extern int32                    g_PAUSE_GAME_SELECTION_Y[];
extern int32                    g_PAUSE_GAME_SELECTION_X[];
extern int32                    g_PAUSE_GAME_VALUE_X[];
extern int32                    g_PAUSE_GAME_VALUE_Y[];

void
initialize_pause_game_display(int32 coordinate_, int32 value_);
void
update_pause_game_menu_viewport(MenuViewport *viewport_);
void
render_pause_game_track_map(const MenuViewport  *viewport_,
                            const MenuRectangle *rectangle_,
                            const MenuScale     *scale_,
                            uint32               color_);
void
render_pause_game_vehicles(const MenuViewport  *viewport_,
                           const MenuRectangle *rectangle_,
                           const MenuScale     *scale_);
void
render_pause_game_menu_contents(void);
void
render_pause_game_menu_initial(void);
void
render_pause_game_menu_selection_change(void);
void
render_pause_game_menu_selection(int32 selected_option_);
void
render_pause_game_menu_value(int32 value_);
int32
run_frontend_menu(void);
