#pragma once

#include "simulation_snapshot.h"
#include "vehicle_runtime_pool.h"

/* Retail aliases this handoff to the runtime pool's opponent slot. */
#define G_ACTIVE_VEHICLE_RUNTIME_RECORD ((void *) g_VEHICLE_RUNTIME_POOL_STATE.opponent_object)
extern int32   g_VEHICLE_COLLISION_RESPONSE_MAGNITUDE;
extern int32   g_VEHICLE_ORIENTATION_STABILITY_VALUE;
extern Vector3 g_VEHICLE_COLLISION_CONTACT_VELOCITY;
extern int32   g_VEHICLE_COLLISION_POSITION_CORRECTION_ENABLED;
extern int32   g_VEHICLE_SURFACE_COLLISION_NORMAL_SCALE;
extern int32   g_VEHICLE_SURFACE_COLLISION_FRICTION_SCALE;
extern int32   g_PLAYER_VEHICLE_RECOVERY_PENDING;
extern uint32  g_VEHICLE_MESH_DEFORMATION_RANDOM_STATE;
extern int32   g_VEHICLE_MESH_DEFORMATION_APPLIED;

void
update_player_vehicle_state(void *state_);
void
deform_vehicle_model_vertices_once(VehicleRuntimeObjectState *runtime_object_);
