#pragma once

#include "vehicle_parameter_loader.h"

#include "types.h"

void
adjust_vehicle_control_event_value(VehicleParameterState *state_,
                                   int32                  event_value_,
                                   int32                  input_limit_);
