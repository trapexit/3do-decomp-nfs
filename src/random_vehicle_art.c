#include "bitmap_clip.h"
#include "cel_access.h"
#include "formatted_output.h"
#include "frontend_input_wait.h"
#include "random_number.h"
#include "vehicle_identifiers.h"
#include "vehicle_render_resource_selection.h"
#include "vehicle_selection_state.h"

#include "graphics.h"

#define RANDOM_VEHICLE_ART_SLOT_COUNT 20
#define RANDOM_VEHICLE_ART_CHOICE_COUNT 7
#define RANDOM_VEHICLE_ART_PATH_BYTES 100
#define RANDOM_VEHICLE_ART_CCB_FLAGS 0x5FEE4560
#define RANDOM_VEHICLE_ART_PATH_FORMAT "DriveData/CarData/%s%s%d"
#define RANDOM_VEHICLE_ART_PATH_SUFFIX ".s"

uint8 g_RANDOM_VEHICLE_ART_SLOTS[RANDOM_VEHICLE_ART_SLOT_COUNT];

void
load_random_vehicle_art(int32 reset_slots_)
{
  char  path[RANDOM_VEHICLE_ART_PATH_BYTES];
  CCB  *cel;
  int32 slot;
  int32 index;

  if(reset_slots_ != 0)
    {
      for(index = 0; index < RANDOM_VEHICLE_ART_SLOT_COUNT; index++)
        g_RANDOM_VEHICLE_ART_SLOTS[index] = 0;
    }

  do
    {
      slot = scale_by_random_fraction(RANDOM_VEHICLE_ART_CHOICE_COUNT);
    }
  while(g_RANDOM_VEHICLE_ART_SLOTS[slot] != 0);
  g_RANDOM_VEHICLE_ART_SLOTS[slot] = 1;

  sprintf(path,
          RANDOM_VEHICLE_ART_PATH_FORMAT,
          g_VEHICLE_IDENTIFIER_POINTERS[g_PLAYER_VEHICLE_INDEX],
          RANDOM_VEHICLE_ART_PATH_SUFFIX,
          slot);
  cel            = load_cel_default(path, 0);
  cel->ccb_Flags = RANDOM_VEHICLE_ART_CCB_FLAGS;
  wait_for_frontend_input();
  (void) DrawCels(g_BITMAP_DISPLAY_STATE.bitmap_items[g_BITMAP_DISPLAY_STATE.current_bitmap_index],
                  cel);
  (void) DisplayScreen(
    g_BITMAP_DISPLAY_STATE.display_resource_items[g_BITMAP_DISPLAY_STATE.current_bitmap_index],
    0);
  wait_for_frontend_input();
  (void) DrawCels(
    g_BITMAP_DISPLAY_STATE.bitmap_items[1 - g_BITMAP_DISPLAY_STATE.current_bitmap_index],
    cel);
  (void) unload_cel_handle(cel);
}
