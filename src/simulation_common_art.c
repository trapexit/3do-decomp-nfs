#include "simulation_common_art.h"

#include "cel_access.h"
#include "game_data_path.h"
#include "pointer_array.h"
#include "sound_effect_state.h"
#include "string_utilities.h"

#define SIMULATION_COMMON_ART_PATH_BYTE_COUNT 80
#define SIMULATION_COMMON_ART_PATH_SUFFIX "DriveData/DriveArt/SimCommonArt.Fam"
#define SIMULATION_COMMON_ART_CCB_FLAGS_MASK 0xE0000000
#define SIMULATION_COMMON_ART_CCB_PIXC_MASK 0x00001C00

void *g_SIMULATION_COMMON_ART;
CCB  *g_SIMULATION_COMMON_ART_CELS[SIMULATION_COMMON_ART_CEL_COUNT];

void
load_simulation_common_art(void)
{
  char path[SIMULATION_COMMON_ART_PATH_BYTE_COUNT];

  copy_string(path, g_GAME_DATA_PATH);
  append_string(path, SIMULATION_COMMON_ART_PATH_SUFFIX);
  g_SIMULATION_COMMON_ART = load_headered_file(path);
}

void
initialize_simulation_common_art_cels(void)
{
  int32 index;
  CCB  *cel;

  for(index = 0; index < SIMULATION_COMMON_ART_CEL_COUNT; index += 1)
    {
      cel =
        (CCB *) get_headered_pointer_array_element((void *const *) g_SIMULATION_COMMON_ART, index);
      g_SIMULATION_COMMON_ART_CELS[index] = cel;
      set_cel_dimension_log2(cel);
      cel->ccb_Flags &= ~SIMULATION_COMMON_ART_CCB_FLAGS_MASK;
      cel->ccb_PIXC &= ~SIMULATION_COMMON_ART_CCB_PIXC_MASK;
    }
}
