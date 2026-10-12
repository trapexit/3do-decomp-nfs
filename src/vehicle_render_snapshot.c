#include "vehicle_render_snapshot.h"

void
capture_vehicle_render_snapshot(VehicleRenderSnapshotState *state_)
{
  int32 value_index;

  state_->render_position = state_->current_position;
  state_->render_vector   = state_->current_vector;
  state_->render_value_a  = state_->current_value_a;
  state_->render_mode     = state_->current_mode;
  state_->render_value_c  = state_->current_value_c;
  state_->render_value_b  = state_->current_value_b;

  for(value_index = 0; value_index < VEHICLE_RENDER_VALUE_COUNT; value_index++)
    state_->render_values[value_index] = state_->current_values[value_index];
}
