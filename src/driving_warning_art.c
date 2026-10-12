#include "driving_warning_art.h"

#include "game_resource_cleanup.h"
#include "pointer_array.h"
#include "simulation_common_art.h"

#define DRIVING_WARNING_ART_FAMILY_INDEX 5
#define DRIVING_WARNING_ART_CEL_COUNT 18
#define DRIVING_WARNING_ART_PIXC_FIRST_INDEX 2
#define DRIVING_WARNING_ART_PIXC_LAST_INDEX 10
#define DRIVING_WARNING_ART_MASK_PIXC 0x1F811F81
#define DRIVING_WARNING_ART_PRIMARY_PIXC 0x1F001F00

void
initialize_driving_warning_art_cels(void)
{
  CCB  *cel;
  int32 cel_index;

  g_GAME_RESOURCE_STATE.driving_warning_art_family =
    get_headered_pointer_array_element((void *const *) g_SIMULATION_COMMON_ART,
                                       DRIVING_WARNING_ART_FAMILY_INDEX);

  for(cel_index = 0; cel_index < DRIVING_WARNING_ART_CEL_COUNT; ++cel_index)
    {
      cel = (CCB *) get_pointer_array_element(
        (void *const *) g_GAME_RESOURCE_STATE.driving_warning_art_family,
        cel_index);
      g_GAME_RESOURCE_STATE.driving_warning_art_cels[cel_index] = cel;
      if((cel_index >= DRIVING_WARNING_ART_PIXC_FIRST_INDEX) &&
         (cel_index <= DRIVING_WARNING_ART_PIXC_LAST_INDEX))
        {
          cel->ccb_PIXC = DRIVING_WARNING_ART_MASK_PIXC;
        }
    }
  g_GAME_RESOURCE_STATE.driving_warning_art_cels[DRIVING_WARNING_ART_PIXC_FIRST_INDEX]->ccb_PIXC =
    DRIVING_WARNING_ART_PRIMARY_PIXC;
}
