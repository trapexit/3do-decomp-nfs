#include "vehicle_index_slots.h"

void
clear_vehicle_index_slots(VehicleIndexSlots *vehicle_state_)
{
  int32 index;

  for(index = 0; index < VEHICLE_INDEX_SLOT_COUNT; index++)
    vehicle_state_->indices[index] = -1;
}
