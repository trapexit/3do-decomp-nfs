/* This module decodes the fixed-order driving-warning dashboard definition. */

#include "camera_io.h"
#include "disk_stream_cleanup.h"
#include "driving_warning_display.h"
#include "memory.h"
#include "pointer_array.h"
#include "string_utilities.h"
#include "text_line_reader.h"

#include "blockfile.h"

#include "stddef.h"


#define DASHBOARD_RESOURCE_PATH_BYTES 80
#define DASHBOARD_RESOURCE_RECORD_COUNT 16
#define DASHBOARD_RESOURCE_RECORD_BYTES 0x1C
#define DASHBOARD_RESOURCE_POINTER_RECORD_TWO 2
#define DASHBOARD_RESOURCE_POINTER_RECORD_THREE 3
#define DASHBOARD_RESOURCE_POINTER_RECORD_FOUR 4
#define DASHBOARD_RESOURCE_DEFINITION_BYTES 0x310
#define DASHBOARD_RESOURCE_CACHE_BYTES 0x310
#define DASHBOARD_RESOURCE_ALLOCATION_FLAGS 0
#define DASHBOARD_RESOURCE_DEFAULT_SELECTION 10
#define DASHBOARD_RESOURCE_EXTENDED_SELECTION 20
#define DASHBOARD_RESOURCE_MODE_EXTENDED 1
#define DASHBOARD_RESOURCE_LINK_BASE 7
#define DASHBOARD_RESOURCE_FIXED16_SHIFT 16
#define DASHBOARD_RESOURCE_FIXED8_SHIFT 8
#define DASHBOARD_RESOURCE_HALF_DIVISOR 2
#define DASHBOARD_RESOURCE_QUARTER_SHIFT 2
#define DASHBOARD_RESOURCE_COORDINATE_WORD_COUNT 2
#define DASHBOARD_RESOURCE_CACHE_SUFFIX ".DCBin"
#define DASHBOARD_RESOURCE_CONSTANTS_SUFFIX "DashConstants"
#define DASHBOARD_RESOURCE_ALLOCATION_NAME "Dash_t"

#define DASHBOARD_FIELD_1C4 0x1C4
#define DASHBOARD_FIELD_1C8 0x1C8
#define DASHBOARD_FIELD_1CC 0x1CC
#define DASHBOARD_FIELD_1D0 0x1D0
#define DASHBOARD_FIELD_1D4 0x1D4
#define DASHBOARD_FIELD_1D8 0x1D8
#define DASHBOARD_FIELD_1DC 0x1DC
#define DASHBOARD_FIELD_1E0 0x1E0
#define DASHBOARD_FIELD_1E4 0x1E4
#define DASHBOARD_FIELD_1E8 0x1E8
#define DASHBOARD_FIELD_1EC 0x1EC
#define DASHBOARD_FIELD_1F0 0x1F0
#define DASHBOARD_FIELD_1F4 0x1F4
#define DASHBOARD_FIELD_1F8 0x1F8
#define DASHBOARD_FIELD_1FC 0x1FC
#define DASHBOARD_FIELD_200 0x200
#define DASHBOARD_FIELD_204 0x204
#define DASHBOARD_FIELD_208 0x208
#define DASHBOARD_FIELD_20C 0x20C
#define DASHBOARD_FIELD_210 0x210
#define DASHBOARD_FIELD_214 0x214
#define DASHBOARD_FIELD_218 0x218
#define DASHBOARD_FIELD_21C 0x21C
#define DASHBOARD_FIELD_220 0x220
#define DASHBOARD_FIELD_224 0x224
#define DASHBOARD_FIELD_238 0x238
#define DASHBOARD_FIELD_23C 0x23C
#define DASHBOARD_FIELD_240 0x240
#define DASHBOARD_FIELD_244 0x244
#define DASHBOARD_FIELD_248 0x248
#define DASHBOARD_FIELD_24C 0x24C
#define DASHBOARD_FIELD_250 0x250
#define DASHBOARD_FIELD_25C 0x25C
#define DASHBOARD_FIELD_260 0x260
#define DASHBOARD_FIELD_278 0x278
#define DASHBOARD_FIELD_27C 0x27C
#define DASHBOARD_FIELD_280 0x280
#define DASHBOARD_FIELD_284 0x284
#define DASHBOARD_FIELD_288 0x288
#define DASHBOARD_FIELD_28C 0x28C
#define DASHBOARD_FIELD_290 0x290
#define DASHBOARD_FIELD_294 0x294
#define DASHBOARD_FIELD_298 0x298
#define DASHBOARD_FIELD_29C 0x29C
#define DASHBOARD_FIELD_2A0 0x2A0
#define DASHBOARD_FIELD_2A4 0x2A4
#define DASHBOARD_FIELD_2A8 0x2A8
#define DASHBOARD_FIELD_2AC 0x2AC
#define DASHBOARD_FIELD_2B0 0x2B0
#define DASHBOARD_FIELD_2B4 0x2B4
#define DASHBOARD_FIELD_2B8 0x2B8
#define DASHBOARD_FIELD_2BC 0x2BC
#define DASHBOARD_FIELD_2C0 0x2C0
#define DASHBOARD_FIELD_2C4 0x2C4
#define DASHBOARD_FIELD_308 0x308
#define DASHBOARD_FIELD_30C 0x30C

#define DASHBOARD_FIELD(definition_, offset_) (*(int32 *) ((uint8 *) (definition_) + (offset_)))

typedef struct DashboardResourceRecord
{
  int32 resource_value_a;
  int32 resource_value_b;
  int32 horizontal_offset;
  int32 vertical_offset;
  int32 horizontal_extent;
  int32 vertical_extent;
  uint8 reserved_018[4];
} DashboardResourceRecord;

typedef struct DashboardResourceDefinition
{
  int32                   record_count;
  DashboardResourceRecord records[DASHBOARD_RESOURCE_RECORD_COUNT];
  uint8                   parsed_fields[0x14C];
} DashboardResourceDefinition;

typedef struct DrivingWarningResourceState
{
  void                        *first_record;
  void                        *second_record;
  uint8                        reserved_008[4];
  int32                        render_selection;
  uint8                        reserved_010[4];
  DashboardResourceDefinition *definition;
  uint8                        reserved_018[0xBC];
  void                        *render_record_a;
  void                        *render_record_b;
  void                        *render_record_c;
  void                        *render_record_d;
  void                        *render_record_e;
  void                        *render_record_f;
  void                        *render_record_g;
  void                        *render_record_h;
  uint8                        resource_constants_loaded;
  uint8                        reserved_0_f5[3];
  uint8                        dashboard_family_loaded;
  uint8                        reserved_0_f9[0x0F];
  void                        *dashboard_record_one;
  void                        *dashboard_family_file;
  void                        *dashboard_record_two;
  void                        *dashboard_record_three;
  void                        *dashboard_record_four;
} DrivingWarningResourceState;

typedef char DashboardResourceDefinitionSize
  [(sizeof(DashboardResourceDefinition) == DASHBOARD_RESOURCE_DEFINITION_BYTES) ? 1 : -1];
typedef char DrivingWarningResourceDefinitionOffset
  [(offsetof(DrivingWarningResourceState, definition) == 0x14) ? 1 : -1];
typedef char DrivingWarningResourceFamilyOffset
  [(offsetof(DrivingWarningResourceState, dashboard_family_file) == 0x10C) ? 1 : -1];

void
load_driving_warning_display_resource(uint8 *resource_definition_, int32 option_)
{
  DashboardResourceDefinition *definition;
  DrivingWarningResourceState *state;
  DashboardResourceRecord     *record;
  Stream                      *stream;
  char                         resource_path[DASHBOARD_RESOURCE_PATH_BYTES];
  int32                        record_index;
  int32                        coordinate_count;

  /* The file format is a fixed sequence, so keep its reads in wire order. */
  load_dashboard_family_file((const char *) resource_definition_, (uint8) option_);
  state = (DrivingWarningResourceState *) &g_DRIVING_WARNING_DISPLAY_STATE;

  if(state->dashboard_family_loaded == 0)
    {
      state->definition = (DashboardResourceDefinition *) get_headered_pointer_array_element(
        (void *const *) state->dashboard_family_file,
        0);
    }
  state->dashboard_record_one =
    get_headered_pointer_array_element((void *const *) state->dashboard_family_file, 1);
  state->dashboard_record_two =
    get_headered_pointer_array_element((void *const *) state->dashboard_family_file,
                                       DASHBOARD_RESOURCE_POINTER_RECORD_TWO);
  state->dashboard_record_three =
    get_headered_pointer_array_element((void *const *) state->dashboard_family_file,
                                       DASHBOARD_RESOURCE_POINTER_RECORD_THREE);
  state->dashboard_record_four =
    get_headered_pointer_array_element((void *const *) state->dashboard_family_file,
                                       DASHBOARD_RESOURCE_POINTER_RECORD_FOUR);

  if(state->dashboard_family_loaded != 0)
    {
      copy_string(resource_path, (const char *) resource_definition_);
      append_string(resource_path, DASHBOARD_RESOURCE_CONSTANTS_SUFFIX);
      stream = open_disk_stream_default(resource_path);
      if(stream != NULL)
        {
          definition = (DashboardResourceDefinition *) allocate_named_memory(
            DASHBOARD_RESOURCE_ALLOCATION_NAME,
            DASHBOARD_RESOURCE_DEFINITION_BYTES,
            DASHBOARD_RESOURCE_ALLOCATION_FLAGS);
          state->definition = definition;
          definition->record_count =
            (read_second_fixed16_text_line(stream) >> DASHBOARD_RESOURCE_FIXED16_SHIFT);

          for(record_index = 0; record_index < definition->record_count; record_index += 1)
            {
              record                   = &definition->records[record_index];
              record->resource_value_a = read_second_fixed16_text_line(stream);
              record->resource_value_b = read_second_fixed16_text_line(stream);
              record->horizontal_offset =
                (read_second_fixed16_text_line(stream) >> DASHBOARD_RESOURCE_FIXED8_SHIFT);
              record->vertical_offset =
                (read_second_fixed16_text_line(stream) >> DASHBOARD_RESOURCE_FIXED8_SHIFT);
              record->horizontal_extent =
                (read_second_fixed16_text_line(stream) >> DASHBOARD_RESOURCE_FIXED8_SHIFT);
              record->vertical_extent =
                (read_second_fixed16_text_line(stream) >> DASHBOARD_RESOURCE_FIXED8_SHIFT);
            }

          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1C4) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1C8) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1E4) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1E8) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1CC) =
            (-(read_second_fixed16_text_line(stream)) / DASHBOARD_RESOURCE_HALF_DIVISOR);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1D0) =
            (-(read_second_fixed16_text_line(stream)) / DASHBOARD_RESOURCE_HALF_DIVISOR);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1D4) =
            (-(read_second_fixed16_text_line(stream)) >> DASHBOARD_RESOURCE_QUARTER_SHIFT);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1D8) =
            (-(read_second_fixed16_text_line(stream)) >> DASHBOARD_RESOURCE_QUARTER_SHIFT);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1DC) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1E0) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1EC) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1F0) =
            (read_second_fixed16_text_line(stream) >> DASHBOARD_RESOURCE_FIXED8_SHIFT);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1F4) =
            (read_second_fixed16_text_line(stream) >> DASHBOARD_RESOURCE_FIXED8_SHIFT);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1F8) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1FC) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_200) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_204) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_208) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_20C) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_210) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_214) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_218) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_21C) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_220) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_224) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_238) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_23C) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_25C) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_260) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_240) =
            (read_second_fixed16_text_line(stream) >> DASHBOARD_RESOURCE_FIXED8_SHIFT);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_244) =
            (read_second_fixed16_text_line(stream) >> DASHBOARD_RESOURCE_FIXED8_SHIFT);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_248) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_278) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_27C) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_24C) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_250) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_280) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_284) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_288) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_28C) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_290) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_294) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_298) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_29C) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_2A0) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_2A4) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_2A8) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_2AC) = read_second_fixed16_text_line(stream);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_2B0) =
            (read_second_fixed16_text_line(stream) >> DASHBOARD_RESOURCE_FIXED8_SHIFT);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_2B4) =
            (read_second_fixed16_text_line(stream) >> DASHBOARD_RESOURCE_FIXED8_SHIFT);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_2B8) = read_second_fixed16_text_line(stream);
          coordinate_count =
            (read_second_fixed16_text_line(stream) >> DASHBOARD_RESOURCE_FIXED16_SHIFT);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_2BC) = coordinate_count;

          for(record_index = 0; record_index < coordinate_count; record_index += 1)
            {
              DASHBOARD_FIELD(definition,
                              DASHBOARD_FIELD_2C0 + (record_index * sizeof(int32) *
                                                     DASHBOARD_RESOURCE_COORDINATE_WORD_COUNT)) =
                read_second_fixed16_text_line(stream);
              DASHBOARD_FIELD(definition,
                              DASHBOARD_FIELD_2C4 + (record_index * sizeof(int32) *
                                                     DASHBOARD_RESOURCE_COORDINATE_WORD_COUNT)) =
                read_second_fixed16_text_line(stream);
            }

          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_308) =
            (read_second_fixed16_text_line(stream) >> DASHBOARD_RESOURCE_FIXED16_SHIFT);
          DASHBOARD_FIELD(definition, DASHBOARD_FIELD_30C) =
            (read_second_fixed16_text_line(stream) >> DASHBOARD_RESOURCE_FIXED16_SHIFT);
          (void) close_disk_stream_and_return_zero(stream);

          if(initialize_camera_io() != 0)
            {
              copy_string(resource_path, (const char *) resource_definition_);
              append_string(resource_path, DASHBOARD_RESOURCE_CACHE_SUFFIX);
              (void) WriteMacFile(resource_path, definition, DASHBOARD_RESOURCE_CACHE_BYTES);
            }
        }
    }

  definition = state->definition;
  if(DASHBOARD_FIELD(definition, DASHBOARD_FIELD_30C) == DASHBOARD_RESOURCE_MODE_EXTENDED)
    {
      initialize_driving_warning_records(resource_definition_,
                                         DASHBOARD_RESOURCE_EXTENDED_SELECTION);
    }
  else
    {
      initialize_driving_warning_records(resource_definition_,
                                         DASHBOARD_RESOURCE_DEFAULT_SELECTION);
    }

  link_driving_warning_records(resource_definition_,
                               definition->record_count +
                                 DASHBOARD_FIELD(definition, DASHBOARD_FIELD_308) +
                                 DASHBOARD_RESOURCE_LINK_BASE);
  copy_string((char *) g_DRIVING_WARNING_DISPLAY_RESOURCE_DEFINITION,
              (const char *) resource_definition_);
  state->first_record              = NULL;
  state->second_record             = NULL;
  state->render_record_a           = NULL;
  state->render_record_b           = NULL;
  state->render_record_c           = NULL;
  state->render_record_d           = NULL;
  state->render_record_e           = NULL;
  state->render_record_f           = NULL;
  state->render_record_g           = NULL;
  state->render_record_h           = NULL;
  state->resource_constants_loaded = 0;

  if(option_ != 0)
    state->render_selection = DASHBOARD_FIELD(definition, DASHBOARD_FIELD_1EC);
}
