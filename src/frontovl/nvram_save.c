#include "nvram_save.h"

#include "controller_input.h"
#include "frontend_frame.h"
#include "nvram_browser.h"
#include "nvram_checksum.h"
#include "post_race_legacy.h"
#include "race_state.h"
#include "runtime_memory.h"
#include "sdk_veneers.h"

#define NVRAM_CONFIGURATION_PATH "/nvram/Drive"
#define NVRAM_HIGH_SCORES_PATH "/nvram/DriveHS"
#define NVRAM_CONFIGURATION_BYTE_COUNT 0x60
#define NVRAM_HIGH_SCORES_BYTE_COUNT 0x820
#define NVRAM_CHECKSUM_OFFSET 0x81C
#define NVRAM_ERROR_LEFT 0
#define NVRAM_ERROR_TOP 10
#define NVRAM_ERROR_WIDTH 320
#define NVRAM_ERROR_HEIGHT 30
#define NVRAM_ERROR_COLOR 0x5400
#define NVRAM_ERROR_TEXT_X 20
#define NVRAM_ERROR_TEXT_Y 20
#define NVRAM_CONFIGURATION_ERROR "NVRAM Full: Configuration not saved!"
#define NVRAM_HIGH_SCORES_ERROR "NVRAM Full: High Scores not saved!"

int32
save_frontend_nvram(void)
{
  int32 retry_result;

  initialize_frontend_display();
  do
    {
      if(save_runtime_file(NVRAM_CONFIGURATION_PATH,
                           RACE_CONFIGURATION,
                           NVRAM_CONFIGURATION_BYTE_COUNT) != 0)
        {
          break;
        }
      prepare_runtime_display();
      fill_runtime_rectangle(NVRAM_ERROR_LEFT,
                             NVRAM_ERROR_TOP,
                             NVRAM_ERROR_WIDTH,
                             NVRAM_ERROR_HEIGHT,
                             NVRAM_ERROR_COLOR);
      draw_runtime_text(NVRAM_CONFIGURATION_ERROR, NVRAM_ERROR_TEXT_X, NVRAM_ERROR_TEXT_Y);
      finish_runtime_frame();
      wait_for_controller_input(0);
      retry_result = run_nvram_recovery_browser();
    }
  while(retry_result > 0);

  *(int32 *) (RACE_PARTICIPANT_RECORDS + NVRAM_CHECKSUM_OFFSET) = 0;
  *(int32 *) (RACE_PARTICIPANT_RECORDS + NVRAM_CHECKSUM_OFFSET) =
    -calculate_nvram_checksum(RACE_PARTICIPANT_RECORDS, NVRAM_HIGH_SCORES_BYTE_COUNT);

  do
    {
      if(save_runtime_file(NVRAM_HIGH_SCORES_PATH,
                           RACE_PARTICIPANT_RECORDS,
                           NVRAM_HIGH_SCORES_BYTE_COUNT) != 0)
        {
          break;
        }
      prepare_runtime_display();
      fill_runtime_rectangle(NVRAM_ERROR_LEFT,
                             NVRAM_ERROR_TOP,
                             NVRAM_ERROR_WIDTH,
                             NVRAM_ERROR_HEIGHT,
                             NVRAM_ERROR_COLOR);
      draw_runtime_text(NVRAM_HIGH_SCORES_ERROR, NVRAM_ERROR_TEXT_X, NVRAM_ERROR_TEXT_Y);
      finish_runtime_frame();
      wait_for_controller_input(0);
      retry_result = run_nvram_recovery_browser();
    }
  while(retry_result > 0);

  return 1;
}
