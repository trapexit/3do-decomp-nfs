#include "bitmap_clip.h"
#include "ccb_manager.h"
#include "cel_render.h"
#include "driving_warning_display.h"
#include "frontend_render_state.h"
#include "render_thread_signal.h"

#define DRIVING_WARNING_BACKDROP_WIDTH 64
#define DRIVING_WARNING_BACKDROP_HEIGHT 28
#define DRIVING_WARNING_BACKDROP_POINT_COUNT 4
#define DRIVING_WARNING_BACKDROP_TOP_RIGHT_INDEX 0
#define DRIVING_WARNING_BACKDROP_TOP_LEFT_INDEX 1
#define DRIVING_WARNING_BACKDROP_BOTTOM_LEFT_INDEX 2
#define DRIVING_WARNING_BACKDROP_BOTTOM_RIGHT_INDEX 3

typedef struct DrivingWarningBackdropResources
{
  uint8         reserved_00[0x2C];
  const uint32 *cel_header;
} DrivingWarningBackdropResources;

void
draw_driving_warning_backdrop(int32 left_, int32 top_)
{
  const DrivingWarningBackdropResources *resources;
  Point                                  quad[DRIVING_WARNING_BACKDROP_POINT_COUNT];

  if(g_FRONTEND_RENDER_STATE.menu.backdrop_enabled == 0)
    return;

  quad[DRIVING_WARNING_BACKDROP_TOP_RIGHT_INDEX].pt_X    = left_ + DRIVING_WARNING_BACKDROP_WIDTH;
  quad[DRIVING_WARNING_BACKDROP_TOP_RIGHT_INDEX].pt_Y    = top_;
  quad[DRIVING_WARNING_BACKDROP_TOP_LEFT_INDEX].pt_X     = left_;
  quad[DRIVING_WARNING_BACKDROP_TOP_LEFT_INDEX].pt_Y     = top_;
  quad[DRIVING_WARNING_BACKDROP_BOTTOM_LEFT_INDEX].pt_X  = left_;
  quad[DRIVING_WARNING_BACKDROP_BOTTOM_LEFT_INDEX].pt_Y  = top_ + DRIVING_WARNING_BACKDROP_HEIGHT;
  quad[DRIVING_WARNING_BACKDROP_BOTTOM_RIGHT_INDEX].pt_X = left_ + DRIVING_WARNING_BACKDROP_WIDTH;
  quad[DRIVING_WARNING_BACKDROP_BOTTOM_RIGHT_INDEX].pt_Y = top_ + DRIVING_WARNING_BACKDROP_HEIGHT;

  reset_ccb_chain_for_display();
  resources = (const DrivingWarningBackdropResources *) g_RENDER_THREAD_STATE.allocated_memory;
  draw_background_cel_header_to_quad(resources->cel_header, quad);
  finalize_current_ccb_chain();
  (void) draw_current_bitmap_cels((CCB *) g_CCB_MANAGER_STATE.allocation_start);
}
