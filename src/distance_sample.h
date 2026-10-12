#pragma once

#include "types.h"

typedef struct DistanceSample
{
  int32 distance;
  int32 value;
} DistanceSample;

typedef struct DistanceSampleTable
{
  uint8          reserved_00[0x84];
  int32          sample_count;
  uint8          reserved_88[0x28];
  DistanceSample samples[1];
} DistanceSampleTable;

int32
get_distance_sample_value(const DistanceSampleTable *table_, int32 distance_);
