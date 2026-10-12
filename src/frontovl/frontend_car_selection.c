#include "frontend_car_selection.h"

#include "frontend_frame.h"
#include "frontend_vehicle_selection.h"

#define FRONTEND_CAR_SCENE_SHAPE_COUNT 1
#define FRONTEND_CAR_SCENE_PRIMARY_ENABLED 1
#define FRONTEND_CAR_SCENE_SECONDARY_ENABLED 1
#define FRONTEND_CAR_SCENE_DURATION 20

int32
display_selected_car_scene(int32 display_mode_, int32 car_index_)
{
  (void) display_mode_;

  return present_frontend_scene(FRONTEND_CAR_SCENE_SHAPE_COUNT,
                                &g_FRONTEND_CAR_SHAPE_NAMES[car_index_],
                                0,
                                NULL,
                                NULL,
                                FRONTEND_CAR_SCENE_PRIMARY_ENABLED,
                                FRONTEND_CAR_SCENE_SECONDARY_ENABLED,
                                FRONTEND_CAR_SCENE_DURATION);
}
