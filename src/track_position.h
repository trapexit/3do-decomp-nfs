#pragma once

#include "geometry.h"

#include "types.h"

typedef struct TrackSegment
{
  uint8   midpoint_negative_width;
  uint8   midpoint_positive_width;
  uint8   negative_boundary_width;
  uint8   positive_boundary_width;
  uint8   lane_boundary_offsets;
  uint8   boundary_behavior_codes;
  uint8   surface_index_codes;
  uint8   road_geometry_type;
  Vector3 surface_reference;
  int16   surface_incline;
  int16   surface_bank;
  int16   heading;
  uint16  status_flags;
  int16   surface_offset_x;
  int16   surface_offset_y;
  int16   surface_offset_z;
  uint8   reserved_22[2];
} TrackSegment;

typedef struct TrackOrientationState
{
  uint8 reserved_00[0x0C];
  int32 x_rotation;
  int32 y_rotation;
  int32 z_rotation;
  uint8 reserved_18[0x2C];
  int32 segment_index;
} TrackOrientationState;

extern TrackSegment *g_TRACK_SEGMENTS;

int32
is_track_orientation_within_bounds(const TrackOrientationState *orientation_);
int32
calculate_track_surface_height(int32 segment_index_, const Vector3 *position_);
uint32
adjust_track_segment_index(const Vector3 *position_, int32 *segment_index_);
