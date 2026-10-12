#include "vehicle_status_records.h"

#include "frontend_transition.h"
#include "geometry.h"

#define TRACK_SEGMENT_FLAG_ACTIVE 1
#define TRACK_SEGMENT_FLAG_HIGH_SHIFT 8

int32
count_active_track_segments(void)
{
  int32 active_record_count;
  int32 record_index;
  uint16 status_flags;

  active_record_count = 0;
  for(record_index = 0; record_index < g_GAME_RUNTIME_STATE.track_segment_count; record_index++)
    {
      status_flags =
        (uint16) (((uint16) g_REFERENCE_DISTANCE_RECORDS[record_index].traffic_event_flags_high
                   << TRACK_SEGMENT_FLAG_HIGH_SHIFT) |
                  (uint16) g_REFERENCE_DISTANCE_RECORDS[record_index].traffic_event_flags_low);
      active_record_count += ((status_flags & TRACK_SEGMENT_FLAG_ACTIVE) != 0);
    }

  return active_record_count;
}
