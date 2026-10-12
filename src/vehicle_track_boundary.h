#pragma once

#include "simulation_snapshot.h"
#include "vehicle_parameter_loader.h"

void
update_player_vehicle_track_boundary(VehicleParameterState *state_);
int32
is_vehicle_outside_track_bounds(SimulationSource *source_);
