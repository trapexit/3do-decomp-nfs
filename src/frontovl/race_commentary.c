#include "race_commentary.h"

#include "race_state.h"
#include "random_number.h"
#include "runtime_memory.h"
#include "sdk_veneers.h"

#define RACE_PARTICIPANT_RECORD_BYTE_COUNT 0x1C0
#define RACE_PARTICIPANT_TIMING_OFFSET 4
#define COMMENTARY_LARGE_TIME_GAP 300
#define COMMENTARY_DECISIVE_TIME_GAP 1800
#define COMMENTARY_PROGRESS_GAP 5
#define COMMENTARY_DISABLED_RACE_MODE 8
#define COMMENTARY_FOUR_VARIANTS 4
#define COMMENTARY_THREE_VARIANTS 3
#define COMMENTARY_TWO_VARIANTS 2
#define COMMENTARY_EVEN_VARIANT_BASE 2
#define COMMENTARY_EVEN_VARIANT_STEP 2
#define COMMENTARY_CUE_BYTE_COUNT 5

typedef struct RaceParticipantTiming
{
  uint8 reserved_before_elapsed_time[0x160];
  int32 elapsed_time;
  uint8 reserved_before_progress[0x58];
  int32 progress;
} RaceParticipantTiming;

void
select_race_commentary_cue(char *cue_code_)
{
  RaceParticipantTiming *participant_timing;
  int32                  participant_time;
  int32                  primary_time;
  int32                  secondary_time;
  int32                  participant_gap;
  int32                  secondary_gap;
  int32                  progress_gap;
  int32                  variant;

  participant_timing = (RaceParticipantTiming *) (RACE_PARTICIPANT_RECORDS +
                                                  RACE_CONFIGURATION->track_index *
                                                    RACE_PARTICIPANT_RECORD_BYTE_COUNT +
                                                  RACE_PARTICIPANT_TIMING_OFFSET);
  RUNTIME_MEMCPY_VENEER(cue_code_, "", 1);

  participant_time = participant_timing->elapsed_time;
  primary_time     = RACE_TIMING_STATE->primary_time;
  secondary_time   = RACE_TIMING_STATE->secondary_time;
  participant_gap  = participant_time - primary_time;
  secondary_gap    = secondary_time - primary_time;

  if(participant_gap > COMMENTARY_LARGE_TIME_GAP && RACE_CONFIGURATION->skill_level != 0)
    {
      variant = random_below(COMMENTARY_FOUR_VARIANTS) + 1;
      runtime_format_string(cue_code_, "64.%d", variant);
      return;
    }
  if(secondary_gap > COMMENTARY_LARGE_TIME_GAP &&
     RACE_CONFIGURATION->opponent_vehicle_index != COMMENTARY_DISABLED_RACE_MODE)
    {
      variant = random_below(COMMENTARY_THREE_VARIANTS) + 1;
      runtime_format_string(cue_code_, "65.%d", variant);
      return;
    }
  if(participant_gap > 0 && RACE_CONFIGURATION->skill_level != 0)
    {
      variant = random_below(COMMENTARY_THREE_VARIANTS) + 1;
      runtime_format_string(cue_code_, "56.%d", variant);
      return;
    }
  if(secondary_gap > 0 &&
     RACE_CONFIGURATION->opponent_vehicle_index != COMMENTARY_DISABLED_RACE_MODE)
    {
      variant = random_below(COMMENTARY_THREE_VARIANTS) + 1;
      runtime_format_string(cue_code_, "57.%d", variant);
      return;
    }

  progress_gap = RACE_TIMING_STATE->progress - participant_timing->progress;
  if(progress_gap > COMMENTARY_PROGRESS_GAP)
    {
      variant = random_below(COMMENTARY_THREE_VARIANTS) + 1;
      runtime_format_string(cue_code_, "69.%d", variant);
      return;
    }
  if(progress_gap > 0)
    {
      RUNTIME_MEMCPY_VENEER(cue_code_, "1.07", COMMENTARY_CUE_BYTE_COUNT);
      return;
    }

  secondary_gap = primary_time - secondary_time;
  if(secondary_gap > COMMENTARY_DECISIVE_TIME_GAP &&
     RACE_CONFIGURATION->opponent_vehicle_index != COMMENTARY_DISABLED_RACE_MODE)
    {
      variant = random_below(COMMENTARY_TWO_VARIANTS) + 1;
      runtime_format_string(cue_code_, "60.%d", variant);
      return;
    }
  if(secondary_gap > 0 &&
     RACE_CONFIGURATION->opponent_vehicle_index != COMMENTARY_DISABLED_RACE_MODE)
    {
      variant = random_below(COMMENTARY_TWO_VARIANTS) + 1;
      runtime_format_string(cue_code_, "59.%d", variant);
      return;
    }

  participant_gap = primary_time - participant_time;
  if(participant_gap > COMMENTARY_DECISIVE_TIME_GAP && RACE_CONFIGURATION->skill_level != 0)
    {
      variant = COMMENTARY_EVEN_VARIANT_BASE +
                random_below(COMMENTARY_TWO_VARIANTS) * COMMENTARY_EVEN_VARIANT_STEP;
      runtime_format_string(cue_code_, "68.%d", variant);
      return;
    }
  if(participant_gap > 0 && RACE_CONFIGURATION->skill_level != 0)
    {
      variant = random_below(COMMENTARY_THREE_VARIANTS) + 1;
      runtime_format_string(cue_code_, "67.%d", variant);
      return;
    }

  RUNTIME_MEMCPY_VENEER(cue_code_, "2.02", COMMENTARY_CUE_BYTE_COUNT);
}
