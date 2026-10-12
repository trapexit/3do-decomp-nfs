#include "track_relocation.h"

#include "road_data.h"
#include "road_processing.h"
#include "simulation_snapshot.h"
#include "track_position.h"

#define TRACK_POSITION_UNITS_PER_SECTION 4
#define TRACK_RELOCATION_VERTICAL_OFFSET 0x20000
#define TRACK_ROTATION_FIXED_SHIFT 10
#define TRACK_RELOCATION_SIMULATION_MODE 8
#define NO_PENDING_TRACK_POSITION -1
#define READ_SIGNED_BIG_ENDIAN_INT16(value_)                                                       \
  ((int32) (int16) ((((uint32) ((const uint8 *) (value_))[0]) << 8) |                              \
                    (uint32) ((const uint8 *) (value_))[1]))

int32 g_PENDING_SIMULATION_TRACK_POSITION = NO_PENDING_TRACK_POSITION;

void
apply_pending_simulation_track_position(void)
{
  int32 track_position;

  track_position = g_PENDING_SIMULATION_TRACK_POSITION;
  if(track_position < 0)
    return;

  relocate_simulation_track_position(track_position);
  g_PENDING_SIMULATION_TRACK_POSITION = NO_PENDING_TRACK_POSITION;
}

void
set_pending_simulation_track_position(int32 track_position_)
{
  g_PENDING_SIMULATION_TRACK_POSITION = track_position_;
}

void
set_pending_simulation_track_section(int32 section_index_)
{
  set_pending_simulation_track_position(section_index_ * TRACK_POSITION_UNITS_PER_SECTION);
}

void
relocate_simulation_track_position(int32 track_position_)
{
  SimulationRenderState *state;
  const TrackSegment    *segment;
  const Vector3         *position_source;

  state                      = &g_SIMULATION_RENDER_STATE;
  state->position_flags      = 1;
  state->track_segment_index = track_position_;
  synchronize_road_section_stream(track_position_ / TRACK_POSITION_UNITS_PER_SECTION);

  segment                  = &g_TRACK_SEGMENTS[track_position_];
  position_source          = state->position_source;
  state->position_offset.x = (segment->surface_reference.x - position_source->x);
  state->position_offset.y =
    ((segment->surface_reference.y - position_source->y) + TRACK_RELOCATION_VERTICAL_OFFSET);
  state->position_offset.z = (segment->surface_reference.z - position_source->z);
  state->vector_offset.x =
    -(READ_SIGNED_BIG_ENDIAN_INT16(&segment->surface_incline) << TRACK_ROTATION_FIXED_SHIFT);
  state->vector_offset.z =
    (READ_SIGNED_BIG_ENDIAN_INT16(&segment->surface_bank) << TRACK_ROTATION_FIXED_SHIFT);
  state->vector_offset.y =
    (READ_SIGNED_BIG_ENDIAN_INT16(&segment->heading) << TRACK_ROTATION_FIXED_SHIFT);
  state->vector_offset.y =
    (READ_SIGNED_BIG_ENDIAN_INT16(&(segment + 1)->heading) << TRACK_ROTATION_FIXED_SHIFT);

  g_SIMULATION_MODE_CONFIGURATIONS[TRACK_RELOCATION_SIMULATION_MODE].transition_vector =
    state->position_offset;
  update_simulation_state((SimulationRenderState *) g_FRONTEND_RENDER_STATE.projection.source);
  capture_simulation_render_state(
    (SimulationRenderState *) g_FRONTEND_RENDER_STATE.projection.source);
  update_road_geometry(g_FRONTEND_RENDER_STATE.projection.source);
}
