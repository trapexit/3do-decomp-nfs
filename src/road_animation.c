/* Advances the vehicle render schedule while the road animation frame changes. */

#include "simulation_snapshot.h"
#include "vehicle_render_resource_selection.h"
#include "vehicle_runtime_pool.h"

#define ROAD_ANIMATION_FRACTION_NEGATIVE_OFFSET (-0x29999)
#define ROAD_ANIMATION_FRACTION_POSITIVE_OFFSET 0x29999
#define ROAD_ANIMATION_FRACTION_LIMIT 0x30001
#define ROAD_ANIMATION_REVERSE_FRAME_ADJUSTMENT 1
#define ROAD_ANIMATION_FORWARD_FRAME_ADJUSTMENT 1
#define ROAD_ANIMATION_FORWARD_FRACTION_ADJUSTMENT 2
#define VEHICLE_ANIMATION_STATE_LIMIT 20

VehicleSimulationState *g_VEHICLE_ANIMATION_STATES[VEHICLE_ANIMATION_STATE_LIMIT];
int32                   g_VEHICLE_ANIMATION_STATE_COUNT;

void
advance_frontend_animation_step(int32 step_)
{
  VehicleSimulationState *vehicle;
  int32                   vehicle_frame;

  if(g_VEHICLE_ROAD_ANIMATION_STATE.reverse_animation_frame < step_)
    {
      g_VEHICLE_ROAD_ANIMATION_STATE.reverse_vehicle_index =
        g_VEHICLE_RUNTIME_POOL_STATE.render_object_count - 1;
      g_VEHICLE_ROAD_ANIMATION_STATE.reverse_active_index = 0;
    }

  while(g_VEHICLE_ROAD_ANIMATION_STATE.reverse_vehicle_index >= 0)
    {
      vehicle = g_VEHICLE_ANIMATION_STATES[g_VEHICLE_ROAD_ANIMATION_STATE.reverse_vehicle_index];
      g_VEHICLE_ROAD_ANIMATION_STATE.reverse_vehicle = vehicle;
      g_VEHICLE_ROAD_ANIMATION_STATE.reverse_active_index =
        g_VEHICLE_ROAD_ANIMATION_STATE.reverse_vehicle_index;
      vehicle_frame = vehicle->simulation_source.alternate_track_segment_index;
      if((vehicle->simulation_source.alternate_track_position_fraction +
          ROAD_ANIMATION_FRACTION_NEGATIVE_OFFSET) < 0)
        {
          vehicle_frame -= ROAD_ANIMATION_REVERSE_FRAME_ADJUSTMENT;
        }
      if(vehicle_frame < step_)
        break;
      if((vehicle_frame == step_) &&
         ((vehicle != (VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE) ||
          (g_SIMULATION_RENDER_STATE.render_mode != 0)))
        {
          (void) prepare_vehicle_road_animation(vehicle, 0);
          (void) render_vehicle_road_animation(vehicle, 0);
        }
      g_VEHICLE_ROAD_ANIMATION_STATE.reverse_vehicle_index--;
    }

  g_VEHICLE_ROAD_ANIMATION_STATE.reverse_animation_frame = step_;
}

void
advance_secondary_road_animation(int32 frame_index_)
{
  VehicleSimulationState *vehicle;
  int32                   vehicle_frame;

  if(frame_index_ < g_VEHICLE_ROAD_ANIMATION_STATE.forward_animation_frame)
    {
      g_VEHICLE_ROAD_ANIMATION_STATE.forward_vehicle_index = 0;
    }

  while(g_VEHICLE_ROAD_ANIMATION_STATE.forward_vehicle_index <
        g_VEHICLE_RUNTIME_POOL_STATE.render_object_count)
    {
      vehicle = g_VEHICLE_ANIMATION_STATES[g_VEHICLE_ROAD_ANIMATION_STATE.forward_vehicle_index];
      g_VEHICLE_ROAD_ANIMATION_STATE.forward_vehicle = vehicle;
      if((vehicle->simulation_source.alternate_track_position_fraction +
          ROAD_ANIMATION_FRACTION_POSITIVE_OFFSET) < ROAD_ANIMATION_FRACTION_LIMIT)
        {
          vehicle_frame = vehicle->simulation_source.alternate_track_segment_index +
                          ROAD_ANIMATION_FORWARD_FRAME_ADJUSTMENT;
        }
      else
        {
          vehicle_frame = vehicle->simulation_source.alternate_track_segment_index +
                          ROAD_ANIMATION_FORWARD_FRACTION_ADJUSTMENT;
        }
      if(frame_index_ < vehicle_frame)
        break;
      if((vehicle_frame == frame_index_) &&
         ((vehicle != (VehicleSimulationState *) G_ACTIVE_SIMULATION_SOURCE) ||
          (g_SIMULATION_RENDER_STATE.render_mode != 0)))
        {
          (void) prepare_vehicle_road_animation(vehicle, 1);
          (void) render_vehicle_road_animation(vehicle, 1);
        }
      g_VEHICLE_ROAD_ANIMATION_STATE.forward_vehicle_index++;
    }

  g_VEHICLE_ROAD_ANIMATION_STATE.forward_animation_frame = frame_index_;
}
