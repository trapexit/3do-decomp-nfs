#include "horizon_resources.h"

#include "cel_access.h"
#include "disk_stream_cleanup.h"
#include "fixed_math.h"
#include "formatted_output.h"
#include "game_bootstrap.h"
#include "game_data_path.h"
#include "horizon_render.h"
#include "memory.h"
#include "pointer_array.h"
#include "render_thread_signal.h"
#include "string_utilities.h"
#include "text_line_reader.h"
#include "vehicle_drive_statistics.h"

#include "stdlib.h"

#define HORIZON_CONFIGURATION_PRESET_COUNT 9
#define HORIZON_CONFIGURATION_VALUE_COUNT 14
#define HORIZON_CONFIGURATION_PRESETS_BYTES 0x1F8
#define HORIZON_INFO_PATH_BYTES 128
#define HORIZON_INFO_LINE_BYTES 128
#define HORIZON_INFO_LINE_READ_LIMIT 127
#define HORIZON_FILL_COMPONENT_SHIFT_MOST 10
#define HORIZON_FILL_COMPONENT_SHIFT_MIDDLE 5
#define HORIZON_FIRST_RESOURCE_PACKET_INDEX 0
#define HORIZON_SECOND_RESOURCE_PACKET_INDEX 1
#define HORIZON_FIRST_CEL_DIMENSION_INDEX 6
#define HORIZON_CITY_TRACK_INDEX 1
#define HORIZON_ALPINE_TRACK_INDEX 0
#define HORIZON_COASTAL_TRACK_INDEX 2
#define HORIZON_FIRST_EVENT_INDEX 0
#define HORIZON_SECOND_EVENT_INDEX 1
#define HORIZON_THIRD_EVENT_INDEX 2
#define HORIZON_ALPINE_THIRD_VARIANT 1
#define HORIZON_PRESET_CITY_ONE 0
#define HORIZON_PRESET_CITY_TWO 1
#define HORIZON_PRESET_CITY_THREE 2
#define HORIZON_PRESET_ALPINE_ONE 3
#define HORIZON_PRESET_ALPINE_TWO 4
#define HORIZON_PRESET_ALPINE_THREE 5
#define HORIZON_PRESET_COASTAL_ONE 6
#define HORIZON_PRESET_COASTAL_TWO 7
#define HORIZON_PRESET_COASTAL_THREE 8
#define HORIZON_FIXED16_SHIFT 16
#define HORIZON_CEL_DIMENSION_MASK 0xFF

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

enum
{
  HORIZON_PRESET_FIRST_ORIGIN_Y,
  HORIZON_PRESET_SECOND_ORIGIN_Y,
  HORIZON_PRESET_FIRST_ORIGIN_Z,
  HORIZON_PRESET_SECOND_ORIGIN_Z,
  HORIZON_PRESET_FIRST_VERTICAL_OFFSET,
  HORIZON_PRESET_SECOND_VERTICAL_OFFSET,
  HORIZON_PRESET_FIRST_RADIUS,
  HORIZON_PRESET_SECOND_RADIUS,
  HORIZON_PRESET_FIRST_FILL_MOST,
  HORIZON_PRESET_FIRST_FILL_MIDDLE,
  HORIZON_PRESET_FIRST_FILL_LEAST,
  HORIZON_PRESET_SECOND_FILL_MOST,
  HORIZON_PRESET_SECOND_FILL_MIDDLE,
  HORIZON_PRESET_SECOND_FILL_LEAST
};

int32 g_HORIZON_CONFIGURATION_PRESETS[HORIZON_CONFIGURATION_PRESET_COUNT]
                                     [HORIZON_CONFIGURATION_VALUE_COUNT] = {
                                       { -15, 0, 0, 0, 30, 0, 64, 64, 9, 9, 20, 3, 12, 11 },
                                       { -15, 0, 0, 0, 30, 0, 64, 64, 9, 9, 20, 3, 10, 13 },
                                       { -15, 0, 0, 0, 30, 0, 64, 64, 9, 9, 20, 3, 3, 16 },
                                       { -4, 0, 7, 2, 6, 17, 30, 64, 4, 8, 18, 5, 5, 19 },
                                       { -1, 6, 7, 2, 6, 17, 30, 64, 4, 8, 18, 5, 5, 19 },
                                       { -3, 0, 7, 2, 6, 17, 30, 64, 4, 8, 18, 28, 28, 28 },
                                       { -1, 3, 7, 2, 8, 20, 30, 64, 16, 25, 30, 5, 8, 3 },
                                       { -1, 2, 7, 2, 4, 20, 20, 64, 16, 25, 30, 20, 22, 24 },
                                       { -1, 2, 7, 2, 4, 20, 20, 64, 16, 25, 30, 12, 12, 4 }
                                     };

COMPILE_TIME_SIZE_ASSERT(horizon_configuration_presets_size,
                         sizeof(g_HORIZON_CONFIGURATION_PRESETS) == 0x1F8);

HorizonRing *g_HORIZON_RINGS[HORIZON_RING_COUNT];
void        *g_HORIZON_CEL_RESOURCE;
int32        g_HORIZON_SCREEN_POSITION;
int32        g_HORIZON_CONFIGURATION_VARIANT;
int32        g_HORIZON_PROJECTION_SCALE_FIXED = 0x00E480F1;
int32        g_HORIZON_VERTICAL_ORIGIN_FIXED;
int32        g_HORIZON_BITMAP_PAGE_COUNT = 75;

void
free_horizon_ring_memory(void *memory_)
{
  free_memory(memory_);
}

void
free_horizon_ring_resources(void)
{
  free_horizon_ring_memory(g_HORIZON_RINGS[0]);
  free_horizon_ring_memory(g_HORIZON_RINGS[1]);
}

void
initialize_horizon_configuration(const char *track_name_)
{
  int32              *preset;
  HorizonRenderState *state;
  Vector3             first_origin;
  Vector3             second_origin;
  CCB                *cel;
  char                info_path[HORIZON_INFO_PATH_BYTES];
  char                line[HORIZON_INFO_LINE_BYTES];
  char                token[HORIZON_INFO_LINE_BYTES];
  const char         *cursor;
  Stream             *stream;

  g_HORIZON_CONFIGURATION_VARIANT = 0;
  if(g_STARTUP_HELP_MODE == 0)
    {
      preset = NULL;
      if((compare_strings((const uint8 *) track_name_, (const uint8 *) "cl1") == 0) ||
         (compare_strings((const uint8 *) track_name_, (const uint8 *) "Cl1") == 0))
        {
          g_TRACK_INDEX = HORIZON_CITY_TRACK_INDEX;
          g_EVENT_INDEX = HORIZON_FIRST_EVENT_INDEX;
          preset        = g_HORIZON_CONFIGURATION_PRESETS[HORIZON_PRESET_CITY_ONE];
        }
      if((compare_strings((const uint8 *) track_name_, (const uint8 *) "cl2") == 0) ||
         (compare_strings((const uint8 *) track_name_, (const uint8 *) "Cl2") == 0))
        {
          g_TRACK_INDEX = HORIZON_CITY_TRACK_INDEX;
          g_EVENT_INDEX = HORIZON_SECOND_EVENT_INDEX;
          preset        = g_HORIZON_CONFIGURATION_PRESETS[HORIZON_PRESET_CITY_TWO];
        }
      if((compare_strings((const uint8 *) track_name_, (const uint8 *) "cl3") == 0) ||
         (compare_strings((const uint8 *) track_name_, (const uint8 *) "Cl3") == 0))
        {
          g_TRACK_INDEX = HORIZON_CITY_TRACK_INDEX;
          g_EVENT_INDEX = HORIZON_THIRD_EVENT_INDEX;
          preset        = g_HORIZON_CONFIGURATION_PRESETS[HORIZON_PRESET_CITY_THREE];
        }
      if((compare_strings((const uint8 *) track_name_, (const uint8 *) "al1") == 0) ||
         (compare_strings((const uint8 *) track_name_, (const uint8 *) "Al1") == 0))
        {
          g_TRACK_INDEX = HORIZON_ALPINE_TRACK_INDEX;
          g_EVENT_INDEX = HORIZON_FIRST_EVENT_INDEX;
          preset        = g_HORIZON_CONFIGURATION_PRESETS[HORIZON_PRESET_ALPINE_ONE];
        }
      if((compare_strings((const uint8 *) track_name_, (const uint8 *) "al2") == 0) ||
         (compare_strings((const uint8 *) track_name_, (const uint8 *) "Al2") == 0))
        {
          g_TRACK_INDEX = HORIZON_ALPINE_TRACK_INDEX;
          g_EVENT_INDEX = HORIZON_SECOND_EVENT_INDEX;
          preset        = g_HORIZON_CONFIGURATION_PRESETS[HORIZON_PRESET_ALPINE_TWO];
        }
      if((compare_strings((const uint8 *) track_name_, (const uint8 *) "al3") == 0) ||
         (compare_strings((const uint8 *) track_name_, (const uint8 *) "Al3") == 0))
        {
          g_HORIZON_CONFIGURATION_VARIANT = HORIZON_ALPINE_THIRD_VARIANT;
          g_TRACK_INDEX                   = HORIZON_ALPINE_TRACK_INDEX;
          g_EVENT_INDEX                   = HORIZON_THIRD_EVENT_INDEX;
          preset = g_HORIZON_CONFIGURATION_PRESETS[HORIZON_PRESET_ALPINE_THREE];
        }
      if((compare_strings((const uint8 *) track_name_, (const uint8 *) "cy1") == 0) ||
         (compare_strings((const uint8 *) track_name_, (const uint8 *) "Cy1") == 0))
        {
          g_TRACK_INDEX = HORIZON_COASTAL_TRACK_INDEX;
          g_EVENT_INDEX = HORIZON_FIRST_EVENT_INDEX;
          preset        = g_HORIZON_CONFIGURATION_PRESETS[HORIZON_PRESET_COASTAL_ONE];
        }
      if((compare_strings((const uint8 *) track_name_, (const uint8 *) "cy2") == 0) ||
         (compare_strings((const uint8 *) track_name_, (const uint8 *) "Cy2") == 0))
        {
          g_TRACK_INDEX = HORIZON_COASTAL_TRACK_INDEX;
          g_EVENT_INDEX = HORIZON_SECOND_EVENT_INDEX;
          preset        = g_HORIZON_CONFIGURATION_PRESETS[HORIZON_PRESET_COASTAL_TWO];
        }
      if((compare_strings((const uint8 *) track_name_, (const uint8 *) "cy3") == 0) ||
         (compare_strings((const uint8 *) track_name_, (const uint8 *) "Cy3") == 0))
        {
          g_TRACK_INDEX = HORIZON_COASTAL_TRACK_INDEX;
          g_EVENT_INDEX = HORIZON_THIRD_EVENT_INDEX;
          preset        = g_HORIZON_CONFIGURATION_PRESETS[HORIZON_PRESET_COASTAL_THREE];
        }
      if((compare_strings((const uint8 *) track_name_, (const uint8 *) "test1") == 0) ||
         (compare_strings((const uint8 *) track_name_, (const uint8 *) "Test1") == 0))
        preset = g_HORIZON_CONFIGURATION_PRESETS[HORIZON_PRESET_ALPINE_ONE];
      if((compare_strings((const uint8 *) track_name_, (const uint8 *) "test2") == 0) ||
         (compare_strings((const uint8 *) track_name_, (const uint8 *) "Test2") == 0))
        preset = g_HORIZON_CONFIGURATION_PRESETS[HORIZON_PRESET_ALPINE_ONE];
      if((compare_strings((const uint8 *) track_name_, (const uint8 *) "test3") == 0) ||
         (compare_strings((const uint8 *) track_name_, (const uint8 *) "Test3") == 0))
        preset = g_HORIZON_CONFIGURATION_PRESETS[HORIZON_PRESET_ALPINE_ONE];

      state                       = &g_HORIZON_RENDER_STATE;
      state->first_ring_origin_y  = preset[HORIZON_PRESET_FIRST_ORIGIN_Y] << HORIZON_FIXED16_SHIFT;
      state->second_ring_origin_y = preset[HORIZON_PRESET_SECOND_ORIGIN_Y] << HORIZON_FIXED16_SHIFT;
      state->first_ring_origin_z  = preset[HORIZON_PRESET_FIRST_ORIGIN_Z] << HORIZON_FIXED16_SHIFT;
      state->second_ring_origin_z = preset[HORIZON_PRESET_SECOND_ORIGIN_Z] << HORIZON_FIXED16_SHIFT;
      state->first_ring_vertical_offset  = preset[HORIZON_PRESET_FIRST_VERTICAL_OFFSET]
                                        << HORIZON_FIXED16_SHIFT;
      state->second_ring_vertical_offset = preset[HORIZON_PRESET_SECOND_VERTICAL_OFFSET]
                                        << HORIZON_FIXED16_SHIFT;
      state->first_ring_radius_fixed = preset[HORIZON_PRESET_FIRST_RADIUS] << HORIZON_FIXED16_SHIFT;
      state->second_ring_radius_fixed = preset[HORIZON_PRESET_SECOND_RADIUS]
                                     << HORIZON_FIXED16_SHIFT;
      state->primary_fill_value =
        ((uint32) preset[HORIZON_PRESET_FIRST_FILL_MOST] << HORIZON_FILL_COMPONENT_SHIFT_MOST) |
        ((uint32) preset[HORIZON_PRESET_FIRST_FILL_MIDDLE] << HORIZON_FILL_COMPONENT_SHIFT_MIDDLE) |
        (uint32) preset[HORIZON_PRESET_FIRST_FILL_LEAST];
      state->secondary_fill_value =
        ((uint32) preset[HORIZON_PRESET_SECOND_FILL_MOST] << HORIZON_FILL_COMPONENT_SHIFT_MOST) |
        ((uint32) preset[HORIZON_PRESET_SECOND_FILL_MIDDLE]
         << HORIZON_FILL_COMPONENT_SHIFT_MIDDLE) |
        (uint32) preset[HORIZON_PRESET_SECOND_FILL_LEAST];
    }
  else
    {
      sprintf(info_path,
              "%s%s%sInfo",
              g_GAME_DATA_PATH,
              "DriveData/DriveArt/Horizons/",
              track_name_);
      stream = open_disk_stream_default(info_path);
      state  = &g_HORIZON_RENDER_STATE;
      read_noncomment_text_line(line, HORIZON_INFO_LINE_READ_LIMIT, stream);
      state->first_ring_origin_y = parse_fixed16_decimal(line);
      read_noncomment_text_line(line, HORIZON_INFO_LINE_READ_LIMIT, stream);
      state->second_ring_origin_y = parse_fixed16_decimal(line);
      read_noncomment_text_line(line, HORIZON_INFO_LINE_READ_LIMIT, stream);
      state->first_ring_origin_z = parse_fixed16_decimal(line);
      read_noncomment_text_line(line, HORIZON_INFO_LINE_READ_LIMIT, stream);
      state->second_ring_origin_z = parse_fixed16_decimal(line);
      read_noncomment_text_line(line, HORIZON_INFO_LINE_READ_LIMIT, stream);
      state->first_ring_vertical_offset = parse_fixed16_decimal(line);
      read_noncomment_text_line(line, HORIZON_INFO_LINE_READ_LIMIT, stream);
      state->second_ring_vertical_offset = parse_fixed16_decimal(line);
      read_noncomment_text_line(line, HORIZON_INFO_LINE_READ_LIMIT, stream);
      state->first_ring_radius_fixed = parse_fixed16_decimal(line);
      read_noncomment_text_line(line, HORIZON_INFO_LINE_READ_LIMIT, stream);
      state->second_ring_radius_fixed = parse_fixed16_decimal(line);
      read_noncomment_text_line(line, HORIZON_INFO_LINE_READ_LIMIT, stream);
      copy_text_token(token, line);
      state->primary_fill_value = (uint32) atoi(token) << HORIZON_FILL_COMPONENT_SHIFT_MOST;
      cursor                    = skip_text_token(line);
      copy_text_token(token, cursor);
      state->primary_fill_value |= (uint32) atoi(token) << HORIZON_FILL_COMPONENT_SHIFT_MIDDLE;
      cursor = skip_text_token(cursor);
      copy_text_token(token, cursor);
      state->primary_fill_value |= (uint32) atoi(token);
      read_noncomment_text_line(line, HORIZON_INFO_LINE_READ_LIMIT, stream);
      copy_text_token(token, line);
      state->secondary_fill_value = (uint32) atoi(token) << HORIZON_FILL_COMPONENT_SHIFT_MOST;
      cursor                      = skip_text_token(line);
      copy_text_token(token, cursor);
      state->secondary_fill_value |= (uint32) atoi(token) << HORIZON_FILL_COMPONENT_SHIFT_MIDDLE;
      cursor = skip_text_token(cursor);
      copy_text_token(token, cursor);
      state->secondary_fill_value |= (uint32) atoi(token);
      close_disk_stream_and_return_zero(stream);
    }

  first_origin.x = 0;
  first_origin.y = state->first_ring_origin_y;
  first_origin.z = state->first_ring_origin_z;
  sprintf(info_path, "%s%s", track_name_, "Near");
  g_HORIZON_RINGS[0] = create_horizon_ring(state->first_ring_vertical_offset,
                                           state->first_ring_radius_fixed,
                                           info_path,
                                           &first_origin,
                                           HORIZON_FIRST_RESOURCE_PACKET_INDEX);
  cel                = (CCB *) get_pointer_array_element((void *const *) g_HORIZON_CEL_RESOURCE,
                                                         HORIZON_FIRST_CEL_DIMENSION_INDEX);
  g_RENDER_PIPELINE_CCB_FLAGS = &cel->ccb_Flags;
  cel->ccb_Width              = (int32) 1 << ((uint32) cel->ccb_Width & HORIZON_CEL_DIMENSION_MASK);
  cel->ccb_Height = (int32) 1 << ((uint32) cel->ccb_Height & HORIZON_CEL_DIMENSION_MASK);

  second_origin.x = 0;
  second_origin.y = state->second_ring_origin_y;
  second_origin.z = state->second_ring_origin_z;
  sprintf(info_path, "%s%s", track_name_, "Far");
  g_HORIZON_RINGS[1] = create_horizon_ring(state->second_ring_vertical_offset,
                                           state->second_ring_radius_fixed,
                                           info_path,
                                           &second_origin,
                                           HORIZON_SECOND_RESOURCE_PACKET_INDEX);
}
