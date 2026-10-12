#include "road_object_table.h"

#include "game_frame_count.h"
#include "integer_division.h"
#include "road_processing.h"
#include "signal_status.h"
#include "simulation_resource_cleanup.h"
#include "simulation_snapshot.h"
#include "sound_spatial.h"
#include "track_position.h"
#include "track_processing_cache.h"

#include "operamath.h"

#include "stddef.h"

#define ROAD_OBJECT_RENDER_RECORD_LIMIT 80
#define ROAD_OBJECT_ACTIVE_POSITION_OFFSET 0x0C
#define ROAD_OBJECT_ACTIVE_HEADING_OFFSET 0x1C
#define ROAD_OBJECT_ACTIVE_SEGMENT_OFFSET 0x50
#define ROAD_OBJECT_ACTIVE_RESOURCE_OFFSET 0x4D4
#define ROAD_OBJECT_BOUND_DIVISOR 2
#define ROAD_OBJECT_SOUND_SEGMENT_MINIMUM_EXCLUSIVE -9
#define ROAD_OBJECT_SOUND_SEGMENT_MAXIMUM_EXCLUSIVE 10
#define ROAD_OBJECT_SOUND_ID -1
#define ROAD_OBJECT_SOUND_CLASS 2
#define ROAD_OBJECT_SOUND_VARIANT 9
#define ROAD_OBJECT_SOUND_GAIN 0x10000
#define ROAD_OBJECT_POSITION_SCALE 256
#define ROAD_OBJECT_ACTIVATION_FRAME_NUMERATOR 1200
#define ROAD_OBJECT_ACTIVATION_FRAME_BIAS 1
#define ROAD_OBJECT_OFFSET_X_BYTE 0x0A
#define ROAD_OBJECT_OFFSET_Y_BYTE 0x0C
#define ROAD_OBJECT_OFFSET_Z_BYTE 0x0E
#define ROAD_OBJECT_OFFSET_LOW_BYTE 1
#define ROAD_OBJECT_BIG_ENDIAN_BYTE_SHIFT 8
#define ROAD_OBJECT_SCAN_BACKWARD -1
#define ROAD_OBJECT_SCAN_FORWARD 1

typedef struct RoadObjectActiveVehicleState
{
  uint8                 reserved_000[ROAD_OBJECT_ACTIVE_POSITION_OFFSET];
  Vector3               position;
  uint8                 reserved_018[ROAD_OBJECT_ACTIVE_HEADING_OFFSET - 0x18];
  int32                 heading;
  uint8                 reserved_020[ROAD_OBJECT_ACTIVE_SEGMENT_OFFSET - 0x20];
  int32                 track_segment_index;
  uint8                 reserved_054[ROAD_OBJECT_ACTIVE_RESOURCE_OFFSET - 0x54];
  RenderResourceRecord *render_resource;
} RoadObjectActiveVehicleState;

typedef char RoadObjectActivePositionOffset[(offsetof(RoadObjectActiveVehicleState, position) ==
                                             ROAD_OBJECT_ACTIVE_POSITION_OFFSET) ?
                                              1 :
                                              -1];
typedef char RoadObjectActiveHeadingOffset
  [(offsetof(RoadObjectActiveVehicleState, heading) == ROAD_OBJECT_ACTIVE_HEADING_OFFSET) ? 1 : -1];
typedef char
  RoadObjectActiveSegmentOffset[(offsetof(RoadObjectActiveVehicleState, track_segment_index) ==
                                 ROAD_OBJECT_ACTIVE_SEGMENT_OFFSET) ?
                                  1 :
                                  -1];
typedef char
  RoadObjectActiveResourceOffset[(offsetof(RoadObjectActiveVehicleState, render_resource) ==
                                  ROAD_OBJECT_ACTIVE_RESOURCE_OFFSET) ?
                                   1 :
                                   -1];

void
count_road_object_records(void)
{
  int32 record_index;

  record_index = 0;
  while((G_ROAD_OBJECT_RECORDS[record_index].record_id != -1) &&
        (record_index < ROAD_OBJECT_SCAN_LIMIT))
    {
      record_index++;
    }

  g_ROAD_OBJECT_RECORD_COUNT = record_index;
}

void
initialize_road_object_processing(int32 section_index_)
{
  int32 first_record_index;
  int32 last_record_index;
  int32 lower_record_index;
  int32 middle_record_id;
  int32 middle_record_index;
  int32 target_record_id;

  g_ROAD_OBJECT_PROCESSING_STATE.first_record_index = 0;
  if(G_ROAD_OBJECT_RECORDS[0].record_id >= 0)
    {
      target_record_id    = (section_index_ * ROAD_OBJECT_RECORDS_PER_SECTION);
      last_record_index   = (g_ROAD_OBJECT_RECORD_COUNT - 1);
      lower_record_index  = 0;
      middle_record_index = (last_record_index / ROAD_OBJECT_SEARCH_DIVISOR);
      while((lower_record_index + 1) < last_record_index)
        {
          middle_record_index =
            ((last_record_index + lower_record_index) / ROAD_OBJECT_SEARCH_DIVISOR);
          if(G_ROAD_OBJECT_RECORDS[middle_record_index].record_id < target_record_id)
            lower_record_index = middle_record_index;
          else
            last_record_index = middle_record_index;
        }

      middle_record_id = G_ROAD_OBJECT_RECORDS[middle_record_index].record_id;
      while(middle_record_index > 0)
        {
          middle_record_index--;
          if(G_ROAD_OBJECT_RECORDS[middle_record_index].record_id != middle_record_id)
            break;
        }

      while((G_ROAD_OBJECT_RECORDS[middle_record_index].record_id < target_record_id) &&
            (middle_record_index < (g_ROAD_OBJECT_RECORD_COUNT - 1)))
        middle_record_index++;
      g_ROAD_OBJECT_PROCESSING_STATE.first_record_index = middle_record_index;
    }

  first_record_index = g_ROAD_OBJECT_PROCESSING_STATE.first_record_index;
  g_ROAD_OBJECT_PROCESSING_STATE.current_record_index   = first_record_index;
  g_ROAD_OBJECT_PROCESSING_STATE.mesh_vertex_count      = 0;
  g_ROAD_OBJECT_PROCESSING_STATE.processed_record_count = 0;
  g_ROAD_OBJECT_PROCESSING_STATE.projected_vertex_count = 0;
  g_ROAD_OBJECT_PROCESSING_STATE.current_record_id =
    G_ROAD_OBJECT_RECORDS[first_record_index].record_id;
}

void
prepare_road_object_render_range(void)
{
  int32 processed_record_count;

  processed_record_count = g_ROAD_OBJECT_PROCESSING_STATE.processed_record_count;
  if(processed_record_count > ROAD_OBJECT_RENDER_RECORD_LIMIT)
    {
      g_ROAD_OBJECT_PROCESSING_STATE.processed_record_count = 0;
      g_ROAD_OBJECT_PROCESSING_STATE.current_record_id      = -1;
      return;
    }

  if(processed_record_count <= 0)
    {
      g_ROAD_OBJECT_PROCESSING_STATE.current_record_id = -1;
      return;
    }

  g_ROAD_OBJECT_PROCESSING_STATE.current_record_index =
    (g_ROAD_OBJECT_PROCESSING_STATE.first_record_index + processed_record_count - 1);
  g_ROAD_OBJECT_PROCESSING_STATE.render_vertex_cursor =
    g_ROAD_OBJECT_PROCESSING_STATE.projected_vertex_count;
  g_ROAD_OBJECT_PROCESSING_STATE.current_record_id =
    G_ROAD_OBJECT_RECORDS[g_ROAD_OBJECT_PROCESSING_STATE.current_record_index].record_id;
}

int32
activate_nearby_road_object(void)
{
  const RoadObjectActiveVehicleState *active;
  const RoadObjectRecord             *record;
  RoadObjectOrientationState         *orientation_state;
  const uint8                        *record_bytes;
  Vector3                             position;
  int32                               direction;
  int32                               offset_x;
  int32                               offset_y;
  int32                               offset_z;
  int32                               segment_index;

  active        = (const RoadObjectActiveVehicleState *) G_ACTIVE_SIMULATION_SOURCE;
  segment_index = active->track_segment_index;
  while((g_RUNTIME_STATE.road_object_activation_record_id == segment_index) &&
        (g_RUNTIME_STATE.road_object_activation_record_index != 0))
    {
      g_RUNTIME_STATE.road_object_activation_record_index -= 1;
      g_RUNTIME_STATE.road_object_activation_record_id =
        G_ROAD_OBJECT_RECORDS[g_RUNTIME_STATE.road_object_activation_record_index].record_id;
    }
  record = &G_ROAD_OBJECT_RECORDS[g_RUNTIME_STATE.road_object_activation_record_index];

  if(segment_index < g_RUNTIME_STATE.road_object_activation_record_id)
    direction = ROAD_OBJECT_SCAN_BACKWARD;
  else if(record[ROAD_OBJECT_SCAN_FORWARD].record_id != -1)
    direction = ROAD_OBJECT_SCAN_FORWARD;
  else
    return 0;

  for(;;)
    {
      orientation_state =
        &g_ROAD_OBJECT_ORIENTATION_STATES[g_RUNTIME_STATE.road_object_activation_record_index];
      if((direction == ROAD_OBJECT_SCAN_BACKWARD) &&
         (g_RUNTIME_STATE.road_object_activation_record_index == 0))
        return 0;

      if((g_RUNTIME_STATE.road_object_activation_record_id == segment_index) &&
         (orientation_state->current == 0))
        {
          record_bytes = (const uint8 *) record;
          offset_x =
            (int32) (int16) (((uint16) record_bytes[ROAD_OBJECT_OFFSET_X_BYTE]
                              << ROAD_OBJECT_BIG_ENDIAN_BYTE_SHIFT) |
                             record_bytes[ROAD_OBJECT_OFFSET_X_BYTE + ROAD_OBJECT_OFFSET_LOW_BYTE]);
          offset_y =
            (int32) (int16) (((uint16) record_bytes[ROAD_OBJECT_OFFSET_Y_BYTE]
                              << ROAD_OBJECT_BIG_ENDIAN_BYTE_SHIFT) |
                             record_bytes[ROAD_OBJECT_OFFSET_Y_BYTE + ROAD_OBJECT_OFFSET_LOW_BYTE]);
          offset_z =
            (int32) (int16) (((uint16) record_bytes[ROAD_OBJECT_OFFSET_Z_BYTE]
                              << ROAD_OBJECT_BIG_ENDIAN_BYTE_SHIFT) |
                             record_bytes[ROAD_OBJECT_OFFSET_Z_BYTE + ROAD_OBJECT_OFFSET_LOW_BYTE]);
          position.x = g_TRACK_SEGMENTS[record->record_id].surface_reference.x +
                       (offset_x * ROAD_OBJECT_POSITION_SCALE);
          position.y = g_TRACK_SEGMENTS[record->record_id].surface_reference.y +
                       (offset_y * ROAD_OBJECT_POSITION_SCALE);
          position.z = g_TRACK_SEGMENTS[record->record_id].surface_reference.z +
                       (offset_z * ROAD_OBJECT_POSITION_SCALE);
          if(is_road_object_near_active_vehicle(&position) != 0)
            {
              orientation_state->current =
                (uint8) (divide_signed_integer(ROAD_OBJECT_ACTIVATION_FRAME_NUMERATOR,
                                               G_GAME_FRAME_COUNTER) +
                         ROAD_OBJECT_ACTIVATION_FRAME_BIAS);
              invalidate_track_processing_cache();
              return 1;
            }
        }

      g_RUNTIME_STATE.road_object_activation_record_index += direction;
      record = &G_ROAD_OBJECT_RECORDS[g_RUNTIME_STATE.road_object_activation_record_index];
      g_RUNTIME_STATE.road_object_activation_record_id = record->record_id;
      if(direction == ROAD_OBJECT_SCAN_BACKWARD)
        {
          if((g_RUNTIME_STATE.road_object_activation_record_id < segment_index) ||
             (g_RUNTIME_STATE.road_object_activation_record_index == 0))
            return 0;
        }
      else if((g_RUNTIME_STATE.road_object_activation_record_id > segment_index) ||
              (record[ROAD_OBJECT_SCAN_FORWARD].record_id == -1))
        return 0;
    }
}

int32
is_road_object_near_active_vehicle(const Vector3 *position_)
{
  const RoadObjectActiveVehicleState *active;
  const RenderResourceRecord         *resource;
  int32                               cosine;
  int32                               delta_x;
  int32                               delta_z;
  int32                               distance;
  int32                               forward_distance;
  int32                               half_extent;
  int32                               lateral_distance;
  int32                               pan;
  int32                               segment_difference;
  int32                               sine;

  active  = (const RoadObjectActiveVehicleState *) G_ACTIVE_SIMULATION_SOURCE;
  delta_x = position_->x - active->position.x;
  delta_z = position_->z - active->position.z;
  cosine  = CosF16(active->heading);
  sine    = SinF16(active->heading);

  forward_distance = MulSF16(cosine, delta_x) - MulSF16(sine, delta_z);
  resource         = active->render_resource;
  half_extent      = resource->width_extent / ROAD_OBJECT_BOUND_DIVISOR;
  if((forward_distance > half_extent) || (forward_distance < -half_extent))
    return 0;

  lateral_distance = MulSF16(sine, delta_x) + MulSF16(cosine, delta_z);
  half_extent      = resource->depth_extent / ROAD_OBJECT_BOUND_DIVISOR;
  if((lateral_distance > half_extent) || (lateral_distance < -half_extent))
    return 0;

  segment_difference =
    active->track_segment_index - (int32) g_SIMULATION_RENDER_STATE.track_segment_index;
  if((segment_difference < ROAD_OBJECT_SOUND_SEGMENT_MAXIMUM_EXCLUSIVE) &&
     (segment_difference > ROAD_OBJECT_SOUND_SEGMENT_MINIMUM_EXCLUSIVE))
    {
      calculate_spatial_sound_parameters(&active->position, &pan, &distance);
      play_spatial_sound_effect(ROAD_OBJECT_SOUND_ID,
                                ROAD_OBJECT_SOUND_CLASS,
                                ROAD_OBJECT_SOUND_VARIANT,
                                ROAD_OBJECT_SOUND_GAIN,
                                distance,
                                pan);
    }
  return 1;
}

int32
seek_road_object_record(int32 record_id_)
{
  if(g_RUNTIME_STATE.road_object_seek_record_id <= record_id_)
    {
      if(G_ROAD_OBJECT_RECORDS[g_RUNTIME_STATE.road_object_seek_record_index + 1].record_id == -1)
        {
          return 0;
        }

      do
        {
          if(g_RUNTIME_STATE.road_object_seek_record_id == record_id_)
            {
              return 1;
            }

          g_RUNTIME_STATE.road_object_seek_record_index++;
          g_RUNTIME_STATE.road_object_seek_record_id =
            G_ROAD_OBJECT_RECORDS[g_RUNTIME_STATE.road_object_seek_record_index].record_id;
          if(g_RUNTIME_STATE.road_object_seek_record_id > record_id_)
            {
              return 0;
            }
        }
      while(G_ROAD_OBJECT_RECORDS[g_RUNTIME_STATE.road_object_seek_record_index + 1].record_id !=
            -1);
    }
  else
    {
      if(g_RUNTIME_STATE.road_object_seek_record_index == 0)
        {
          return 0;
        }

      do
        {
          if(g_RUNTIME_STATE.road_object_seek_record_id == record_id_)
            {
              return 1;
            }

          g_RUNTIME_STATE.road_object_seek_record_index--;
          g_RUNTIME_STATE.road_object_seek_record_id =
            G_ROAD_OBJECT_RECORDS[g_RUNTIME_STATE.road_object_seek_record_index].record_id;
          if(g_RUNTIME_STATE.road_object_seek_record_id < record_id_)
            {
              return 0;
            }
        }
      while(g_RUNTIME_STATE.road_object_seek_record_index != 0);
    }

  return 0;
}
