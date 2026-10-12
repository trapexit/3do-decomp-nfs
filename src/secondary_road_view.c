/* Prepares the secondary road viewport from the current track orientation. */

#include "cel_render.h"
#include "frontend_render_state.h"
#include "geometry.h"
#include "render_thread_signal.h"
#include "road_render.h"
#include "simulation_snapshot.h"
#include "window_fill.h"

#define SECONDARY_ROAD_VIEW_SPECIAL_FILL_VALUE 0x00000883UL
#define SECONDARY_ROAD_VIEW_SOLID_FILL_VALUE 0x00000C63UL
#define SECONDARY_ROAD_VIEW_SPECIAL_GEOMETRY 8
#define SECONDARY_ROAD_VIEW_GEOMETRY_A 4
#define SECONDARY_ROAD_VIEW_GEOMETRY_B 7
#define SECONDARY_ROAD_VIEW_GEOMETRY_C 9
#define SECONDARY_ROAD_VIEW_YAW_WRAP 0x80
#define SECONDARY_ROAD_VIEW_YAW_MAXIMUM 0x40
#define SECONDARY_ROAD_VIEW_YAW_MINIMUM -0x80
#define SECONDARY_ROAD_VIEW_CENTER_X 0x20
#define SECONDARY_ROAD_VIEW_CENTER_Y 0x0E
#define SECONDARY_ROAD_VIEW_ROTATION_SHIFT 15
#define SECONDARY_ROAD_VIEW_FIXED16_SHIFT 16
#define SECONDARY_ROAD_VIEW_FIXED16_ONE 0x00010000
#define SECONDARY_ROAD_VIEW_TRACK_OFFSET 3
#define SECONDARY_ROAD_VIEW_PITCH_LIMIT 0x100
#define SECONDARY_ROAD_VIEW_PITCH_WRAP 0x200
#define SECONDARY_ROAD_VIEW_PITCH_MULTIPLIER 3
#define SECONDARY_ROAD_VIEW_PITCH_DIVISOR 4
#define SECONDARY_ROAD_VIEW_RESERVED_BYTES 0x140

uint32 g_SECONDARY_ROAD_VIEW_FILL_VALUE = SECONDARY_ROAD_VIEW_SPECIAL_FILL_VALUE;
uint8  g_SECONDARY_ROAD_VIEW_RESERVED_STATE[SECONDARY_ROAD_VIEW_RESERVED_BYTES];

static int32
_extract_secondary_view_rotation(int32 rotation_)
{
  return (int32) (int16) ((uint32) rotation_ >> SECONDARY_ROAD_VIEW_ROTATION_SHIFT);
}

void
prepare_secondary_road_view(void)
{
  const SimulationProjectionSource *source;
  int32                             pitch;
  uint8                             geometry_type;
  int32                             yaw;

  source = g_FRONTEND_RENDER_STATE.projection.source;
  geometry_type =
    g_REFERENCE_DISTANCE_RECORDS[source->track_section_position - SECONDARY_ROAD_VIEW_TRACK_OFFSET]
      .road_geometry_type;
  if((geometry_type == SECONDARY_ROAD_VIEW_GEOMETRY_A) ||
     (geometry_type == SECONDARY_ROAD_VIEW_GEOMETRY_B) ||
     (geometry_type == SECONDARY_ROAD_VIEW_GEOMETRY_C))
    {
      fill_display_window(SECONDARY_ROAD_VIEW_SOLID_FILL_VALUE);
      return;
    }
  if(geometry_type == SECONDARY_ROAD_VIEW_SPECIAL_GEOMETRY)
    {
      fill_display_window(g_SECONDARY_ROAD_VIEW_FILL_VALUE);
      return;
    }

  yaw   = -_extract_secondary_view_rotation(source->projection_transform.y_rotation);
  pitch = _extract_secondary_view_rotation(source->projection_transform.x_rotation);
  if(pitch > SECONDARY_ROAD_VIEW_PITCH_LIMIT)
    pitch -= SECONDARY_ROAD_VIEW_PITCH_WRAP;
  pitch = (pitch * SECONDARY_ROAD_VIEW_PITCH_MULTIPLIER) / SECONDARY_ROAD_VIEW_PITCH_DIVISOR;
  pitch -= g_RENDER_THREAD_STATE.track_render_profile_value;
  while(yaw > SECONDARY_ROAD_VIEW_YAW_MAXIMUM)
    yaw -= SECONDARY_ROAD_VIEW_YAW_WRAP;
  while(yaw > SECONDARY_ROAD_VIEW_YAW_MINIMUM)
    {
      draw_transformed_ccb(
        (CCB *) g_RENDER_PIPELINE_CCB_FLAGS,
        SECONDARY_ROAD_VIEW_CENTER_X << SECONDARY_ROAD_VIEW_FIXED16_SHIFT,
        SECONDARY_ROAD_VIEW_CENTER_Y << SECONDARY_ROAD_VIEW_FIXED16_SHIFT,
        (SECONDARY_ROAD_VIEW_CENTER_X - yaw) << SECONDARY_ROAD_VIEW_FIXED16_SHIFT,
        (SECONDARY_ROAD_VIEW_CENTER_Y - pitch) << SECONDARY_ROAD_VIEW_FIXED16_SHIFT,
        -source->projection_transform.z_rotation,
        SECONDARY_ROAD_VIEW_FIXED16_ONE,
        SECONDARY_ROAD_VIEW_FIXED16_ONE);
      yaw -= SECONDARY_ROAD_VIEW_YAW_WRAP;
    }
}
