#include "horizon_render.h"

#include "bitmap_clip.h"
#include "formatted_output.h"
#include "game_data_path.h"
#include "game_session_resources.h"
#include "geometry.h"
#include "memory.h"
#include "pointer_array.h"
#include "quad_mapping.h"
#include "render_bounds.h"
#include "render_thread_signal.h"
#include "simulation_snapshot.h"

#include "operamath.h"
#include "operror.h"

#include "stddef.h"

#define HORIZON_FIXED16_SHIFT 16
#define HORIZON_POSITION_SHIFT 10
#define HORIZON_VERTEX_PROJECTION_SHIFT 8
#define HORIZON_VERTICAL_CENTER 120
#define HORIZON_SCREEN_BOTTOM 240
#define HORIZON_DIRECTION_EDGE_LIMIT 655
#define HORIZON_MATRIX_DIMENSION 3
#define HORIZON_INITIAL_PROJECTION_SCALE_FIXED (-15)
#define HORIZON_RING_SEGMENT_COUNT 24
#define HORIZON_RING_VERTICES_PER_SEGMENT 2
#define HORIZON_RING_TRAILING_VERTEX_COUNT 2
#define HORIZON_RING_MAX_PROJECTED_POINT_COUNT 50
#define HORIZON_RING_SOURCE_CEL_COUNT 6
#define HORIZON_RESOURCE_PATH_BYTES 128
#define HORIZON_RESOURCE_PRIMARY_PACKET_INDEX 3
#define HORIZON_RESOURCE_SECONDARY_PACKET_INDEX 4
#define HORIZON_RING_POINT_COUNT 10
#define HORIZON_RING_ANGLE_SHIFT 24
#define HORIZON_QUAD_POINT_COUNT 4
#define HORIZON_RING_ANGLE_ORIGIN_FIXED 0x320000
#define HORIZON_RING_ANGLE_STEP_FIXED 0x0AAAAA
#define HORIZON_RIGHT_CLIP_FIXED 0x1400000
#define HORIZON_VISIBLE_CEL_THRESHOLD 3
#define HORIZON_DISPLAY_HEIGHT_PIXELS 240
#define HORIZON_BITMAP_PAGE_BYTES 0x800
#define HORIZON_FILL_PIXEL_SHIFT 16
#define HORIZON_VRAM_WRITE_MASK -1

enum
{
  HORIZON_QUAD_TOP_LEFT_INDEX     = 0,
  HORIZON_QUAD_TOP_RIGHT_INDEX    = 1,
  HORIZON_QUAD_BOTTOM_RIGHT_INDEX = 2,
  HORIZON_QUAD_BOTTOM_LEFT_INDEX  = 3
};

typedef char SizeAssertHorizonRingCelsOffset[(offsetof(HorizonRing, cels) == 0x4A4) ? 1 : -1];
typedef char SizeAssertHorizonRingSize[(sizeof(HorizonRing) == 0x1180) ? 1 : -1];
typedef char SizeAssertHorizonRenderStateSize[(sizeof(HorizonRenderState) == 0x28) ? 1 : -1];

HorizonRenderState g_HORIZON_RENDER_STATE;
int32              g_HORIZON_PROJECTION_SHIFT = HORIZON_VERTEX_PROJECTION_SHIFT;

HorizonRing *
create_horizon_ring(int32          vertical_offset_,
                    int32          radius_fixed_,
                    const char    *resource_name_,
                    const Vector3 *origin_,
                    int32          resource_packet_index_)
{
  int32        angle;
  uint32       cel_index;
  CCB         *cel;
  HorizonRing *ring;
  char         resource_path[HORIZON_RESOURCE_PATH_BYTES];
  uint32       vertex_index;

  ring = (HorizonRing *) allocate_named_memory("HR_Ring", sizeof(*ring), 0);
  if(ring == NULL)
    return NULL;

  ring->enabled = (vertical_offset_ >= 1);
  sprintf(resource_path,
          "%s%s%s.fam",
          g_GAME_DATA_PATH,
          "DriveData/DriveArt/Horizons/",
          resource_name_);

  if(resource_packet_index_ == 0)
    {
      g_HORIZON_CEL_RESOURCE =
        acquire_game_session_resource_packet(g_GAME_SESSION_RESOURCE_BUFFER_INDEX,
                                             HORIZON_RESOURCE_PRIMARY_PACKET_INDEX);
      while(g_HORIZON_CEL_RESOURCE == NULL)
        {
          g_HORIZON_CEL_RESOURCE =
            acquire_game_session_resource_packet(g_GAME_SESSION_RESOURCE_BUFFER_INDEX,
                                                 HORIZON_RESOURCE_PRIMARY_PACKET_INDEX);
        }
    }
  else
    {
      g_HORIZON_CEL_RESOURCE =
        acquire_game_session_resource_packet(g_GAME_SESSION_RESOURCE_BUFFER_INDEX,
                                             HORIZON_RESOURCE_SECONDARY_PACKET_INDEX);
      while(g_HORIZON_CEL_RESOURCE == NULL)
        {
          g_HORIZON_CEL_RESOURCE =
            acquire_game_session_resource_packet(g_GAME_SESSION_RESOURCE_BUFFER_INDEX,
                                                 HORIZON_RESOURCE_SECONDARY_PACKET_INDEX);
        }
    }

  ring->resource_packet = g_HORIZON_CEL_RESOURCE;
  for(cel_index = 0; cel_index < HORIZON_RING_SOURCE_CEL_COUNT; cel_index += 1)
    {
      ring->source_cels[cel_index] =
        (const CCB *) get_pointer_array_element((void *const *) g_HORIZON_CEL_RESOURCE,
                                                (int32) cel_index);
    }

  ring->origin           = *origin_;
  ring->radius_fixed     = radius_fixed_;
  ring->point_count      = HORIZON_RING_POINT_COUNT;
  ring->active_cel_index = 0;
  for(cel_index = 0; cel_index < HORIZON_RING_CEL_COUNT; cel_index += 1)
    {
      cel          = &ring->cels[cel_index];
      *cel         = *ring->source_cels[cel_index % HORIZON_RING_SOURCE_CEL_COUNT];
      angle        = ((uint32) cel_index << HORIZON_RING_ANGLE_SHIFT) / HORIZON_RING_SEGMENT_COUNT;
      vertex_index = cel_index * HORIZON_RING_VERTICES_PER_SEGMENT;
      ring->vertices[vertex_index].x = MulSF16(SinF16((frac16) angle), radius_fixed_) + origin_->x;
      ring->vertices[vertex_index].y = origin_->y + vertical_offset_;
      ring->vertices[vertex_index].z = MulSF16(CosF16((frac16) angle), radius_fixed_) + origin_->z;
      ring->vertices[vertex_index + 1].x =
        MulSF16(SinF16((frac16) angle), radius_fixed_) + origin_->x;
      ring->vertices[vertex_index + 1].y = origin_->y;
      ring->vertices[vertex_index + 1].z =
        MulSF16(CosF16((frac16) angle), radius_fixed_) + origin_->z;
      cel->ccb_NextPtr = &ring->cels[(cel_index + 1) % HORIZON_RING_CEL_COUNT];
      cel->ccb_Flags &= ~(CCB_SKIP | CCB_LAST);
      cel->ccb_Flags |= CCB_NPABS;
    }

  return ring;
}

static void
_draw_horizon_ring(Item bitmap_item_, HorizonRing *ring_);
static int32
_calculate_horizon_screen_position(const Vector3 *direction_);
static void
_update_horizon_ring(HorizonRing *ring_, const Vector3 *camera_translation_);
static uint32
_is_horizon_quad_visible(const Point2 *quad_);

void
update_horizon_render_projection(const SimulationRenderState       *state_,
                                 const SimulationViewConfiguration *view_configuration_)
{
  Vector3 camera_translation;

  g_HORIZON_PROJECTION_SCALE_FIXED =
    ((uint32) g_RENDER_BOUNDS.horizontal_projection_scale << HORIZON_FIXED16_SHIFT);
  g_HORIZON_VERTICAL_ORIGIN_FIXED =
    ((view_configuration_->vertical_origin + HORIZON_VERTICAL_CENTER) << HORIZON_FIXED16_SHIFT);
  camera_translation.x = (-state_->render_position.x >> HORIZON_POSITION_SHIFT);
  camera_translation.y = (-state_->render_position.y >> HORIZON_POSITION_SHIFT);
  camera_translation.z = (-state_->render_position.z >> HORIZON_POSITION_SHIFT);
  update_horizon_ring_projection(&camera_translation);
}

void
render_horizon(Item display_resource_item_, Item bitmap_item_)
{
  fill_horizon_bitmap(display_resource_item_,
                      g_HORIZON_RENDER_STATE.primary_fill_value,
                      g_HORIZON_RENDER_STATE.secondary_fill_value);
  _draw_horizon_ring(bitmap_item_, g_HORIZON_RINGS[1]);
  _draw_horizon_ring(bitmap_item_, g_HORIZON_RINGS[0]);
}

void
update_horizon_ring_projection(const Vector3 *camera_translation_)
{
  const Matrix3x3 *view_matrix;
  Vector3          horizon_direction;

  view_matrix         = (const Matrix3x3 *) g_FRONTEND_RENDER_STATE.projection.view_state;
  horizon_direction.x = view_matrix->m02;
  horizon_direction.y = 0;
  horizon_direction.z = view_matrix->m22;
  MulVec3Mat33_F16((frac16 *) &horizon_direction,
                   (frac16 *) &horizon_direction,
                   (frac16(*)[HORIZON_MATRIX_DIMENSION]) view_matrix);
  g_HORIZON_SCREEN_POSITION = _calculate_horizon_screen_position(&horizon_direction);

  _update_horizon_ring(g_HORIZON_RINGS[1], camera_translation_);
  _update_horizon_ring(g_HORIZON_RINGS[0], camera_translation_);
}

void
update_horizon_ring_cels(HorizonRing *ring_, const Vector3 *camera_translation_)
{
  const Matrix3x3 *view_matrix;
  uint32           cel_index;
  uint32           point_index;
  uint32           projected_point_count;
  uint8            visible_cel_count;
  CCB             *cel;
  Point2           projected[HORIZON_RING_MAX_PROJECTED_POINT_COUNT];
  Point2           quad[HORIZON_QUAD_POINT_COUNT];
  Vector3          shifted_translation;
  Vector3          transformed[HORIZON_RING_MAX_PROJECTED_POINT_COUNT];

  ring_->active_cel_index =
    ((g_SIMULATION_RENDER_STATE.current_vector.y - HORIZON_RING_ANGLE_ORIGIN_FIXED) /
     HORIZON_RING_ANGLE_STEP_FIXED) %
    HORIZON_RING_SEGMENT_COUNT;
  if(ring_->active_cel_index < 0)
    ring_->active_cel_index += HORIZON_RING_SEGMENT_COUNT;

  shifted_translation.x = (camera_translation_->x >> g_HORIZON_PROJECTION_SHIFT);
  shifted_translation.y = (camera_translation_->y >> g_HORIZON_PROJECTION_SHIFT);
  shifted_translation.z = (camera_translation_->z >> g_HORIZON_PROJECTION_SHIFT);
  projected_point_count =
    ((ring_->point_count * HORIZON_RING_VERTICES_PER_SEGMENT) + HORIZON_RING_TRAILING_VERTEX_COUNT);
  translate_shifted_vector3_array(
    (int32) projected_point_count,
    &ring_->vertices[ring_->active_cel_index * HORIZON_RING_VERTICES_PER_SEGMENT],
    (uint32) g_HORIZON_PROJECTION_SHIFT,
    &shifted_translation,
    transformed);
  view_matrix = (const Matrix3x3 *) g_FRONTEND_RENDER_STATE.projection.view_state;
  MulManyVec3Mat33_F16((vec3f16 *) transformed,
                       (vec3f16 *) transformed,
                       *(mat33f16 *) view_matrix,
                       (int32) projected_point_count);
  project_vector3_array_with_depth_table((int32) projected_point_count, transformed, projected, 0);
  mask_shifted_point2_array((int32) projected_point_count,
                            projected,
                            0,
                            &g_PROJECTED_POINT_COORDINATE_MASKS,
                            projected);

  visible_cel_count = 0;
  cel_index         = (uint32) ring_->active_cel_index;
  cel               = &ring_->cels[cel_index];
  for(point_index = 0; point_index < ring_->point_count; point_index++)
    {
      quad[HORIZON_QUAD_TOP_LEFT_INDEX] =
        projected[(point_index * HORIZON_RING_VERTICES_PER_SEGMENT) + HORIZON_QUAD_TOP_RIGHT_INDEX];
      quad[HORIZON_QUAD_TOP_RIGHT_INDEX] =
        projected[point_index * HORIZON_RING_VERTICES_PER_SEGMENT];
      quad[HORIZON_QUAD_BOTTOM_RIGHT_INDEX] =
        projected[(point_index * HORIZON_RING_VERTICES_PER_SEGMENT) +
                  HORIZON_QUAD_BOTTOM_RIGHT_INDEX];
      quad[HORIZON_QUAD_BOTTOM_LEFT_INDEX] =
        projected[(point_index * HORIZON_RING_VERTICES_PER_SEGMENT) +
                  HORIZON_QUAD_BOTTOM_LEFT_INDEX];
      cel->ccb_Flags &= ~CCB_LAST;
      if(_is_horizon_quad_visible(quad) != 0)
        {
          visible_cel_count++;
          cel->ccb_Flags &= ~CCB_SKIP;
          map_fixed16_quad_to_ccb((CcbTemplate *) cel, quad, cel->ccb_Width, cel->ccb_Height);
        }
      else
        {
          cel->ccb_Flags |= CCB_SKIP;
          if(visible_cel_count > HORIZON_VISIBLE_CEL_THRESHOLD)
            {
              cel->ccb_Flags |= CCB_LAST;
              break;
            }
        }

      cel++;
    }

  ring_->cels[cel_index + ring_->point_count - 1].ccb_Flags |= CCB_LAST;
}

void
fill_horizon_bitmap(Item   display_resource_item_,
                    uint32 primary_fill_value_,
                    uint32 secondary_fill_value_)
{
  Bitmap *bitmap;
  Err     status;
  int32   primary_page_count;
  int32   total_page_count;
  uint32  primary_fill_word;
  uint32  secondary_fill_word;
  Screen *screen;

  screen           = (Screen *) LookupItem(display_resource_item_);
  bitmap           = screen->scr_TempBitmap;
  total_page_count = g_HORIZON_BITMAP_PAGE_COUNT;
  /* ARM60 signed multiplication wraps before the signed division. */
  primary_page_count =
    ((g_HORIZON_SCREEN_POSITION * total_page_count) / HORIZON_DISPLAY_HEIGHT_PIXELS);
  primary_fill_word = (primary_fill_value_ | (primary_fill_value_ << HORIZON_FILL_PIXEL_SHIFT));

  if(primary_page_count >= total_page_count)
    {
      status = SetVRAMPages(g_RENDER_THREAD_STATE.vram_io_request,
                            bitmap->bm_Buffer,
                            (int32) primary_fill_word,
                            total_page_count,
                            HORIZON_VRAM_WRITE_MASK);
      if(status < 0)
        PrintfSysErr(status);
      return;
    }

  secondary_fill_word =
    (secondary_fill_value_ | (secondary_fill_value_ << HORIZON_FILL_PIXEL_SHIFT));
  if(primary_page_count <= 0)
    {
      status = SetVRAMPages(g_RENDER_THREAD_STATE.vram_io_request,
                            bitmap->bm_Buffer,
                            (int32) secondary_fill_word,
                            total_page_count,
                            HORIZON_VRAM_WRITE_MASK);
      if(status < 0)
        PrintfSysErr(status);
      return;
    }

  status = SetVRAMPages(g_RENDER_THREAD_STATE.vram_io_request,
                        bitmap->bm_Buffer,
                        (int32) primary_fill_word,
                        primary_page_count,
                        HORIZON_VRAM_WRITE_MASK);
  if(status < 0)
    PrintfSysErr(status);

  status = SetVRAMPages(g_RENDER_THREAD_STATE.vram_io_request,
                        bitmap->bm_Buffer + (primary_page_count * HORIZON_BITMAP_PAGE_BYTES),
                        (int32) secondary_fill_word,
                        total_page_count - primary_page_count,
                        HORIZON_VRAM_WRITE_MASK);
  if(status < 0)
    PrintfSysErr(status);
}

static void
_draw_horizon_ring(Item bitmap_item_, HorizonRing *ring_)
{
  if(ring_->enabled == 0)
    return;

  (void) draw_bitmap_cels(bitmap_item_, &ring_->cels[ring_->active_cel_index]);
}

static int32
_calculate_horizon_screen_position(const Vector3 *direction_)
{
  frac16 direction_ratio;
  frac16 projected_offset;

  if((direction_->z > -HORIZON_DIRECTION_EDGE_LIMIT) &&
     (direction_->z < HORIZON_DIRECTION_EDGE_LIMIT))
    {
      if(direction_->z > 0)
        {
          if(direction_->y > 0)
            return 0;
        }
      else if(direction_->y <= 0)
        return 0;

      return HORIZON_SCREEN_BOTTOM;
    }

  direction_ratio  = DivSF16(direction_->y, direction_->z);
  projected_offset = MulSF16(g_HORIZON_PROJECTION_SCALE_FIXED, direction_ratio);
  return ((g_HORIZON_VERTICAL_ORIGIN_FIXED - projected_offset) >> HORIZON_FIXED16_SHIFT);
}

static void
_update_horizon_ring(HorizonRing *ring_, const Vector3 *camera_translation_)
{
  if(ring_->enabled == 0)
    return;

  update_horizon_ring_cels(ring_, camera_translation_);
}

static uint32
_is_horizon_quad_visible(const Point2 *quad_)
{
  if((quad_[HORIZON_QUAD_TOP_LEFT_INDEX].x > HORIZON_RIGHT_CLIP_FIXED) &&
     (quad_[HORIZON_QUAD_TOP_RIGHT_INDEX].x > HORIZON_RIGHT_CLIP_FIXED))
    return 0;
  if((quad_[HORIZON_QUAD_BOTTOM_RIGHT_INDEX].x < 0) &&
     (quad_[HORIZON_QUAD_BOTTOM_LEFT_INDEX].x < 0))
    return 0;

  return 1;
}
