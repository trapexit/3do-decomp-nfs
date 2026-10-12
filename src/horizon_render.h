#pragma once

#include "horizon_resources.h"
#include "simulation_snapshot.h"

#include "graphics.h"
#include "types.h"

#define HORIZON_RING_VERTEX_COUNT 96
#define HORIZON_RING_CEL_COUNT 48

struct HorizonRing
{
  int32      enabled;
  Vector3    origin;
  int32      radius_fixed;
  uint8      reserved_014[8];
  int32      active_cel_index;
  uint32     point_count;
  Vector3    vertices[HORIZON_RING_VERTEX_COUNT];
  CCB        cels[HORIZON_RING_CEL_COUNT];
  const CCB *source_cels[6];
  void      *resource_packet;
};

typedef struct HorizonRenderState
{
  int32  first_ring_origin_y;
  int32  second_ring_origin_y;
  int32  first_ring_origin_z;
  int32  second_ring_origin_z;
  int32  first_ring_radius_fixed;
  int32  first_ring_vertical_offset;
  int32  second_ring_radius_fixed;
  int32  second_ring_vertical_offset;
  uint32 primary_fill_value;
  uint32 secondary_fill_value;
} HorizonRenderState;

extern HorizonRenderState g_HORIZON_RENDER_STATE;
extern int32              g_HORIZON_PROJECTION_SHIFT;

void
update_horizon_render_projection(const SimulationRenderState       *state_,
                                 const SimulationViewConfiguration *view_configuration_);
HorizonRing *
create_horizon_ring(int32          vertical_offset_,
                    int32          radius_fixed_,
                    const char    *resource_name_,
                    const Vector3 *origin_,
                    int32          resource_packet_index_);
void
initialize_horizon_configuration(const char *track_name_);
void
render_horizon(Item display_resource_item_, Item bitmap_item_);
void
update_horizon_ring_projection(const Vector3 *camera_translation_);
void
update_horizon_ring_cels(HorizonRing *ring_, const Vector3 *camera_translation_);
void
fill_horizon_bitmap(Item   display_resource_item_,
                    uint32 primary_fill_value_,
                    uint32 secondary_fill_value_);
