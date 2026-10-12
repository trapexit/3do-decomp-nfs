#include "frontend_transition.h"

#define FRONTEND_MENU_COORDINATE_FIXED16_SHIFT 16

FrontendMenuCoordinates g_FRONTEND_MENU_COORDINATES;

void
update_frontend_menu_coordinates(void)
{
  FrontendMenuItem *current_item;

  current_item                  = g_FRONTEND_RENDER_STATE.menu.current_item;
  g_FRONTEND_MENU_COORDINATES.x = current_item->coordinate_x
                               << FRONTEND_MENU_COORDINATE_FIXED16_SHIFT;
  g_FRONTEND_MENU_COORDINATES.y = current_item->coordinate_y
                               << FRONTEND_MENU_COORDINATE_FIXED16_SHIFT;
}
