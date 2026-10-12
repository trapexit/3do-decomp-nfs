#include "distance_slice_window.h"

#include "bitmap_clip.h"
#include "ccb_manager.h"
#include "cel_render.h"
#include "display_buffer_state.h"
#include "file_save.h"
#include "formatted_output.h"
#include "memory.h"
#include "render_thread_signal.h"
#include "road_data.h"
#include "road_object_table.h"
#include "road_processing.h"
#include "road_render.h"
#include "simulation_snapshot.h"
#include "track_disk_stream.h"
#include "track_processing_cache.h"
#include "window_fill.h"
#include "window_pixel.h"

#include "stddef.h"

#define DISTANCE_SLICE_WINDOW_DATA_OFFSET 0x50
#define DISTANCE_SLICE_ZERO_RUN_FLAG 0x80000000
#define DISTANCE_SLICE_WORD_BYTES 4
#define DISTANCE_SLICE_WORD_SHIFT 2
#define DISTANCE_SLICE_WINDOW_WIDTH 0x240
#define DISTANCE_SLICE_WINDOW_HEIGHT 0xDC
#define DISTANCE_SLICE_WINDOW_MEMORY_FLAGS 0x20
#define DISTANCE_SLICE_DEFAULT_BATCH_COUNT 0x10
#define DISTANCE_SLICE_RENDER_ENABLED 0x00000001
#define DISTANCE_SLICE_WINDOW_RENDER 0x00000002
#define DISTANCE_SLICE_DESCRIPTOR_BYTES 0x50
#define DISTANCE_SLICE_PATH_BYTES 80
#define DISTANCE_SLICE_PIXEL_BYTES 2
#define DISTANCE_SLICE_NO_FILE (-1)
#define DISTANCE_SLICE_BATCH_COUNT 12
#define DISTANCE_SLICE_MESH_POINT_STRIDE 11
#define DISTANCE_SLICE_MIDDLE_DIVISOR 2
#define DISTANCE_SLICE_FAR_DIVISOR 4
#define DISTANCE_SLICE_TRACK_SECTION_STEP 4
#define DISTANCE_SLICE_DESCRIPTOR_SCALE 0x100
#define DISTANCE_SLICE_FIXED16_SHIFT 16
#define DISTANCE_SLICE_NEAR_POINT_INDEX 0
#define DISTANCE_SLICE_MIDDLE_POINT_INDEX 1
#define DISTANCE_SLICE_FAR_POINT_INDEX 2
#define DISTANCE_SLICE_PREVIEW_LEFT 16
#define DISTANCE_SLICE_PREVIEW_TOP 16
#define DISTANCE_SLICE_PREVIEW_RIGHT 304
#define DISTANCE_SLICE_PREVIEW_BOTTOM 126
#define DISTANCE_SLICE_PREVIEW_POINT_COUNT 4
#define DISTANCE_SLICE_PREVIEW_TOP_LEFT 0
#define DISTANCE_SLICE_PREVIEW_TOP_RIGHT 1
#define DISTANCE_SLICE_PREVIEW_BOTTOM_RIGHT 2
#define DISTANCE_SLICE_PREVIEW_BOTTOM_LEFT 3
#define DISTANCE_SLICE_WINDOW_PATH_FORMAT ":DistSlices:%s:%s.horz%03d.win"
#define DISTANCE_SLICE_DESCRIPTOR_PATH_FORMAT ":DistSlices:%sdci:%04d.dci"

int32 g_DISTANCE_SLICE_SAVE_PENDING;
int32 g_DISTANCE_SLICE_FILE_INDEX;

typedef char SizeAssertRenderThreadDistanceSliceIndexOffset
  [(offsetof(RenderThreadState, distance_slice_index) == 0x14) ? 1 : -1];
typedef char SizeAssertRenderThreadDistanceSliceWindowOffset
  [(offsetof(RenderThreadState, distance_slice_window) == 0x24) ? 1 : -1];
typedef char SizeAssertWindowClipLeftOffset[(offsetof(Window, clip_left) == 0x08) ? 1 : -1];
typedef char SizeAssertWindowClipTopOffset[(offsetof(Window, clip_top) == 0x0C) ? 1 : -1];
typedef char SizeAssertWindowClipRightOffset[(offsetof(Window, clip_right) == 0x10) ? 1 : -1];
typedef char SizeAssertWindowClipBottomOffset[(offsetof(Window, clip_bottom) == 0x14) ? 1 : -1];

static void
_find_distance_slice_crop_origin(int32 *crop_x_, int32 *crop_y_);
static void
_offset_distance_slice_screen_points(int32 crop_x_, int32 crop_y_);

void
initialize_distance_slice_window(void)
{
  if(G_TRACK_DATA->stream_layout_mode == 0)
    {
      g_RENDER_THREAD_STATE.render_flags           = 0;
      g_TRACK_PROCESSING_CACHE.section_batch_count = DISTANCE_SLICE_DEFAULT_BATCH_COUNT;
    }
  else
    {
      g_RENDER_THREAD_STATE.render_flags           = DISTANCE_SLICE_RENDER_ENABLED;
      g_TRACK_PROCESSING_CACHE.section_batch_count = g_RENDER_THREAD_STATE.distance_slice_index + 1;
      g_ROAD_SECTION_CEL                           = NULL;
    }

  g_DISTANCE_SLICE_SAVE_PENDING = DISTANCE_SLICE_NO_FILE;
  g_DISTANCE_SLICE_FILE_INDEX   = DISTANCE_SLICE_NO_FILE;
  if((g_RENDER_THREAD_STATE.render_flags & DISTANCE_SLICE_WINDOW_RENDER) != 0)
    {
      g_RENDER_THREAD_STATE.distance_slice_window =
        allocate_window(DISTANCE_SLICE_WINDOW_WIDTH,
                        DISTANCE_SLICE_WINDOW_HEIGHT,
                        DISTANCE_SLICE_WINDOW_MEMORY_FLAGS);
    }
  else
    {
      g_RENDER_THREAD_STATE.distance_slice_window = NULL;
    }
}

void
free_distance_slice_window(void)
{
  if(g_RENDER_THREAD_STATE.distance_slice_window != NULL)
    free_memory(g_RENDER_THREAD_STATE.distance_slice_window);
}

int32
compress_distance_slice_window(Window *window_, int32 byte_count_)
{
  uint32 *read_pointer;
  uint32 *write_pointer;
  int32   output_word_count;
  int32   remaining_word_count;

  if(byte_count_ < 0)
    byte_count_ += DISTANCE_SLICE_WORD_BYTES - 1;
  remaining_word_count = byte_count_ >> DISTANCE_SLICE_WORD_SHIFT;
  read_pointer         = (uint32 *) ((uint8 *) window_ + DISTANCE_SLICE_WINDOW_DATA_OFFSET);
  write_pointer        = read_pointer;
  output_word_count    = 0;

  while(remaining_word_count != 0)
    {
      uint32 value;

      value = *read_pointer;
      if(value == 0)
        {
          uint32 zero_run_length;

          zero_run_length = 0;
          do
            {
              zero_run_length++;
              remaining_word_count--;
              read_pointer++;
            }
          while((remaining_word_count != 0) && (*read_pointer == 0));
          *write_pointer = zero_run_length | DISTANCE_SLICE_ZERO_RUN_FLAG;
        }
      else
        {
          read_pointer++;
          remaining_word_count--;
          *write_pointer = value;
        }

      write_pointer++;
      output_word_count++;
    }

  return output_word_count << DISTANCE_SLICE_WORD_SHIFT;
}

void
save_distance_slice_files(void)
{
  char    window_path[DISTANCE_SLICE_PATH_BYTES];
  char    descriptor_path[DISTANCE_SLICE_PATH_BYTES];
  Window *window;
  int32   byte_count;

  if(g_DISTANCE_SLICE_SAVE_PENDING == DISTANCE_SLICE_NO_FILE)
    return;

  sprintf(window_path,
          DISTANCE_SLICE_WINDOW_PATH_FORMAT,
          g_ROAD_RUNTIME_STATE.track_name,
          g_ROAD_RUNTIME_STATE.track_name,
          g_DISTANCE_SLICE_FILE_INDEX);
  sprintf(descriptor_path,
          DISTANCE_SLICE_DESCRIPTOR_PATH_FORMAT,
          g_ROAD_RUNTIME_STATE.track_name,
          g_DISTANCE_SLICE_FILE_INDEX);

  window     = g_RENDER_THREAD_STATE.distance_slice_window;
  byte_count = ((window->clip_right - window->clip_left) *
                (window->clip_bottom - window->clip_top) * DISTANCE_SLICE_PIXEL_BYTES);
  byte_count = compress_distance_slice_window(window, byte_count);
  save_file_with_error(window_path, window, byte_count + DISTANCE_SLICE_WINDOW_DATA_OFFSET);
  save_file_with_error(descriptor_path, &g_ROAD_RENDER_DESCRIPTOR, DISTANCE_SLICE_DESCRIPTOR_BYTES);

  g_DISTANCE_SLICE_SAVE_PENDING = DISTANCE_SLICE_NO_FILE;
  g_DISTANCE_SLICE_FILE_INDEX   = DISTANCE_SLICE_NO_FILE;
}

uint32
scan_distance_slice_row(int32 y_)
{
  Window *window;
  uint32  pixel;
  int32   x;

  pixel  = 0;
  window = g_RENDER_THREAD_STATE.distance_slice_window;
  x      = window->clip_right - window->clip_left;
  while(x != 0)
    {
      x--;
      pixel |= read_window_pixel(x, y_);
      if(pixel != 0)
        break;
    }

  return pixel;
}

uint32
scan_distance_slice_column(int32 x_)
{
  Window *window;
  uint32  pixel;
  int32   y;

  pixel  = 0;
  window = g_RENDER_THREAD_STATE.distance_slice_window;
  y      = window->clip_bottom - window->clip_top;
  while(y != 0)
    {
      y--;
      pixel |= read_window_pixel(x_, y);
      if(pixel != 0)
        break;
    }

  return pixel;
}

void
render_distance_slice_window(void)
{
  SimulationProjectionSource *source;
  Window                     *window;
  int32                       crop_x;
  int32                       crop_y;
  int32                       mesh_point_index;
  int32                       section_count;
  int32                       track_section_position;

  source                                       = g_FRONTEND_RENDER_STATE.projection.source;
  g_TRACK_PROCESSING_CACHE.section_batch_count = DISTANCE_SLICE_BATCH_COUNT;
  invalidate_track_processing_cache();
  update_road_geometry(source);
  rebuild_simulation_view_matrices();
  configure_simulation_projection(source, 0);

  mesh_point_index =
    (g_ROAD_MESH_TRIANGLE_COUNT -
     ((g_TRACK_PROCESSING_CACHE.section_batch_count -
       g_RENDER_THREAD_STATE.distance_slice_index) *
      DISTANCE_SLICE_MESH_POINT_STRIDE));
  g_ROAD_RENDER_DESCRIPTOR.reference_points[DISTANCE_SLICE_NEAR_POINT_INDEX] =
    G_ROAD_MESH_VERTICES[mesh_point_index];

  mesh_point_index =
    (g_ROAD_MESH_TRIANGLE_COUNT -
     (((g_TRACK_PROCESSING_CACHE.section_batch_count -
        g_RENDER_THREAD_STATE.distance_slice_index) *
       DISTANCE_SLICE_MESH_POINT_STRIDE) /
      DISTANCE_SLICE_MIDDLE_DIVISOR));
  g_ROAD_RENDER_DESCRIPTOR.reference_points[DISTANCE_SLICE_MIDDLE_POINT_INDEX] =
    G_ROAD_MESH_VERTICES[mesh_point_index];

  mesh_point_index =
    (g_ROAD_MESH_TRIANGLE_COUNT -
     ((g_TRACK_PROCESSING_CACHE.section_batch_count * DISTANCE_SLICE_MESH_POINT_STRIDE) /
      DISTANCE_SLICE_FAR_DIVISOR));
  g_ROAD_RENDER_DESCRIPTOR.reference_points[DISTANCE_SLICE_FAR_POINT_INDEX] =
    G_ROAD_MESH_VERTICES[mesh_point_index];

  (void) project_world_point_to_screen(
    &g_ROAD_RENDER_DESCRIPTOR.reference_points[DISTANCE_SLICE_NEAR_POINT_INDEX],
    &g_ROAD_RENDER_DESCRIPTOR.screen_offsets[DISTANCE_SLICE_NEAR_POINT_INDEX]);
  (void) project_world_point_to_screen(
    &g_ROAD_RENDER_DESCRIPTOR.reference_points[DISTANCE_SLICE_MIDDLE_POINT_INDEX],
    &g_ROAD_RENDER_DESCRIPTOR.screen_offsets[DISTANCE_SLICE_MIDDLE_POINT_INDEX]);
  (void) project_world_point_to_screen(
    &g_ROAD_RENDER_DESCRIPTOR.reference_points[DISTANCE_SLICE_FAR_POINT_INDEX],
    &g_ROAD_RENDER_DESCRIPTOR.screen_offsets[DISTANCE_SLICE_FAR_POINT_INDEX]);
  g_ROAD_RENDER_DESCRIPTOR.scale.values.horizontal = DISTANCE_SLICE_DESCRIPTOR_SCALE;
  g_ROAD_RENDER_DESCRIPTOR.scale.values.vertical   = DISTANCE_SLICE_DESCRIPTOR_SCALE;

  window = g_RENDER_THREAD_STATE.distance_slice_window;
  reset_ccb_chain_for_display();
  prepare_road_object_render_range();
  store_display_buffer_descriptor_snapshot((const DisplayBufferDescriptor *) window);
  fill_window_rectangle(window->clip_left,
                        window->clip_top,
                        window->clip_right - window->clip_left,
                        window->clip_bottom - window->clip_top,
                        0);
  render_road_mesh_geometry(g_TRACK_PROCESSING_CACHE.section_batch_count);
  section_count = (g_TRACK_PROCESSING_CACHE.section_batch_count -
                   g_RENDER_THREAD_STATE.distance_slice_index);
  track_section_position =
    (source->track_section_position +
     (g_RENDER_THREAD_STATE.distance_slice_index * DISTANCE_SLICE_TRACK_SECTION_STEP));
  render_road_scene_range(section_count, track_section_position);
  finalize_current_ccb_chain();
  (void) draw_bitmap_cels(window->bitmap_item, (CCB *) g_CCB_MANAGER_STATE.allocation_start);
  store_display_buffer_descriptor_snapshot((const DisplayBufferDescriptor *) window);
  _find_distance_slice_crop_origin(&crop_x, &crop_y);
  _offset_distance_slice_screen_points(crop_x, crop_y);

  g_TRACK_PROCESSING_CACHE.section_batch_count = (g_RENDER_THREAD_STATE.distance_slice_index + 1);
  invalidate_track_processing_cache();
}

void
complete_distance_slice_capture(void)
{
  Point preview_quad[DISTANCE_SLICE_PREVIEW_POINT_COUNT];
  int32 current_bitmap_index;

  if(g_DISTANCE_SLICE_SAVE_PENDING != DISTANCE_SLICE_NO_FILE)
    {
      preview_quad[DISTANCE_SLICE_PREVIEW_TOP_LEFT].pt_X     = DISTANCE_SLICE_PREVIEW_LEFT;
      preview_quad[DISTANCE_SLICE_PREVIEW_TOP_LEFT].pt_Y     = DISTANCE_SLICE_PREVIEW_TOP;
      preview_quad[DISTANCE_SLICE_PREVIEW_TOP_RIGHT].pt_X    = DISTANCE_SLICE_PREVIEW_RIGHT;
      preview_quad[DISTANCE_SLICE_PREVIEW_TOP_RIGHT].pt_Y    = DISTANCE_SLICE_PREVIEW_TOP;
      preview_quad[DISTANCE_SLICE_PREVIEW_BOTTOM_RIGHT].pt_X = DISTANCE_SLICE_PREVIEW_RIGHT;
      preview_quad[DISTANCE_SLICE_PREVIEW_BOTTOM_RIGHT].pt_Y = DISTANCE_SLICE_PREVIEW_BOTTOM;
      preview_quad[DISTANCE_SLICE_PREVIEW_BOTTOM_LEFT].pt_X  = DISTANCE_SLICE_PREVIEW_LEFT;
      preview_quad[DISTANCE_SLICE_PREVIEW_BOTTOM_LEFT].pt_Y  = DISTANCE_SLICE_PREVIEW_BOTTOM;

      reset_ccb_chain_for_display();
      draw_background_cel_header_to_quad(g_RENDER_THREAD_STATE.distance_slice_window->preamble,
                                         preview_quad);
      finalize_current_ccb_chain();

      current_bitmap_index = g_BITMAP_DISPLAY_STATE.current_bitmap_index;
      (void) DrawCels(g_BITMAP_DISPLAY_STATE.bitmap_items[current_bitmap_index],
                      (CCB *) g_CCB_MANAGER_STATE.allocation_start);
      (void) DrawCels(g_BITMAP_DISPLAY_STATE.bitmap_items[1 - current_bitmap_index],
                      (CCB *) g_CCB_MANAGER_STATE.allocation_start);
    }

  save_distance_slice_files();
}

static void
_find_distance_slice_crop_origin(int32 *crop_x_, int32 *crop_y_)
{
  int32 x;
  int32 y;

  x = 0;
  while((x < DISTANCE_SLICE_WINDOW_WIDTH) && (scan_distance_slice_column(x) == 0))
    {
      x++;
    }

  y = 0;
  while((y < DISTANCE_SLICE_WINDOW_HEIGHT) && (scan_distance_slice_row(y) == 0))
    {
      y++;
    }

  *crop_x_ = x;
  *crop_y_ = y;
}

static void
_offset_distance_slice_screen_points(int32 crop_x_, int32 crop_y_)
{
  int32 index;
  int32 x_offset;
  int32 y_offset;

  x_offset = (crop_x_ << DISTANCE_SLICE_FIXED16_SHIFT);
  y_offset = (crop_y_ << DISTANCE_SLICE_FIXED16_SHIFT);
  for(index = 0; index < ROAD_RENDER_SCREEN_OFFSET_COUNT; index++)
    {
      g_ROAD_RENDER_DESCRIPTOR.screen_offsets[index].x -= x_offset;
      g_ROAD_RENDER_DESCRIPTOR.screen_offsets[index].y -= y_offset;
    }
}
