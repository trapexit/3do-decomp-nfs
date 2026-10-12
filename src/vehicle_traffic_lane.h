#pragma once

#include "vehicle_simulation_state.h"

#include "types.h"

struct DoublyLinkedList;

#define VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT 3
#define VEHICLE_TRAFFIC_EVENT_PROBABILITY_COUNT 8
#define VEHICLE_TRACK_FEATURE_PROBABILITY_COUNT 4
#define VEHICLE_TRAFFIC_REFRESH_MODE_COUNT 8

typedef struct VehicleTrafficLaneNeighbor
{
  void                   *reserved_00;
  VehicleSimulationState *vehicle;
} VehicleTrafficLaneNeighbor;

void
collect_traffic_lane_neighbor_limits(
  VehicleSimulationState        *state_,
  const struct DoublyLinkedList *vehicle_list_,
  const uint32                   vehicle_lane_masks_[VEHICLE_SIMULATION_STATE_COUNT],
  int32                          current_lane_,
  uint32                        *matched_lane_mask_,
  int32                          primary_speed_limits_[VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT],
  int32                          secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT],
  VehicleSimulationState        *lane_neighbors_[VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT]);
void
trigger_vehicle_track_collision_event(VehicleSimulationState *state_);
void
advance_vehicle_track_feature_event(void);
void
update_vehicle_overtake_statistics(void);
void
update_racer_proximity_event(VehicleSimulationState *state_,
                             int32                   lane_index_,
                             Vector3                *avoidance_offset_);
void
update_traffic_vehicle_lane_response(VehicleSimulationState *state_,
                                     int32                   lane_index_,
                                     Vector3                *avoidance_offset_);
void
score_traffic_lane_speed_limits(
  VehicleSimulationState *state_,
  const int32             primary_speed_limits_[VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT],
  const int32             secondary_speed_limits_[VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT],
  int32                   lane_scores_[VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT]);
void
constrain_traffic_vehicle_lane_speed(
  VehicleSimulationState           *state_,
  const VehicleTrafficLaneNeighbor *neighbor_,
  int32                             current_lane_,
  int32                             selected_lane_,
  const int32                       speed_limits_[VEHICLE_TRAFFIC_LANE_SPEED_LIMIT_COUNT]);
void
refresh_vehicle_traffic_lane_state(VehicleSimulationState *state_);
void
coordinate_vehicle_traffic_lanes(const struct DoublyLinkedList *vehicle_list_);

extern int32 g_VEHICLE_TRAFFIC_EVENT_PROBABILITIES[VEHICLE_TRAFFIC_EVENT_PROBABILITY_COUNT];
extern int32 g_VEHICLE_TRACK_FEATURE_PROBABILITIES[VEHICLE_TRACK_FEATURE_PROBABILITY_COUNT];
extern int32 g_VEHICLE_TRAFFIC_REFRESH_MODES[VEHICLE_TRAFFIC_REFRESH_MODE_COUNT];
extern int32 g_VEHICLE_TRAFFIC_COORDINATION_DISABLED;
extern int32 g_VEHICLE_TRAFFIC_ACTIVE_LANE_INDEX;
extern int32 g_VEHICLE_TRAFFIC_LANE_CONSTRAINTS_ENABLED;
extern int32 g_VEHICLE_TRAFFIC_COORDINATION_COUNTER;
extern int32 g_VEHICLE_LAST_TRACK_FEATURE_SEGMENT;
