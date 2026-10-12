#include "track_processing_cache.h"

#include "simulation_snapshot.h"

#define TRACK_PROCESSING_CACHE_INVALID_INDEX (-1)

TrackProcessingCache g_TRACK_PROCESSING_CACHE;

void
invalidate_track_processing_cache(void)
{
  g_SIMULATION_RENDER_STATE.position_flags |= 1;
  g_TRACK_PROCESSING_CACHE.track_section_position = TRACK_PROCESSING_CACHE_INVALID_INDEX;
  g_TRACK_PROCESSING_CACHE.geometry_start_section = TRACK_PROCESSING_CACHE_INVALID_INDEX;
}
