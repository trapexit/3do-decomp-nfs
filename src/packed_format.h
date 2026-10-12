#pragma once

#include "types.h"

#define PACKED_CALLBACK_COUNT 9

typedef enum PackedFormat
{
  PACKED_FORMAT_UNKNOWN         = 0,
  PACKED_FORMAT_6_E             = 1,
  PACKED_FORMAT_7_A             = 2,
  PACKED_FORMAT_7_A_VARIANT_1_1 = 3,
  PACKED_FORMAT_7_A_VARIANT_2_1 = 4,
  PACKED_FORMAT_46              = 5,
  PACKED_FORMAT_30              = 7,
  PACKED_FORMAT_32              = 9,
  PACKED_FORMAT_34              = 10,
  PACKED_FORMAT_72              = 11,
  PACKED_FORMAT_10_78           = 12,
  PACKED_FORMAT_74              = 16,
  PACKED_FORMAT_70              = 17,
  PACKED_FORMAT_UNRECOGNIZED    = 18,
  PACKED_FORMAT_76              = 20,
  PACKED_FORMAT_76_VARIANT_1_1  = 21,
  PACKED_FORMAT_76_VARIANT_2_1  = 22,
  PACKED_FORMAT_42_44           = 23,
  PACKED_FORMAT_26_2_A          = 24,
  PACKED_FORMAT_2_C             = 25,
  PACKED_FORMAT_28_2_E          = 26,
  PACKED_FORMAT_66              = 27,
  PACKED_FORMAT_6_A_C0          = 31,
  PACKED_FORMAT_LEGACY          = 33
} PackedFormat;

typedef int32 (*PackedCallback)(void);

extern PackedCallback g_PACKED_CALLBACKS[PACKED_CALLBACK_COUNT];

int32
packed_callback_default(void);
int32
get_fixed_packed_data_size(void);
PackedFormat
identify_packed_format(const uint8 *source_);
int32
read_packed_data_size_field(const uint8 *source_);
int32
get_packed_data_size(const uint8 *source_, int32 source_size_);
