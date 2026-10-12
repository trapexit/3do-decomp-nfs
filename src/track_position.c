#include "track_position.h"

#include "operamath.h"

#define TRACK_COORDINATE_WRAP_LIMIT 0x00800000
#define TRACK_COORDINATE_WRAP_ADJUST 0x01000000
#define TRACK_Y_ROTATION_BOUNDARY 0x00140000
#define TRACK_XZ_ROTATION_BOUNDARY 0x001C0000
#define TRACK_SEGMENT_REFERENCE_SHIFT 10
#define TRACK_MIDPOINT_SHIFT 1
#define TRACK_REFERENCE_HEADING_HIGH_INDEX 4
#define TRACK_REFERENCE_HEADING_LOW_INDEX 5
#define TRACK_REFERENCE_HEADING_BYTE_SHIFT 8
#define READ_SIGNED_BIG_ENDIAN_INT16(value_)                                                       \
  ((int32) (int16) ((((uint32) ((const uint8 *) (value_))[0]) << 8) |                              \
                    (uint32) ((const uint8 *) (value_))[1]))

TrackSegment *g_TRACK_SEGMENTS;

int32
is_track_orientation_within_bounds(const TrackOrientationState *orientation_)
{
  int32 y_rotation_difference;
  int32 transverse_rotation;

  y_rotation_difference =
    orientation_->y_rotation -
    ((int32) (int16) (((uint32) g_REFERENCE_DISTANCE_RECORDS[orientation_->segment_index]
                                    .packed_track_direction[TRACK_REFERENCE_HEADING_HIGH_INDEX]
                        << TRACK_REFERENCE_HEADING_BYTE_SHIFT) |
                       (uint32) g_REFERENCE_DISTANCE_RECORDS[orientation_->segment_index]
                         .packed_track_direction[TRACK_REFERENCE_HEADING_LOW_INDEX])
     << TRACK_SEGMENT_REFERENCE_SHIFT);
  if(y_rotation_difference > TRACK_COORDINATE_WRAP_LIMIT)
    y_rotation_difference -= TRACK_COORDINATE_WRAP_ADJUST;
  else if(y_rotation_difference < -TRACK_COORDINATE_WRAP_LIMIT)
    y_rotation_difference += TRACK_COORDINATE_WRAP_ADJUST;

  if((y_rotation_difference >= TRACK_Y_ROTATION_BOUNDARY) ||
     (y_rotation_difference <= -TRACK_Y_ROTATION_BOUNDARY))
    return 0;

  transverse_rotation = orientation_->z_rotation;
  if(transverse_rotation <= 0)
    transverse_rotation = -transverse_rotation;
  if(transverse_rotation > TRACK_XZ_ROTATION_BOUNDARY)
    return 0;

  transverse_rotation = orientation_->x_rotation;
  if(transverse_rotation <= 0)
    transverse_rotation = -transverse_rotation;
  return transverse_rotation <= TRACK_XZ_ROTATION_BOUNDARY;
}

int32
calculate_track_surface_height(int32 segment_index_, const Vector3 *position_)
{
  TrackSegment  *segment;
  Vector3        offset_edge;
  Vector3        next_edge;
  Vector3        offset_vertex;
  Vector3        surface_normal;
  const Vector3 *next_reference;
  int32          height_offset;

  segment        = &g_TRACK_SEGMENTS[segment_index_];
  next_reference = &(segment + 1)->surface_reference;
  offset_vertex.x =
    segment->surface_reference.x + (READ_SIGNED_BIG_ENDIAN_INT16(&segment->surface_offset_x) << 1);
  offset_vertex.y =
    segment->surface_reference.y + (READ_SIGNED_BIG_ENDIAN_INT16(&segment->surface_offset_y) << 1);
  offset_vertex.z =
    segment->surface_reference.z + (READ_SIGNED_BIG_ENDIAN_INT16(&segment->surface_offset_z) << 1);

  next_edge.x   = next_reference->x - segment->surface_reference.x;
  next_edge.y   = next_reference->y - segment->surface_reference.y;
  next_edge.z   = next_reference->z - segment->surface_reference.z;
  offset_edge.x = offset_vertex.x - segment->surface_reference.x;
  offset_edge.y = offset_vertex.y - segment->surface_reference.y;
  offset_edge.z = offset_vertex.z - segment->surface_reference.z;
  Cross3_F16((frac16 *) &surface_normal, (frac16 *) &next_edge, (frac16 *) &offset_edge);

  height_offset = -MulSF16(surface_normal.x, position_->x - next_reference->x);
  height_offset -= MulSF16(surface_normal.z, position_->z - next_reference->z);
  return next_reference->y + DivSF16(height_offset, surface_normal.y);
}

/* Keep the adjacent midpoint comparisons together because they form one stabilization loop. */
uint32
adjust_track_segment_index(const Vector3 *position_, int32 *segment_index_)
{
  const TrackSegment *previous_segment;
  const TrackSegment *segment;
  const TrackSegment *next_segment;
  Vector3             midpoint;
  uint32              current_distance;
  uint32              adjacent_distance;
  uint32              adjusted;
  int32               previous_index;

  adjusted = 0;
  if(*segment_index_ == -1)
    return adjusted;

  midpoint.y = 0;
  do
    {
      previous_index = *segment_index_;
      segment        = &g_TRACK_SEGMENTS[previous_index];
      next_segment   = segment + 1;
      midpoint.x     = ((segment->surface_reference.x + next_segment->surface_reference.x) >>
                        TRACK_MIDPOINT_SHIFT);
      midpoint.z     = ((segment->surface_reference.z + next_segment->surface_reference.z) >>
                        TRACK_MIDPOINT_SHIFT);
      current_distance =
        (uint32) calculate_vector3_planar_squared_distance_fixed8(&midpoint, position_);

      midpoint.x = ((next_segment->surface_reference.x + (next_segment + 1)->surface_reference.x) >>
                    TRACK_MIDPOINT_SHIFT);
      midpoint.z = ((next_segment->surface_reference.z + (next_segment + 1)->surface_reference.z) >>
                    TRACK_MIDPOINT_SHIFT);
      adjacent_distance =
        (uint32) calculate_vector3_planar_squared_distance_fixed8(&midpoint, position_);
      if(adjacent_distance < current_distance)
        {
          *segment_index_ += 1;
          adjusted = 1;
        }
      else if(*segment_index_ > 0)
        {
          previous_segment = segment - 1;
          midpoint.x = ((previous_segment->surface_reference.x + segment->surface_reference.x) >>
                        TRACK_MIDPOINT_SHIFT);
          midpoint.z = ((previous_segment->surface_reference.z + segment->surface_reference.z) >>
                        TRACK_MIDPOINT_SHIFT);
          adjacent_distance =
            (uint32) calculate_vector3_planar_squared_distance_fixed8(&midpoint, position_);
          if(adjacent_distance < current_distance)
            {
              *segment_index_ -= 1;
              adjusted = 1;
            }
        }
    }
  while(previous_index != *segment_index_);

  return adjusted;
}
