#pragma once

#include "doubly_linked_list.h"

#include "types.h"

struct RenderResourceRecord;
struct SimulationSource;
struct VehicleRuntimeFrameRecord;
struct VehicleDynamicRenderObject;
struct VehicleParameters;

typedef struct VehicleRuntimeRenderMetadata
{
  uint8 reserved_00[0x70];
  int32 collision_radius;
  int32 half_render_height;
  int32 half_render_depth;
  int32 half_render_width;
} VehicleRuntimeRenderMetadata;

#define VEHICLE_RUNTIME_OBJECT_FLAGS_OFFSET 0x188
#define VEHICLE_RUNTIME_OBJECT_SLOT_OFFSET 0x4C8

typedef struct VehicleRuntimeObjectState
{
  uint8                         reserved_000[0x78];
  int32                         render_width;
  int32                         render_height;
  uint8                         reserved_080[0x40];
  VehicleRuntimeRenderMetadata *render_metadata;
  uint8                         reserved_0_c4[0x94];
  int32                         collision_motion_projection;
  uint8                         reserved_15_c[VEHICLE_RUNTIME_OBJECT_FLAGS_OFFSET - 0x15C];
  uint32                        status_flags;
  uint8  reserved_18_c[0x450 - VEHICLE_RUNTIME_OBJECT_FLAGS_OFFSET - sizeof(uint32)];
  int32  spatial_sound_gain;
  uint32 spatial_sound_configuration;
  uint8  reserved_458[0x4B4 - 0x458];
  void  *vehicle_spec_auxiliary_a;
  void  *vehicle_spec_auxiliary_b;
  struct VehicleParameters *vehicle_parameters;
  void                     *state_update_callback;
  void                     *render_update_callback;
  int32                     slot_index;
  uint32                    runtime_flags;
  int32                     configuration_mode;
  void                     *render_resource_state;
  int32                     render_detail_level;
  void                     *render_object;
  uint8                   **render_shape_pointers;
} VehicleRuntimeObjectState;

typedef struct VehicleRuntimePoolState
{
  const char                *vehicle_resource_identifiers[8];
  int32                      snapshot_preserved_state;
  uint8                      reserved_024[4];
  int32                      render_update_pending;
  int32                      render_update_ready;
  DoublyLinkedListStorage   *active_record_list;
  uint8                      reserved_034[4];
  DoublyLinkedListStorage   *vehicle_record_pool;
  VehicleRuntimeObjectState *player_object;
  VehicleRuntimeObjectState *opponent_object;
  uint8                      reserved_044[0x6C];
  int32                      render_object_count;
  uint8                      reserved_0_b4[0x28];
  int32                      audio_channels_stopped;
  int32                      sound_update_complete;
  int32                      road_render_state;
  uint8                      reserved_0_e8[4];
  uint8                      vehicle_specs_text_mode;
  uint8                      reserved_0_ed[7];
  void                      *vehicle_specs_family;
  uint8                      reserved_0_f8[0x0C];
} VehicleRuntimePoolState;

typedef VehicleRuntimePoolState VehicleRuntimeControllerState;

extern VehicleRuntimePoolState g_VEHICLE_RUNTIME_POOL_STATE;
extern void                   *g_VEHICLE_RUNTIME_AUXILIARY_ALLOCATION;
extern int32                   g_VEHICLE_RUNTIME_SOUND_UPDATE_PENDING;
extern int32                   g_VEHICLE_RUNTIME_FRAME_DELAY_COUNTER;
extern int32                   g_VEHICLE_COLLISION_SOUND_PENDING;
extern int32                   g_VEHICLE_COLLISION_SOUND_ACTIVE;
extern uint8                   g_VEHICLE_TRACK_FLICKER_DATA[194];

void
initialize_vehicle_runtime_pool(void);
void *
load_vehicle_track_flicker_data(void);
void
reset_vehicle_runtime_objects(void);
void
release_vehicle_runtime_objects(void);
void
insert_vehicle_runtime_object(VehicleRuntimeObjectState *object_);
void
deactivate_vehicle_runtime_object(VehicleRuntimeObjectState *object_);
void
queue_vehicle_runtime_object_release(VehicleRuntimeObjectState *object_);
void
configure_vehicle_runtime_object(VehicleRuntimeObjectState *object_,
                                 const char                *resource_identifier_,
                                 int32                      configuration_mode_);
int32
assign_vehicle_runtime_object_art(VehicleRuntimeObjectState *object_, int32 slot_index_);
void
initialize_vehicle_runtime_art_state(const struct VehicleDynamicRenderObject *resource_,
                                     struct SimulationSource                 *source_);
void
initialize_vehicle_runtime_objects(void);
void
prepare_vehicle_runtime_render(void);
void
update_vehicle_runtime_recovery_steering(void);
int32
calculate_vehicle_audio_motion_scale(struct VehicleRuntimeFrameRecord *record_);
void
update_vehicle_runtime_objects(void);
void
shutdown_vehicle_runtime_pool(void);
