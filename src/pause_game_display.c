#include "pause_game_display.h"

#include "frontend_control_input.h"
#include "menu_layout.h"
#include "menu_option_selector.h"
#include "pause_game_shape.h"
#include "shape_lookup.h"

#include "stddef.h"

#define PAUSE_GAME_DISPLAY_ENTRY_COUNT 1
#define PAUSE_GAME_SHAPE_POSITION_OFFSET 0x0C
#define PAUSE_GAME_LEFT_OFFSET 150
#define PAUSE_GAME_TOP_OFFSET 11
#define PAUSE_GAME_RIGHT_OFFSET 243
#define PAUSE_GAME_BOTTOM_OFFSET 125
#define PAUSE_GAME_OPTION_PRIMARY_SCALE 0xC000
#define PAUSE_GAME_OPTION_SECONDARY_SCALE 0x10000
#define PAUSE_GAME_PACKED_POSITION_SHIFT 16
#define PAUSE_GAME_OPTION_DISABLED 0
#define PAUSE_GAME_OPTION_PRIMARY 1
#define PAUSE_GAME_OPTION_SECONDARY 2

MenuRectangle           g_PAUSE_GAME_MENU_RECTANGLES[PAUSE_GAME_DISPLAY_ENTRY_COUNT];
PauseGameOptionSelector g_PAUSE_GAME_OPTION_SELECTORS[PAUSE_GAME_DISPLAY_ENTRY_COUNT];
int32                   g_PAUSE_GAME_MENU_GRID_FLAGS[PAUSE_GAME_DISPLAY_ENTRY_COUNT];
MenuViewport            g_PAUSE_GAME_MENU_VIEWPORT;
uint8                   g_PAUSE_GAME_DISPLAY_RESERVED_STATE[0x20];

typedef char SizeAssertPauseGameOptionSelector[(sizeof(PauseGameOptionSelector) == 0x38) ? 1 : -1];
typedef char SizeAssertPauseGameMenuRectangle[(sizeof(MenuRectangle) == 0x2C) ? 1 : -1];
typedef char SizeAssertPauseGameMenuViewport[(sizeof(MenuViewport) == 0x28) ? 1 : -1];

void
initialize_pause_game_display(int32 coordinate_, int32 value_)
{
  MenuRectangle           *rectangle;
  PauseGameOptionSelector *selector;
  const uint8             *shape;
  uint32                   packed_position;
  int32                    horizontal_position;
  int32                    vertical_position;
  int32                    index;

  shape               = (const uint8 *) require_shape(G_PAUSE_GAME_SHAPE, (const uint32 *) "menu");
  packed_position     = *(const uint32 *) (shape + PAUSE_GAME_SHAPE_POSITION_OFFSET);
  horizontal_position = (int32) packed_position >> PAUSE_GAME_PACKED_POSITION_SHIFT;
  vertical_position   = (int32) (int16) packed_position;

  rectangle         = &g_PAUSE_GAME_MENU_RECTANGLES[0];
  rectangle->left   = PAUSE_GAME_LEFT_OFFSET + horizontal_position;
  rectangle->top    = PAUSE_GAME_TOP_OFFSET + vertical_position;
  rectangle->right  = PAUSE_GAME_RIGHT_OFFSET + horizontal_position;
  rectangle->bottom = PAUSE_GAME_BOTTOM_OFFSET + vertical_position;

  selector                                       = &g_PAUSE_GAME_OPTION_SELECTORS[0];
  selector->selected_index                       = 0;
  selector->options[PAUSE_GAME_OPTION_DISABLED]  = 0;
  selector->options[PAUSE_GAME_OPTION_PRIMARY]   = PAUSE_GAME_OPTION_PRIMARY_SCALE;
  selector->options[PAUSE_GAME_OPTION_SECONDARY] = PAUSE_GAME_OPTION_SECONDARY_SCALE;
  initialize_menu_viewport(&g_PAUSE_GAME_MENU_VIEWPORT, coordinate_, value_);

  for(index = 0; index < PAUSE_GAME_DISPLAY_ENTRY_COUNT; index++)
    {
      update_menu_rectangle_metrics(&g_PAUSE_GAME_MENU_RECTANGLES[index]);
      reset_menu_option_selector((MenuOptionSelector *) &g_PAUSE_GAME_OPTION_SELECTORS[index]);
    }
}
