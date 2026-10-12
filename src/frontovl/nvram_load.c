#include "nvram_load.h"

#include "nvram_checksum.h"
#include "nvram_save.h"
#include "race_state.h"
#include "runtime_memory.h"
#include "runtime_memory_ops.h"
#include "tracked_memory.h"

#include "event.h"

#define NVRAM_CONFIGURATION_ALLOCATION_NAME "NVRAM config"
#define NVRAM_HIGH_SCORES_ALLOCATION_NAME "NVRAM HS"
#define NVRAM_HIGH_SCORES_INITIAL_ALLOCATION_NAME "NVRAM HS Init"
#define NVRAM_CONFIGURATION_BYTE_COUNT RACE_CONFIGURATION_BYTE_COUNT
#define NVRAM_HIGH_SCORES_BYTE_COUNT 0x820
#define NVRAM_CONFIGURATION_VERSION 12
#define NVRAM_HIGH_SCORES_VERSION 13
#define NVRAM_HIGH_SCORES_CHECKSUM_OFFSET 0x81C
#define DEFAULT_TRACK_INDEX 2
#define DEFAULT_PLAYER_VEHICLE_INDEX 1
#define DEFAULT_OPPONENT_VEHICLE_INDEX 4
#define DEFAULT_OPTION_ENABLED 1
#define DEFAULT_OPTION_DISABLED 0
#define UNASSIGNED_CONTROL_0_INDEX 0
#define UNASSIGNED_CONTROL_1_INDEX 1
#define UNASSIGNED_CONTROL_2_INDEX 2
#define UNASSIGNED_CONTROL_3_INDEX 3
#define UNASSIGNED_CONTROL_4_INDEX 4
#define UNASSIGNED_CONTROL_5_INDEX 5

char g_NVRAM_CONFIGURATION_PATH[16]       = "/nvram/Drive";
char g_NVRAM_HIGH_SCORES_PATH[16]         = "/nvram/DriveHS";
char g_NVRAM_INITIAL_HIGH_SCORES_PATH[16] = "FrontEnd/InitHS";

typedef char NvramConfigurationPathSize[sizeof(g_NVRAM_CONFIGURATION_PATH) == 16 ? 1 : -1];
typedef char NvramHighScoresPathSize[sizeof(g_NVRAM_HIGH_SCORES_PATH) == 16 ? 1 : -1];
typedef char
  NvramInitialHighScoresPathSize[sizeof(g_NVRAM_INITIAL_HIGH_SCORES_PATH) == 16 ? 1 : -1];

static int32
_read_nvram_file(void       *destination_,
                 uint32      required_byte_count_,
                 const char *allocation_name_,
                 int32       handle_,
                 uint32      available_byte_count_)
{
  void *file_data;

  file_data = allocate_named_primary_memory(available_byte_count_, allocation_name_);
  if(file_data == NULL)
    {
      return 0;
    }

  read_runtime_file(handle_, file_data, available_byte_count_);
  close_runtime_file(handle_);
  copy_runtime_memory(destination_, file_data, required_byte_count_);
  free_primary_memory(file_data);
  return 1;
}

void
reset_frontend_configuration(void)
{
  RACE_CONFIGURATION->version                             = NVRAM_CONFIGURATION_VERSION;
  RACE_CONFIGURATION->track_index                         = DEFAULT_TRACK_INDEX;
  RACE_CONFIGURATION->player_vehicle_index                = DEFAULT_PLAYER_VEHICLE_INDEX;
  RACE_CONFIGURATION->opponent_vehicle_index              = DEFAULT_OPPONENT_VEHICLE_INDEX;
  RACE_CONFIGURATION->music_enabled                       = DEFAULT_OPTION_ENABLED;
  RACE_CONFIGURATION->sound_effects_enabled               = DEFAULT_OPTION_ENABLED;
  RACE_CONFIGURATION->stereo_enabled                      = DEFAULT_OPTION_ENABLED;
  RACE_CONFIGURATION->anti_lock_brakes_enabled            = DEFAULT_OPTION_ENABLED;
  RACE_CONFIGURATION->traction_control_enabled            = DEFAULT_OPTION_ENABLED;
  RACE_CONFIGURATION->performance_reduction_enabled       = DEFAULT_OPTION_ENABLED;
  RACE_CONFIGURATION->skill_level                         = DEFAULT_OPTION_DISABLED;
  RACE_CONFIGURATION->host_commentary_enabled             = DEFAULT_OPTION_ENABLED;
  RACE_CONFIGURATION->control_preset_index                = DEFAULT_OPTION_DISABLED;
  RACE_CONFIGURATION->joystick_calibration_configured     = DEFAULT_OPTION_DISABLED;
  RACE_CONFIGURATION->control_bindings.primary_action     = ControlA;
  RACE_CONFIGURATION->control_bindings.secondary_action   = ControlB;
  RACE_CONFIGURATION->control_bindings.tertiary_action    = ControlC;
  RACE_CONFIGURATION->control_bindings.right_shift_action = ControlRightShift;
  RACE_CONFIGURATION->control_bindings.left_shift_action  = ControlLeftShift;
  RACE_CONFIGURATION->control_bindings.down_left_action   = ControlDown | ControlLeft;
  RACE_CONFIGURATION->control_bindings.left_action        = ControlLeft;
  RACE_CONFIGURATION->control_bindings.down_right_action  = ControlDown | ControlRight;
  RACE_CONFIGURATION->control_bindings.up_right_action    = ControlUp | ControlRight;
  RACE_CONFIGURATION->control_bindings.right_action       = ControlRight;
  RACE_CONFIGURATION->control_bindings.up_left_action     = ControlUp | ControlLeft;
  RACE_CONFIGURATION->control_bindings.start_action       = ControlStart;
  RACE_CONFIGURATION->control_bindings.auxiliary_action   = ControlX;
  RACE_CONFIGURATION->control_bindings.unassigned[UNASSIGNED_CONTROL_0_INDEX] = 0;
  RACE_CONFIGURATION->control_bindings.unassigned[UNASSIGNED_CONTROL_1_INDEX] = 0;
  RACE_CONFIGURATION->control_bindings.unassigned[UNASSIGNED_CONTROL_2_INDEX] = 0;
  RACE_CONFIGURATION->control_bindings.unassigned[UNASSIGNED_CONTROL_3_INDEX] = 0;
  RACE_CONFIGURATION->control_bindings.unassigned[UNASSIGNED_CONTROL_4_INDEX] = 0;
  RACE_CONFIGURATION->control_bindings.unassigned[UNASSIGNED_CONTROL_5_INDEX] = 0;
}

void
reset_frontend_high_scores(void)
{
  uint32 byte_count;
  void  *mapped_data;
  int32  handle;
  int32  checksum;

  fill_runtime_memory(RACE_PARTICIPANT_RECORDS, 0, NVRAM_HIGH_SCORES_BYTE_COUNT);
  *(int32 *) RACE_PARTICIPANT_RECORDS = NVRAM_HIGH_SCORES_VERSION;
  checksum = calculate_nvram_checksum(RACE_PARTICIPANT_RECORDS, NVRAM_HIGH_SCORES_BYTE_COUNT);
  *(int32 *) (RACE_PARTICIPANT_RECORDS + NVRAM_HIGH_SCORES_CHECKSUM_OFFSET) = -checksum;

  mapped_data = NULL;
  open_runtime_file(g_NVRAM_INITIAL_HIGH_SCORES_PATH, &handle, &mapped_data, &byte_count);
  if(byte_count != NVRAM_HIGH_SCORES_BYTE_COUNT && byte_count < NVRAM_HIGH_SCORES_BYTE_COUNT)
    {
      byte_count = NVRAM_HIGH_SCORES_BYTE_COUNT;
    }
  if(handle != 0)
    {
      _read_nvram_file(RACE_PARTICIPANT_RECORDS,
                       NVRAM_HIGH_SCORES_BYTE_COUNT,
                       NVRAM_HIGH_SCORES_INITIAL_ALLOCATION_NAME,
                       handle,
                       byte_count);
    }
}

int32
load_frontend_nvram(void)
{
  uint32 byte_count;
  void  *mapped_data;
  int32  handle;
  int32  configuration_was_reset;

  configuration_was_reset = 0;
  mapped_data             = NULL;
  open_runtime_file(g_NVRAM_CONFIGURATION_PATH, &handle, &mapped_data, &byte_count);
  if(byte_count != NVRAM_CONFIGURATION_BYTE_COUNT && byte_count < NVRAM_CONFIGURATION_BYTE_COUNT)
    {
      byte_count = NVRAM_CONFIGURATION_BYTE_COUNT;
    }
  if(handle == 0)
    {
      reset_frontend_configuration();
      configuration_was_reset = 1;
    }
  else
    {
      if(_read_nvram_file(RACE_CONFIGURATION,
                          NVRAM_CONFIGURATION_BYTE_COUNT,
                          NVRAM_CONFIGURATION_ALLOCATION_NAME,
                          handle,
                          byte_count) == 0)
        {
          return 0;
        }
      if(*(int32 *) RACE_CONFIGURATION != NVRAM_CONFIGURATION_VERSION)
        {
          reset_frontend_configuration();
          configuration_was_reset = 1;
        }
    }

  mapped_data = NULL;
  open_runtime_file(g_NVRAM_HIGH_SCORES_PATH, &handle, &mapped_data, &byte_count);
  if(byte_count != NVRAM_HIGH_SCORES_BYTE_COUNT && byte_count < NVRAM_HIGH_SCORES_BYTE_COUNT)
    {
      byte_count = NVRAM_HIGH_SCORES_BYTE_COUNT;
    }
  if(handle == 0)
    {
      reset_frontend_high_scores();
    }
  else
    {
      if(_read_nvram_file(RACE_PARTICIPANT_RECORDS,
                          NVRAM_HIGH_SCORES_BYTE_COUNT,
                          NVRAM_HIGH_SCORES_ALLOCATION_NAME,
                          handle,
                          byte_count) == 0)
        {
          return 0;
        }
      if(*(int32 *) RACE_PARTICIPANT_RECORDS != NVRAM_HIGH_SCORES_VERSION)
        {
          reset_frontend_high_scores();
          configuration_was_reset = 1;
        }
      else if(calculate_nvram_checksum(RACE_PARTICIPANT_RECORDS, NVRAM_HIGH_SCORES_BYTE_COUNT) != 0)
        {
          reset_frontend_high_scores();
        }
    }

  if(configuration_was_reset != 0)
    {
      save_frontend_nvram();
    }
  return 1;
}
