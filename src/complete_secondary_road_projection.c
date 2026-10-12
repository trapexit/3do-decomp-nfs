/* Completes the secondary road projection and returns to the primary view. */

#include "bitmap_clip.h"
#include "ccb_manager.h"
#include "display_buffer_state.h"
#include "frontend_render_state.h"
#include "render_thread_signal.h"
#include "road_render.h"
#include "simulation_snapshot.h"
#include "window_memory.h"

int32 g_SECONDARY_ROAD_PROJECTION_SECTION_COUNT;

void
complete_secondary_road_projection(void)
{
  Window *secondary_window;

  secondary_window = (Window *) g_RENDER_THREAD_STATE.allocated_memory;
  reset_ccb_chain_for_display();
  store_display_buffer_descriptor_snapshot((const DisplayBufferDescriptor *) secondary_window);
  prepare_secondary_road_view();
  render_secondary_road_geometry(g_SECONDARY_ROAD_PROJECTION_SECTION_COUNT);
  finalize_current_ccb_chain();
  (void) draw_bitmap_cels(secondary_window->bitmap_item,
                          (CCB *) g_CCB_MANAGER_STATE.allocation_start);
  configure_simulation_projection(g_FRONTEND_RENDER_STATE.projection.source, 0);
}
