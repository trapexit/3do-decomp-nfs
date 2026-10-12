#pragma once

#include "vehicle_parameter_loader.h"

#include "types.h"

extern int32 g_VEHICLE_ACCELERATE_BUTTON_MASK;
extern int32 g_VEHICLE_BRAKE_BUTTON_MASK;
extern int32 g_VEHICLE_GEAR_UP_BUTTON_MASK;
extern int32 g_VEHICLE_GEAR_TOGGLE_BUTTON_MASK;
extern int32 g_VEHICLE_AUXILIARY_BUTTON_MASK;

void
adjust_vehicle_steering_control(VehicleParameterState *state_,
                                uint32                 button_states_,
                                int32                  analog_target_);
void
apply_vehicle_digital_control_inputs(VehicleParameterState *state_, int32 button_states_);
int32
calculate_vehicle_control_heading(VehicleParameterState *state_);
void
update_player_vehicle_control_mode(VehicleParameterState *state_);
