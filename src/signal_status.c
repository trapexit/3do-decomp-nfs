#include "signal_status.h"

#include "memory.h"
#include "road_processing.h"
#include "track_disk_stream.h"

#include "stddef.h"

#define RUNTIME_STATE_LINEAR_SLEEP_THRESHOLD 0x0000CCCC
#define RUNTIME_STATE_ANGULAR_SLEEP_THRESHOLD 0x00013333
#define RUNTIME_STATE_ORIENTATION_SLEEP_THRESHOLD 0x000008F5
#define RUNTIME_STATE_COLLISION_THRESHOLD_DEFAULT 0x000D0000
#define RUNTIME_STATE_COLLISION_THRESHOLD_PLAYER 0x00100000
#define RUNTIME_STATE_FIELD_2C_INITIAL_VALUE 0x00010000
#define RUNTIME_STATE_FIELD_30_INITIAL_VALUE 0x00034000
#define RUNTIME_STATE_FIELD_38_INITIAL_VALUE 0x00006666

RuntimeState g_RUNTIME_STATE = { RUNTIME_STATE_LINEAR_SLEEP_THRESHOLD,
                                 RUNTIME_STATE_ANGULAR_SLEEP_THRESHOLD,
                                 RUNTIME_STATE_ORIENTATION_SLEEP_THRESHOLD,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 RUNTIME_STATE_COLLISION_THRESHOLD_DEFAULT,
                                 RUNTIME_STATE_COLLISION_THRESHOLD_PLAYER,
                                 RUNTIME_STATE_FIELD_2C_INITIAL_VALUE,
                                 RUNTIME_STATE_FIELD_30_INITIAL_VALUE,
                                 0,
                                 RUNTIME_STATE_FIELD_38_INITIAL_VALUE };

typedef char RuntimeStateSize[(sizeof(RuntimeState) == 0x3C) ? 1 : -1];
typedef char RuntimeStateObjectSize[(sizeof(g_RUNTIME_STATE) == 0x3C) ? 1 : -1];
typedef char RuntimeStateRoadObjectSeekIndexOffset
  [(offsetof(RuntimeState, road_object_seek_record_index) == 0x14) ? 1 : -1];
typedef char RuntimeStateRoadObjectSeekIdOffset
  [(offsetof(RuntimeState, road_object_seek_record_id) == 0x18) ? 1 : -1];
typedef char RuntimeStateRoadObjectActivationIndexOffset
  [(offsetof(RuntimeState, road_object_activation_record_index) == 0x1C) ? 1 : -1];
typedef char RuntimeStateRoadObjectActivationIdOffset
  [(offsetof(RuntimeState, road_object_activation_record_id) == 0x20) ? 1 : -1];

#define G_SIGNAL_STATUSES ((SignalStatus *) g_ROAD_OBJECT_ORIENTATION_STATES)

void
initialize_signal_statuses(int32 signal_count_hint_)
{
  int32 signal_count;
  int32 signal_index;

  (void) signal_count_hint_;
  if(G_SIGNAL_STATUSES != NULL)
    return;

  signal_count = *(int32 *) ((uint8 *) G_TRACK_DATA + TRACK_SIGNAL_STATUS_COUNT_OFFSET);
  g_ROAD_OBJECT_ORIENTATION_STATES =
    (RoadObjectOrientationState *) allocate_named_memory("SignalStatus",
                                                         signal_count * sizeof(SignalStatus),
                                                         0);
  for(signal_index = 0; signal_index < signal_count; signal_index++)
    G_SIGNAL_STATUSES[signal_index].value = 0;
}

void
free_signal_statuses(void)
{
  free_memory(G_SIGNAL_STATUSES);
  g_ROAD_OBJECT_ORIENTATION_STATES = NULL;
}

void
reset_runtime_state(void)
{
  g_RUNTIME_STATE.road_object_seek_record_index       = 0;
  g_RUNTIME_STATE.road_object_seek_record_id          = 0;
  g_RUNTIME_STATE.road_object_activation_record_index = 0;
  g_RUNTIME_STATE.road_object_activation_record_id    = 0;
  g_RUNTIME_STATE.traffic_coordination_countdown      = 1;
  g_RUNTIME_STATE.collision_simulation_enabled        = 0;
  g_RUNTIME_STATE.negative_road_boundary_margin       = RUNTIME_STATE_FIELD_2C_INITIAL_VALUE;
  g_RUNTIME_STATE.positive_road_boundary_margin       = RUNTIME_STATE_FIELD_30_INITIAL_VALUE;
}
