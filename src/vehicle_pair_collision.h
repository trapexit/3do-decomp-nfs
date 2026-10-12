#pragma once

#include "vehicle_runtime_pool.h"

#include "types.h"

extern int32 g_VEHICLE_COLLISION_SOUND_TIME;
extern int32 g_VEHICLE_COLLISION_STATISTICS_SUPPRESSED;
extern int32 g_VEHICLE_COLLISION_SCORE;
extern int32 g_VEHICLE_COLLISION_SOUND_CODES[16];

void
apply_vehicle_collision_recovery_impulse(VehicleRuntimeObjectState *vehicle_);
void
update_vehicle_runtime_interactions(DoublyLinkedListStorage *storage_);
int32
process_vehicle_pair_collision(VehicleRuntimeObjectState *first_vehicle_,
                               VehicleRuntimeObjectState *second_vehicle_);
int32
resolve_vehicle_pair_collision(VehicleRuntimeObjectState *first_vehicle_,
                               VehicleRuntimeObjectState *second_vehicle_);
