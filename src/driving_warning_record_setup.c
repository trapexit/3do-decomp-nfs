#include "driving_warning_display.h"
#include "pointer_array.h"

#define DRIVING_WARNING_RECORD_ACTIVE_FLAG 0x40000000
#define DRIVING_WARNING_RECORD_BACKGROUND_FLAG 0x20000000
#define DRIVING_WARNING_RECORD_LOAD_FLAG 0x00004000
#define DRIVING_WARNING_RECORD_CLEAR_FLAG 0x00008000
#define DRIVING_WARNING_RECORD_WORD_COUNT 17
#define DRIVING_WARNING_LINK_SAVED_PREAMBLE_OFFSET 3
#define DRIVING_WARNING_SPECIAL_MODE 2
#define DRIVING_WARNING_SPECIAL_PREVIOUS_OFFSET 2
#define DRIVING_WARNING_SPECIAL_SECONDARY_OFFSET 1
#define DRIVING_WARNING_SPECIAL_FOLLOWING_OFFSET 2
#define DRIVING_WARNING_DASHBOARD_RECORD_BASE_INDEX 21
#define DRIVING_WARNING_DASHBOARD_RECORD_COUNT 9
#define DRIVING_WARNING_DASHBOARD_CHAIN_FIRST_INDEX 1
#define DRIVING_WARNING_DASHBOARD_CHAIN_LAST_INDEX 6
#define DRIVING_WARNING_PRIMARY_RECORD_COUNT 15
#define DRIVING_WARNING_EXTENDED_SELECTION_COUNT 15
#define DRIVING_WARNING_PREPARED_SOURCE_INDEX 10
#define DRIVING_WARNING_PREPARED_FIRST_INDEX 1
#define DRIVING_WARNING_PREPARED_RECORD_COUNT 5
#define DRIVING_WARNING_SECONDARY_TEMPLATE_COUNT 12
#define DRIVING_WARNING_SECONDARY_RECORD_COUNT 10
#define DRIVING_WARNING_SECONDARY_PREAMBLE 0x1F001F00

typedef struct DrivingWarningLinkDefinition
{
  int32 record_count;
  uint8 reserved_004[0x304];
  int32 transition_record_count;
  int32 display_mode;
} DrivingWarningLinkDefinition;

void
link_driving_warning_records(const uint8 *resource_definition_, int32 link_count_)
{
  const DrivingWarningLinkDefinition *definition;
  DrivingWarningRecord               *dashboard_record;
  DrivingWarningRecord               *record;
  DrivingWarningRecord               *previous_record;
  int32                               base_record_index;
  int32                               record_index;

  (void) resource_definition_;
  definition = (const DrivingWarningLinkDefinition *)
                 g_DRIVING_WARNING_DISPLAY_STATE.warning_record_link_index;
  g_DRIVING_WARNING_DISPLAY_STATE.dynamic_object_link_flags_enabled = 0;
  for(record_index = 0; record_index <= link_count_; ++record_index)
    {
      g_DRIVING_WARNING_RECORD_LINKS[record_index] =
        (DrivingWarningRecord *) get_pointer_array_element(
          (void *const *) g_DRIVING_WARNING_DISPLAY_STATE.dashboard_record_one,
          record_index);
    }
  for(record_index = 0; record_index <= link_count_; ++record_index)
    {
      record       = g_DRIVING_WARNING_RECORD_LINKS[record_index];
      record->next = g_DRIVING_WARNING_RECORD_LINKS[record_index + 1];
      record->flags &= ~DRIVING_WARNING_RECORD_ACTIVE_FLAG;
      record->flags |= DRIVING_WARNING_RECORD_LOAD_FLAG;
      record->flags &= ~DRIVING_WARNING_RECORD_CLEAR_FLAG;
    }

  base_record_index = definition->record_count;
  g_DRIVING_WARNING_DISPLAY_STATE.saved_dynamic_object_link_preamble =
    g_DRIVING_WARNING_RECORD_LINKS[base_record_index + DRIVING_WARNING_LINK_SAVED_PREAMBLE_OFFSET]
      ->preamble;
  if(definition->display_mode == DRIVING_WARNING_SPECIAL_MODE)
    {
      record =
        g_DRIVING_WARNING_RECORD_LINKS[base_record_index -
                                       DRIVING_WARNING_SPECIAL_PREVIOUS_OFFSET];
      record->next = g_DRIVING_WARNING_RECORD_LINKS[base_record_index];
      record =
        g_DRIVING_WARNING_RECORD_LINKS[base_record_index +
                                       DRIVING_WARNING_SPECIAL_SECONDARY_OFFSET];
      record->next =
        g_DRIVING_WARNING_RECORD_LINKS[base_record_index -
                                       DRIVING_WARNING_SPECIAL_SECONDARY_OFFSET];
      record =
        g_DRIVING_WARNING_RECORD_LINKS[base_record_index -
                                       DRIVING_WARNING_SPECIAL_SECONDARY_OFFSET];
      record->next =
        g_DRIVING_WARNING_RECORD_LINKS[base_record_index +
                                       DRIVING_WARNING_SPECIAL_FOLLOWING_OFFSET];
    }

  for(record_index = 0; record_index < DRIVING_WARNING_DASHBOARD_RECORD_COUNT; ++record_index)
    {
      dashboard_record = (DrivingWarningRecord *) get_pointer_array_element(
        (void *const *) g_DRIVING_WARNING_DISPLAY_STATE.dashboard_record_three,
        record_index);
      g_DRIVING_WARNING_DISPLAY_STATE
        .first_digit_record_inputs[DRIVING_WARNING_DASHBOARD_RECORD_BASE_INDEX + record_index] =
        dashboard_record;
      dashboard_record->flags |= DRIVING_WARNING_RECORD_BACKGROUND_FLAG;
      if((record_index >= DRIVING_WARNING_DASHBOARD_CHAIN_FIRST_INDEX) &&
         (record_index <= DRIVING_WARNING_DASHBOARD_CHAIN_LAST_INDEX))
        {
          previous_record =
            g_DRIVING_WARNING_DISPLAY_STATE.first_digit_record_inputs
              [DRIVING_WARNING_DASHBOARD_RECORD_BASE_INDEX + record_index - 1];
          previous_record->next =
            g_DRIVING_WARNING_DISPLAY_STATE.first_digit_record_inputs
              [DRIVING_WARNING_DASHBOARD_RECORD_BASE_INDEX + record_index];
          previous_record =
            g_DRIVING_WARNING_DISPLAY_STATE.first_digit_record_inputs
              [DRIVING_WARNING_DASHBOARD_RECORD_BASE_INDEX + record_index - 1];
          previous_record->flags &= ~DRIVING_WARNING_RECORD_ACTIVE_FLAG;
        }
      dashboard_record = g_DRIVING_WARNING_DISPLAY_STATE.first_digit_record_inputs
                           [DRIVING_WARNING_DASHBOARD_RECORD_BASE_INDEX + record_index];
      dashboard_record->next = NULL;
      dashboard_record = g_DRIVING_WARNING_DISPLAY_STATE.first_digit_record_inputs
                           [DRIVING_WARNING_DASHBOARD_RECORD_BASE_INDEX + record_index];
      dashboard_record->flags |= DRIVING_WARNING_RECORD_ACTIVE_FLAG;
    }
  g_DRIVING_WARNING_DISPLAY_STATE.warning_rendering                    = 1;
  g_DRIVING_WARNING_DISPLAY_STATE.warning_cel_loaded                   = 0;
  g_DRIVING_WARNING_DISPLAY_STATE.dashboard_motion_divisor_initialized = 0;
}

int32
initialize_driving_warning_records(const uint8 *resource_definition_, int32 selection_count_)
{
  const DrivingWarningRecord *prepared_template_record;
  const DrivingWarningRecord *primary_template_record;
  const DrivingWarningRecord *secondary_template_record;
  const int32                *prepared_template_words;
  const int32                *primary_template_words;
  const int32                *secondary_template_words;
  DrivingWarningRecord       *prepared_record;
  DrivingWarningRecord       *primary_record;
  DrivingWarningRecord       *secondary_input_record;
  DrivingWarningRecord       *secondary_record;
  int32                      *prepared_record_words;
  int32                      *primary_record_words;
  int32                      *secondary_record_words;
  int32                       record_index;
  int32                       record_word_index;

  (void) resource_definition_;
  for(record_index = 0; record_index < selection_count_; ++record_index)
    {
      g_DRIVING_WARNING_DISPLAY_STATE.first_digit_record_inputs[record_index] =
        (DrivingWarningRecord *) get_pointer_array_element(
          (void *const *) g_DRIVING_WARNING_DISPLAY_STATE.dashboard_record_two,
          record_index);
    }

  primary_template_record = g_DRIVING_WARNING_DISPLAY_STATE.first_digit_record_inputs[0];
  for(record_index = 0; record_index < DRIVING_WARNING_PRIMARY_RECORD_COUNT; ++record_index)
    {
      primary_record         = &g_DRIVING_WARNING_RECORDS[record_index];
      primary_template_words = (const int32 *) primary_template_record;
      primary_record_words   = (int32 *) primary_record;
      for(record_word_index = 0; record_word_index < DRIVING_WARNING_RECORD_WORD_COUNT;
          ++record_word_index)
        {
          primary_record_words[record_word_index] = primary_template_words[record_word_index];
        }
      primary_record->next = primary_record + 1;
      primary_record->flags &= ~DRIVING_WARNING_RECORD_ACTIVE_FLAG;
    }
  primary_record->next = NULL;
  primary_record->flags |= DRIVING_WARNING_RECORD_ACTIVE_FLAG;

  if(selection_count_ > DRIVING_WARNING_EXTENDED_SELECTION_COUNT)
    {
      prepared_template_record = g_DRIVING_WARNING_DISPLAY_STATE
                                   .first_digit_record_inputs[DRIVING_WARNING_PREPARED_SOURCE_INDEX];
      for(record_index = 0; record_index < DRIVING_WARNING_PREPARED_RECORD_COUNT; ++record_index)
        {
          prepared_record =
            &g_PREPARED_DRIVING_WARNING_RECORDS[DRIVING_WARNING_PREPARED_FIRST_INDEX +
                                                record_index];
          prepared_template_words = (const int32 *) prepared_template_record;
          prepared_record_words   = (int32 *) prepared_record;
          for(record_word_index = 0; record_word_index < DRIVING_WARNING_RECORD_WORD_COUNT;
              ++record_word_index)
            {
              prepared_record_words[record_word_index] =
                prepared_template_words[record_word_index];
            }
          prepared_record->next = prepared_record + 1;
          prepared_record->flags &= ~DRIVING_WARNING_RECORD_ACTIVE_FLAG;
        }
      prepared_record->next = NULL;
      prepared_record->flags |= DRIVING_WARNING_RECORD_ACTIVE_FLAG;
    }

  for(record_index = 0; record_index < DRIVING_WARNING_SECONDARY_TEMPLATE_COUNT; ++record_index)
    {
      secondary_input_record = (DrivingWarningRecord *) get_pointer_array_element(
        (void *const *) g_DRIVING_WARNING_DISPLAY_STATE.dashboard_record_four,
        record_index);
      g_DRIVING_WARNING_DISPLAY_STATE.second_digit_record_inputs[record_index] =
        secondary_input_record;
      secondary_input_record->preamble = DRIVING_WARNING_SECONDARY_PREAMBLE;
    }

  secondary_template_record = g_DRIVING_WARNING_DISPLAY_STATE.second_digit_record_inputs[0];
  for(record_index = 0; record_index < DRIVING_WARNING_SECONDARY_RECORD_COUNT; ++record_index)
    {
      secondary_record         = &g_SECONDARY_DRIVING_WARNING_RECORDS[record_index];
      secondary_template_words = (const int32 *) secondary_template_record;
      secondary_record_words   = (int32 *) secondary_record;
      for(record_word_index = 0; record_word_index < DRIVING_WARNING_RECORD_WORD_COUNT;
          ++record_word_index)
        {
          secondary_record_words[record_word_index] = secondary_template_words[record_word_index];
        }
      secondary_record->next = secondary_record + 1;
      secondary_record->flags &= ~DRIVING_WARNING_RECORD_ACTIVE_FLAG;
    }
  secondary_record->next = NULL;
  secondary_record->flags |= DRIVING_WARNING_RECORD_ACTIVE_FLAG;
  return 1;
}
