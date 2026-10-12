#pragma once

#include "radix_sort.h"
#include "simulation_resource_cleanup.h"
#include "vehicle_simulation_state.h"

#include "types.h"

#define VEHICLE_DYNAMIC_CONFIGURATION_VALUE_COUNT 6
#define VEHICLE_DYNAMIC_CURVE_VALUE_COUNT 100

typedef struct VehicleDynamicRenderObject
{
  uint32 links[3];
  int32  secondary_header_value;
  int32  primary_header_value;
  int32  configuration_values[VEHICLE_DYNAMIC_CONFIGURATION_VALUE_COUNT];
  int32  curve_values[VEHICLE_DYNAMIC_CURVE_VALUE_COUNT];
  int32  scaled_token_values[VEHICLE_DYNAMIC_CONFIGURATION_VALUE_COUNT];
  int32  trailing_fixed_value;
  int32  trailing_integer_value;
} VehicleDynamicRenderObject;

typedef struct VehicleRoadWheelShapeRecord
{
  int32   vertex_indices[4];
  Vector3 vertices[4];
  Vector3 reference_position;
} VehicleRoadWheelShapeRecord;

typedef union VehicleRoadWheelShapeState
{
  int32                       values[76];
  VehicleRoadWheelShapeRecord wheels[4];
} VehicleRoadWheelShapeState;

typedef struct VehicleRoadAnimationState
{
  int32                   opponent_vehicle_override;
  uint8                   reserved_004[0x44];
  Vector3                 low_detail_projection_vertex;
  Matrix3x3               vehicle_model_view_matrix;
  uint8                   reserved_078[0xEC];
  Vector3                *quad_view_points;
  uint8                   reserved_168[4];
  Point2                 *quad_screen_points;
  uint32                 *polygon_depth_sort_scratch;
  Vector3                 tire_smoke_view_position;
  uint8                   reserved_180[0x18];
  Point2                  tire_smoke_screen_offset;
  uint8                   reserved_1_a0[0x10];
  uint8                   dynamic_data_enabled;
  uint8                   reserved_1_b1[0xA3];
  int32                   minimum_vehicle_projection_depth;
  int32                   special_shape_frame_counter;
  int32                   special_shape_frame_phase;
  int32                   special_shape_frame_countdown;
  int32                   special_shape_last_frame;
  int32                   high_detail_shape_countdown;
  int32                   high_detail_shape_phase;
  int32                   high_detail_shape_previous_x;
  int32                   high_detail_shape_previous_y;
  int32                   high_detail_shape_last_frame;
  int32                   reverse_animation_frame;
  VehicleSimulationState *reverse_vehicle;
  int32                   reverse_vehicle_index;
  int32                   reverse_active_index;
  int32                   forward_animation_frame;
  VehicleSimulationState *forward_vehicle;
  int32                   forward_vehicle_index;
} VehicleRoadAnimationState;

extern VehicleRoadAnimationState  g_VEHICLE_ROAD_ANIMATION_STATE;
extern VehicleRoadWheelShapeState g_VEHICLE_ROAD_WHEEL_SHAPE_STATE;
extern VehicleDynamicRenderObject
                              *g_VEHICLE_DYNAMIC_RENDER_OBJECTS[CAR_RENDER_RESOURCE_RECORD_COUNT];
extern VehicleSimulationState *g_VEHICLE_ANIMATION_STATES[20];
extern int32                   g_VEHICLE_ANIMATION_STATE_COUNT;

#define G_OPPONENT_VEHICLE_OVERRIDE (g_VEHICLE_ROAD_ANIMATION_STATE.opponent_vehicle_override)

void
load_vehicle_render_resources(int32 vehicle_index_, int32 opponent_index_);
void
load_vehicle_render_resource_slot(const char *identifier_, int32 slot_);
void
load_vehicle_dynamic_resource(void *destination_, const char *path_);
void
calculate_vehicle_resource_extents(RenderResourceRecord *record_, int32 coordinate_shift_);
void
initialize_vehicle_render_resource_record(void *wrapper_resource_, RenderResourceRecord *record_);
void
initialize_player_vehicle_wheel_shapes(RenderResourceRecord *record_);
void
setup_vehicle_render_shape_references(RenderResourceRecord *record_,
                                      void                 *shared_shape_collection_);
void
load_random_vehicle_art(int32 reset_slots_);
void
advance_frontend_animation_step(int32 step_);
void
advance_secondary_road_animation(int32 frame_index_);
int32
prepare_vehicle_road_projection(VehicleSimulationState *state_, int32 direction_);
int32
prepare_vehicle_road_animation(VehicleSimulationState *state_, int32 direction_);
int32
calculate_vehicle_road_reference_x_projection(const VehicleSimulationState *state_,
                                              int32                         reference_x_,
                                              int32                         reserved_,
                                              int32                         reference_z_);
int32
calculate_vehicle_road_render_heading_difference(const VehicleSimulationState *state_);
void
render_vehicle_road_effect_sprite(int32 screen_x_,
                                  int32 screen_y_,
                                  int32 horizontal_scale_,
                                  int32 vertical_scale_);
void
render_vehicle_road_collision_effects(VehicleSimulationState *state_, int32 layer_);
void
select_vehicle_render_palette(RenderResourceRecord *resource_, int32 palette_index_);
void
update_vehicle_road_render_wheels(VehicleSimulationState *state_);
void
project_high_detail_vehicle_vertices(int32            vertex_count_,
                                     const Matrix3x3 *matrix_,
                                     const Vector3   *translation_,
                                     const Vector3   *vertices_);
void
update_vehicle_road_polygon_depth_keys(int32                     polygon_count_,
                                       const PolygonDepthRecord *polygons_,
                                       const Vector3            *vertices_,
                                       uint32                   *keys_,
                                       uint32                   *scratch_);
void
update_vehicle_road_polygon_batch_depth_keys(int32                     polygon_count_,
                                             const PolygonDepthRecord *polygons_,
                                             const Vector3            *vertices_,
                                             uint32                   *keys_,
                                             uint32                   *scratch_);
int32
prepare_vehicle_road_render(VehicleSimulationState *state_, int32 direction_);
int32
render_vehicle_road_animation(VehicleSimulationState *state_, int32 direction_);
void
update_vehicle_road_render_brake_lights(VehicleSimulationState *state_);
void
update_special_vehicle_road_render_shapes(VehicleSimulationState *state_, int32 detail_);
void
update_active_vehicle_high_detail_shapes(VehicleSimulationState *state_);
void
render_distant_vehicle_road_sprite(VehicleSimulationState *state_,
                                   void                   *resource_,
                                   int32                   direction_);
