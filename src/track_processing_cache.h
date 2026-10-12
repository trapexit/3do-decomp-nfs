#pragma once

#include "types.h"

typedef struct TrackProcessingCache
{
  int32 current_section_index;
  int32 track_section_position;
  int32 geometry_start_section;
  int32 section_batch_count;
} TrackProcessingCache;

extern TrackProcessingCache g_TRACK_PROCESSING_CACHE;

void
invalidate_track_processing_cache(void);
