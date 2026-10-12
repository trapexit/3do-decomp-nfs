#pragma once

#include "simulation_snapshot.h"
#include "vehicle_parameter_loader.h"

void
apply_vehicle_boundary_collision(VehicleParameterState *state_,
                                 int32                  boundary_heading_,
                                 int32                  collision_speed_,
                                 int32                  collision_side_,
                                 int32                  collision_mode_);
void
apply_nonplayer_vehicle_boundary_collision(SimulationSource *source_,
                                           int32             boundary_heading_,
                                           int32             collision_speed_,
                                           int32             collision_side_,
                                           int32             collision_mode_);
