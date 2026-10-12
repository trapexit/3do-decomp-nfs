/* Coordinates one gameplay render pass and its display-buffer handoff. */

#include "render_frame.h"

#include "bitmap_clip.h"
#include "ccb_manager.h"
#include "display_buffer_state.h"
#include "distance_slice_window.h"
#include "driving_warning_display.h"
#include "frontend_render_state.h"
#include "game_frame.h"
#include "game_session_resources.h"
#include "horizon_render.h"
#include "render_thread_signal.h"
#include "road_object_table.h"
#include "road_processing.h"
#include "road_render.h"
#include "simulation_snapshot.h"
#include "track_disk_stream.h"
#include "track_processing_cache.h"
#include "track_relocation.h"
#include "vbl_timer.h"
#include "vehicle_simulation_state.h"
#include "video_window_state.h"
#include "window_line.h"

#include "graphics.h"

#include "stddef.h"

#define RENDER_FRAME_OTHER_BUFFER_INDEX 1
#define RENDER_FRAME_RESET_DELAY 3
#define RENDER_FRAME_VRAM_WAIT_COMMAND 16
#define RENDER_FRAME_NO_VRAM_IO_REQUEST -2
#define RENDER_FRAME_CE_CONTROL 0x24500000UL
#define RENDER_FRAME_CE_CONTROL_MASK 0xFDF00000UL
#define RENDER_FRAME_RESOURCE_PACKET_INDEX 0
#define RENDER_FRAME_COMPLETE_STATE_LIMIT 2
#define RENDER_FRAME_DISTANCE_SLICE_FLAG 1
#define RENDER_FRAME_DISTANCE_SLICE_TOGGLE 2
#define RENDER_FRAME_HORIZON_DISABLED_FLAG 2
#define RENDER_FRAME_WINDOW_LINE_WIDTH 0x140
#define RENDER_FRAME_VBL_INTERVAL 60U
#define RENDER_FRAME_RATE_SCALE 6000U

typedef struct FrameFrontendState
{
  uint8 reserved_00[0x0C];
  uint8 render_mode_zero;
  uint8 render_flags;
} FrameFrontendState;

typedef struct RenderFrameClipBounds
{
  uint8 reserved_00[0x10];
  int32 clip_width;
  int32 clip_height;
} RenderFrameClipBounds;

typedef char RenderFrameTimingStateSize[(sizeof(RenderFrameTimingState) == 0x1C) ? 1 : -1];
typedef char
  RenderThreadVramIoRequestOffset[(offsetof(RenderThreadState, vram_io_request) == 0x78) ? 1 : -1];
typedef char RenderThreadSimulationUpdateSuspendedOffset
  [(offsetof(RenderThreadState, simulation_update_suspended) == 0x7C) ? 1 : -1];

RenderFrameTimingState g_RENDER_FRAME_TIMING_STATE;

void
render_next_frame_callback(void)
{
  render_next_frame();
}

void
render_next_frame(void)
{
  FrameFrontendState    *frame_frontend;
  RenderFrameClipBounds *clip_bounds;
  SimulationRenderState *simulation_state;
  int32                  display_buffer_index;
  int32                  next_display_buffer_index;
  uint32                 vbl_delta;

  if(g_RENDER_THREAD_STATE.render_flags != 0)
    {
      g_TRACK_PROCESSING_CACHE.section_batch_count = g_RENDER_THREAD_STATE.distance_slice_index + 1;
    }

  if((g_DRIVING_WARNING_DISPLAY_UPDATE_PENDING != 0) ||
     (g_FRONTEND_RENDER_STATE.menu.render_reset_pending != 0))
    {
      simulation_state = (SimulationRenderState *) g_FRONTEND_RENDER_STATE.projection.source;
      update_simulation_state(simulation_state);
      capture_simulation_render_state(simulation_state);
    }

  g_BITMAP_DISPLAY_STATE.current_bitmap_index =
    RENDER_FRAME_OTHER_BUFFER_INDEX - g_CURRENT_DISPLAY_BUFFER_INDEX;
  if(g_FRONTEND_RENDER_STATE.menu.render_reset_pending != 0)
    {
      g_FRONTEND_RENDER_STATE.menu.render_reset_pending = 0;
      if(g_FRONTEND_RENDER_STATE.menu.mode == 0)
        {
          g_RENDER_FRAME_TIMING_STATE.transition_delay = 0;
        }
      else
        {
          g_RENDER_FRAME_TIMING_STATE.transition_delay      = RENDER_FRAME_RESET_DELAY;
          g_RENDER_THREAD_STATE.simulation_update_suspended = RENDER_FRAME_NO_VRAM_IO_REQUEST;
        }
      g_RENDER_FRAME_TIMING_STATE.latest_vbl_delta      = 0;
      g_RENDER_FRAME_TIMING_STATE.frame_count           = 0;
      g_RENDER_FRAME_TIMING_STATE.interval_frame_count  = 0;
      g_RENDER_FRAME_TIMING_STATE.total_vbl_delta       = 0;
      g_RENDER_FRAME_TIMING_STATE.accumulated_vbl_delta = 0;
      (void) sample_vbl_delta();
      (void) SetCEControl(
        g_BITMAP_DISPLAY_STATE.bitmap_items[g_BITMAP_DISPLAY_STATE.current_bitmap_index],
        RENDER_FRAME_CE_CONTROL,
        RENDER_FRAME_CE_CONTROL_MASK);
      (void) SetCEControl(
        g_BITMAP_DISPLAY_STATE.bitmap_items[RENDER_FRAME_OTHER_BUFFER_INDEX -
                                            g_BITMAP_DISPLAY_STATE.current_bitmap_index],
        RENDER_FRAME_CE_CONTROL,
        RENDER_FRAME_CE_CONTROL_MASK);
    }

  if(g_RENDER_FRAME_TIMING_STATE.transition_delay != 0)
    {
      if(g_RENDER_FRAME_TIMING_STATE.transition_delay == 1)
        {
          g_RENDER_THREAD_STATE.simulation_update_suspended = RENDER_FRAME_VRAM_WAIT_COMMAND;
          while(g_RENDER_THREAD_STATE.simulation_update_suspended != 0)
            {
            }
        }
      --g_RENDER_FRAME_TIMING_STATE.transition_delay;
    }

  if(g_TRACK_DISK_STREAM_STATE.active == 0)
    return;

  if(acquire_game_session_resource_packet(g_GAME_SESSION_RESOURCE_BUFFER_INDEX,
                                          RENDER_FRAME_RESOURCE_PACKET_INDEX) == NULL)
    {
      return;
    }

  if((g_RENDER_THREAD_STATE.frame_complete < RENDER_FRAME_COMPLETE_STATE_LIMIT) &&
     (g_DRIVING_WARNING_DISPLAY_UPDATE_PENDING == 0))
    {
      if((g_RENDER_FRAME_TIMING_STATE.transition_delay == 0) &&
         (g_FRONTEND_RENDER_STATE.menu.transition_active == 0))
        {
          return;
        }
    }
  g_RENDER_THREAD_STATE.frame_complete = 0;

  apply_pending_simulation_track_position();
  update_road_geometry(g_FRONTEND_RENDER_STATE.projection.source);
  update_road_render_cels(0);
  configure_simulation_projection(g_FRONTEND_RENDER_STATE.projection.source, 0);
  rebuild_simulation_view_matrices();
  render_road_mesh_geometry(g_TRACK_PROCESSING_CACHE.section_batch_count);

  simulation_state = (SimulationRenderState *) g_FRONTEND_RENDER_STATE.projection.source;
  update_horizon_render_projection(simulation_state, simulation_state->render_view_configuration);
  frame_frontend                   = (FrameFrontendState *) &g_FRONTEND_RENDER_STATE;
  frame_frontend->render_mode_zero = (simulation_state->render_mode == 0);
  if(simulation_state->render_flag != 0)
    frame_frontend->render_flags |= RENDER_FRAME_DISTANCE_SLICE_FLAG;
  else
    frame_frontend->render_flags = 0;

  if((frame_frontend->render_flags & RENDER_FRAME_DISTANCE_SLICE_FLAG) != 0)
    {
      if((frame_frontend->render_flags == RENDER_FRAME_DISTANCE_SLICE_FLAG) &&
         (g_DRIVING_WARNING_DISPLAY_STATE.animation_frame < RENDER_FRAME_COMPLETE_STATE_LIMIT))
        {
          render_secondary_road_projection();
          complete_secondary_road_projection();
        }
      frame_frontend->render_flags ^= RENDER_FRAME_DISTANCE_SLICE_TOGGLE;
    }

  reset_ccb_chain_for_display();
  prepare_road_object_render_range();
  render_road_section_cel();
  render_road_scene_range(g_TRACK_PROCESSING_CACHE.section_batch_count,
                          simulation_state->render_track_segment_index);
  clip_bounds = (RenderFrameClipBounds *) g_FRONTEND_RENDER_STATE.projection.projection;
  (void) set_current_bitmap_clip(0, 0, clip_bounds->clip_width, clip_bounds->clip_height);

  display_buffer_index = g_BITMAP_DISPLAY_STATE.current_bitmap_index;
  if((g_RENDER_THREAD_STATE.render_flags & RENDER_FRAME_HORIZON_DISABLED_FLAG) == 0)
    {
      render_horizon(g_BITMAP_DISPLAY_STATE.display_resource_items[display_buffer_index],
                     g_BITMAP_DISPLAY_STATE.bitmap_items[display_buffer_index]);
    }
  prepare_pending_video_window();
  draw_window_line(0, 0, RENDER_FRAME_WINDOW_LINE_WIDTH, 0, 0);
  finalize_current_ccb_chain();
  if((g_RENDER_THREAD_STATE.render_flags & RENDER_FRAME_HORIZON_DISABLED_FLAG) == 0)
    {
      (void) draw_current_bitmap_cels((CCB *) g_CCB_MANAGER_STATE.allocation_start);
    }
  else
    {
      complete_distance_slice_capture();
    }
  (void) reset_current_bitmap_clip();

  if(frame_frontend->render_mode_zero == 0)
    render_driving_warning_display(&G_ACTIVE_SIMULATION_SOURCE->simulation_source.position);
  else
    render_driving_warning_display_variant(&G_ACTIVE_SIMULATION_SOURCE->simulation_source.position);
  g_RENDER_THREAD_STATE.frame_rendered = 1;

  (void) DisplayScreen(g_BITMAP_DISPLAY_STATE.display_resource_items[display_buffer_index], 0);
  g_RENDER_THREAD_STATE.frame_complete = 1;
  ++g_GAME_FRAME_STATE.frame_limit;
  next_display_buffer_index      = RENDER_FRAME_OTHER_BUFFER_INDEX - g_CURRENT_DISPLAY_BUFFER_INDEX;
  g_CURRENT_DISPLAY_BUFFER_INDEX = next_display_buffer_index;
  g_BITMAP_DISPLAY_STATE.current_bitmap_index =
    RENDER_FRAME_OTHER_BUFFER_INDEX - next_display_buffer_index;
  if(g_RENDER_FRAME_TIMING_STATE.transition_delay == 0)
    g_FRONTEND_RENDER_STATE.menu.render_complete = 1;

  ++g_RENDER_FRAME_TIMING_STATE.frame_count;
  ++g_RENDER_FRAME_TIMING_STATE.interval_frame_count;
  vbl_delta                                    = sample_vbl_delta();
  g_RENDER_FRAME_TIMING_STATE.latest_vbl_delta = vbl_delta;
  g_RENDER_FRAME_TIMING_STATE.total_vbl_delta += vbl_delta;
  g_RENDER_FRAME_TIMING_STATE.accumulated_vbl_delta += vbl_delta;
  if(g_RENDER_FRAME_TIMING_STATE.accumulated_vbl_delta < RENDER_FRAME_VBL_INTERVAL)
    {
      return;
    }
  g_RENDER_FRAME_TIMING_STATE.average_rate =
    g_RENDER_FRAME_TIMING_STATE.accumulated_vbl_delta /
    (g_RENDER_FRAME_TIMING_STATE.interval_frame_count * RENDER_FRAME_RATE_SCALE);
  g_RENDER_FRAME_TIMING_STATE.accumulated_vbl_delta = 0;
  g_RENDER_FRAME_TIMING_STATE.interval_frame_count  = 0;
}
