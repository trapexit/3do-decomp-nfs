#include "player_vehicle_dynamics.h"
#include "simulation_snapshot.h"
#include "track_position.h"
#include "vehicle_callbacks.h"

extern void
update_vehicle_track_motion(SimulationSource *source_, int32 update_enabled_);

void
update_driving_warning_vehicle_state(void *vehicle_)
{
  SimulationSource *source;
  int32             track_segment_index;

  source = (SimulationSource *) vehicle_;

  update_player_vehicle_dynamics(source);
  track_segment_index = source->track_segment_index;
  (void) adjust_track_segment_index(&source->position, &track_segment_index);
  source->track_segment_index = track_segment_index;
  update_vehicle_track_motion(source, 1);
}
