/* Renders road geometry through the secondary simulation projection. */

#include "frontend_render_state.h"
#include "road_render.h"
#include "simulation_snapshot.h"

void
render_secondary_road_projection(void)
{
  configure_simulation_projection(g_FRONTEND_RENDER_STATE.projection.source, 1);
  render_road_geometry_points(g_ROAD_GEOMETRY_POINT_COUNT);
}
