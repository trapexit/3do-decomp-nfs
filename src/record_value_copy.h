#pragma once

#include "types.h"

#define RECORD_VALUE_COUNT 4

typedef struct RecordValueBlock
{
  int32 values[RECORD_VALUE_COUNT];
} RecordValueBlock;

typedef struct RecordValueNode
{
  int32 byte_count;
  uint8 reserved_04[0x0C];
  uint8 type;
  uint8 reserved_11[0x0B];
  int32 values[RECORD_VALUE_COUNT];
} RecordValueNode;

typedef struct RecordValueGroup
{
  int32 type;
} RecordValueGroup;

extern RecordValueBlock g_CURRENT_RECORD_VALUES;

void
copy_matching_record_values(const RecordValueGroup *group_);
