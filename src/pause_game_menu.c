#include "ccb_manager.h"
#include "cel_render.h"
#include "display_buffer_state.h"
#include "frontend_control_input.h"
#include "frontend_input_wait.h"
#include "frontend_state.h"
#include "game_audio_system.h"
#include "game_bootstrap.h"
#include "menu_option_selector.h"
#include "pause_game_display.h"
#include "pause_game_shape.h"
#include "quad_render.h"
#include "shape_blit.h"
#include "shape_lookup.h"
#include "simulation_resource_cleanup.h"
#include "simulation_thread_signal.h"
#include "sound_effect_state.h"
#include "track_position.h"
#include "vehicle_simulation_state.h"
#include "video_window_state.h"
#include "window_line.h"

#include "operamath.h"

#include "stddef.h"

#define PAUSE_GAME_MENU_NAVIGATE_DOWN 0x80000000UL
#define PAUSE_GAME_MENU_NAVIGATE_UP 0x40000000UL
#define PAUSE_GAME_MENU_CHANGE_OPTION 0x02000000UL
#define PAUSE_GAME_MENU_EXIT_MASK 0x0D000000UL
#define PAUSE_GAME_MENU_RELEASE_MASK 0x01000000UL
#define PAUSE_GAME_MENU_CONTROL_MASK 0xCF000000UL
#define PAUSE_GAME_MENU_OPTION_COUNT 4
#define PAUSE_GAME_MENU_OPTION_LAST_INDEX 2
#define PAUSE_GAME_MENU_SOUND_OPTION 2
#define PAUSE_GAME_MENU_SHAPE_NAME "menu"
#define PAUSE_GAME_HIGHLIGHT_NAME "lite"
#define PAUSE_GAME_SECONDARY_PROFILE_OFFSET 4
#define PAUSE_GAME_SHAPE_PACKED_POSITION_WORD 3
#define PAUSE_GAME_SHAPE_X_POSITION_SHIFT 16
#define PAUSE_GAME_BACKGROUND_PIXEL_CONTROL 0x1F011F01UL
#define PAUSE_GAME_FOREGROUND_PIXEL_CONTROL 0x1F001F00UL
#define PAUSE_GAME_SELECTION_X 60
#define PAUSE_GAME_SELECTION_Y_FIRST 65
#define PAUSE_GAME_SELECTION_Y_SECOND 91
#define PAUSE_GAME_SELECTION_Y_THIRD 117
#define PAUSE_GAME_SELECTION_Y_FOURTH 142
#define PAUSE_GAME_VALUE_DIVISOR_FIRST 3600
#define PAUSE_GAME_VALUE_DIVISOR_SECOND 600
#define PAUSE_GAME_VALUE_DIVISOR_THIRD 60
#define PAUSE_GAME_VALUE_DIGIT_RADIX 10
#define PAUSE_GAME_VALUE_DIGIT_COUNT 3
#define PAUSE_GAME_VALUE_DIGIT_NAME_BYTES 8
#define PAUSE_GAME_VALUE_DIGIT_FIRST_INDEX 0
#define PAUSE_GAME_VALUE_DIGIT_SECOND_INDEX 1
#define PAUSE_GAME_VALUE_DIGIT_THIRD_INDEX 2
#define PAUSE_GAME_VALUE_DIGIT_CHARACTER_BIAS '0'
#define PAUSE_GAME_VALUE_X_FIRST 270
#define PAUSE_GAME_VALUE_X_SECOND 281
#define PAUSE_GAME_VALUE_X_THIRD 288
#define PAUSE_GAME_VALUE_Y 203
#define PAUSE_GAME_DISPLAY_ENTRY_COUNT 1
#define PAUSE_GAME_TRACK_MAP_COLOR 0x1CE7
#define PAUSE_GAME_PRIMARY_VEHICLE_INDEX 0
#define PAUSE_GAME_SECONDARY_VEHICLE_INDEX 1
#define PAUSE_GAME_NEARBY_TRACK_GAP 75
#define PAUSE_GAME_DISABLED_SIMULATION_MODE 8
#define PAUSE_GAME_VIEWPORT_POSITIVE_LIMIT 0x75300000
#define PAUSE_GAME_VIEWPORT_NEGATIVE_LIMIT (-0x75300000)
#define PAUSE_GAME_VIEWPORT_MINIMUM_SPAN 0x7D0000
#define PAUSE_GAME_VIEWPORT_HALF_MINIMUM_SPAN 0x3E8000
#define PAUSE_GAME_VIEWPORT_HALF_SHIFT 1
#define PAUSE_GAME_VIEWPORT_MARGIN_SHIFT 3
#define PAUSE_GAME_SIGN_BIT_SHIFT 31
#define PAUSE_GAME_FIXED16_FRACTIONAL_BITS 16
#define PAUSE_GAME_TRACK_FIRST_SEGMENT 14
#define PAUSE_GAME_TRACK_LINE_THICKNESS 2
#define PAUSE_GAME_TRACK_BATCH_DIVIDEND 100
#define PAUSE_GAME_FINISH_MARKER_COLOR 0x7FEA
#define PAUSE_GAME_FINISH_MARKER_INNER_OFFSET 1
#define PAUSE_GAME_FINISH_MARKER_MIDDLE_OFFSET 2
#define PAUSE_GAME_FINISH_MARKER_OUTER_OFFSET 3
#define PAUSE_GAME_SECONDARY_MARKER_COLOR 0x5400
#define PAUSE_GAME_PRIMARY_MARKER_COLOR 0x5EF7
#define PAUSE_GAME_MARKER_PIXEL_CONTROL 0x1F001F00UL
#define PAUSE_GAME_MARKER_DIRECTION_SCALE 0x190000
#define PAUSE_GAME_MARKER_DEFAULT_DIRECTION 0x10000
#define PAUSE_GAME_MARKER_EDGE_MARGIN 3
#define PAUSE_GAME_MARKER_OUTLINE_OFFSET 1
#define PAUSE_GAME_MARKER_ROTATION_STEP 0xA0000
#define PAUSE_GAME_MARKER_ROTATION_LIMIT 0xFE0000
#define PAUSE_GAME_MAP_SHAPE_DIMENSIONS_WORD 2
#define PAUSE_GAME_MAP_SHAPE_WIDTH_SHIFT 17
#define PAUSE_GAME_MAP_SHAPE_HEIGHT_SHIFT 1
#define PAUSE_GAME_MAP_SHAPE_FALLBACK_INDEX 4
#define PAUSE_GAME_TRACK_DIRECTION_HIGH_BYTE_SHIFT 8

typedef struct PauseGameMapPoint
{
  int32 x;
  int32 y;
} PauseGameMapPoint;

typedef struct PauseGameMapMarker
{
  PauseGameMapPoint center;
  PauseGameMapPoint endpoint;
  int32             half_width;
  int32             half_height;
  const uint32     *shape;
} PauseGameMapMarker;

DisplayBufferDescriptor *g_PAUSE_GAME_DISPLAY_DESCRIPTOR;
DisplayBufferDescriptor *g_PAUSE_GAME_SECONDARY_DISPLAY_DESCRIPTOR;
int32 g_PAUSE_GAME_SELECTION_X[PAUSE_GAME_MENU_OPTION_COUNT] = { PAUSE_GAME_SELECTION_X,
                                                                 PAUSE_GAME_SELECTION_X,
                                                                 PAUSE_GAME_SELECTION_X,
                                                                 PAUSE_GAME_SELECTION_X };
int32 g_PAUSE_GAME_SELECTION_Y[PAUSE_GAME_MENU_OPTION_COUNT] = { PAUSE_GAME_SELECTION_Y_FIRST,
                                                                 PAUSE_GAME_SELECTION_Y_SECOND,
                                                                 PAUSE_GAME_SELECTION_Y_THIRD,
                                                                 PAUSE_GAME_SELECTION_Y_FOURTH };
int32 g_PAUSE_GAME_VALUE_X[PAUSE_GAME_VALUE_DIGIT_COUNT]     = { PAUSE_GAME_VALUE_X_FIRST,
                                                                 PAUSE_GAME_VALUE_X_SECOND,
                                                                 PAUSE_GAME_VALUE_X_THIRD };
int32 g_PAUSE_GAME_VALUE_Y[PAUSE_GAME_VALUE_DIGIT_COUNT]     = { PAUSE_GAME_VALUE_Y,
                                                                 PAUSE_GAME_VALUE_Y,
                                                                 PAUSE_GAME_VALUE_Y };

void
update_pause_game_menu_viewport(MenuViewport *viewport_)
{
  const SimulationSource *primary;
  const SimulationSource *secondary;
  int32                   track_gap;
  int32                   span;
  int32                   margin;

  primary   = &g_VEHICLE_SIMULATION_STATES[PAUSE_GAME_PRIMARY_VEHICLE_INDEX]->simulation_source;
  secondary = &g_VEHICLE_SIMULATION_STATES[PAUSE_GAME_SECONDARY_VEHICLE_INDEX]->simulation_source;
  track_gap = primary->track_segment_index - secondary->track_segment_index;
  if(track_gap <= 0)
    track_gap = -track_gap;

  viewport_->bounded_left   = PAUSE_GAME_VIEWPORT_POSITIVE_LIMIT;
  viewport_->bounded_top    = PAUSE_GAME_VIEWPORT_POSITIVE_LIMIT;
  viewport_->bounded_right  = PAUSE_GAME_VIEWPORT_NEGATIVE_LIMIT;
  viewport_->bounded_bottom = PAUSE_GAME_VIEWPORT_NEGATIVE_LIMIT;

  if(primary->position.x < viewport_->bounded_left)
    viewport_->bounded_left = primary->position.x;
  if(primary->position.z < viewport_->bounded_top)
    viewport_->bounded_top = primary->position.z;
  if(primary->position.x > viewport_->bounded_right)
    viewport_->bounded_right = primary->position.x;
  if(primary->position.z > viewport_->bounded_bottom)
    viewport_->bounded_bottom = primary->position.z;

  if((track_gap < PAUSE_GAME_NEARBY_TRACK_GAP) &&
     (g_STARTUP_CONFIGURATION.opponent_vehicle_index != PAUSE_GAME_DISABLED_SIMULATION_MODE))
    {
      if(secondary->position.x < viewport_->bounded_left)
        viewport_->bounded_left = secondary->position.x;
      if(secondary->position.z < viewport_->bounded_top)
        viewport_->bounded_top = secondary->position.z;
      if(secondary->position.x > viewport_->bounded_right)
        viewport_->bounded_right = secondary->position.x;
      if(secondary->position.z > viewport_->bounded_bottom)
        viewport_->bounded_bottom = secondary->position.z;
    }

  span = viewport_->bounded_right - viewport_->bounded_left;
  if(span < PAUSE_GAME_VIEWPORT_MINIMUM_SPAN)
    {
      span += (uint32) span >> PAUSE_GAME_SIGN_BIT_SHIFT;
      viewport_->bounded_left += span >> PAUSE_GAME_VIEWPORT_HALF_SHIFT;
      viewport_->bounded_left -= PAUSE_GAME_VIEWPORT_HALF_MINIMUM_SPAN;
      viewport_->bounded_right = viewport_->bounded_left + PAUSE_GAME_VIEWPORT_MINIMUM_SPAN;
    }
  else
    {
      margin = span >> PAUSE_GAME_VIEWPORT_MARGIN_SHIFT;
      viewport_->bounded_left -= margin;
      viewport_->bounded_right += margin;
    }

  span = viewport_->bounded_bottom - viewport_->bounded_top;
  if(span < PAUSE_GAME_VIEWPORT_MINIMUM_SPAN)
    {
      span += (uint32) span >> PAUSE_GAME_SIGN_BIT_SHIFT;
      viewport_->bounded_top += span >> PAUSE_GAME_VIEWPORT_HALF_SHIFT;
      viewport_->bounded_top -= PAUSE_GAME_VIEWPORT_HALF_MINIMUM_SPAN;
      viewport_->bounded_bottom = viewport_->bounded_top + PAUSE_GAME_VIEWPORT_MINIMUM_SPAN;
    }
  else
    {
      margin = span >> PAUSE_GAME_VIEWPORT_MARGIN_SHIFT;
      viewport_->bounded_top -= margin;
      viewport_->bounded_bottom += margin;
    }
}

void
render_pause_game_track_map(const MenuViewport  *viewport_,
                            const MenuRectangle *rectangle_,
                            const MenuScale     *scale_,
                            uint32               color_)
{
  const TrackSegment *segment;
  int32               last_segment_index;
  int32               segment_index;
  int32               previous_x;
  int32               previous_y;
  int32               current_x;
  int32               current_y;

  (void) scale_;
  previous_x         = 0;
  previous_y         = 0;
  last_segment_index = viewport_->reserved_00[0];
  if(last_segment_index < g_VEHICLE_SIMULATION_STATES[PAUSE_GAME_PRIMARY_VEHICLE_INDEX]
                            ->simulation_source.track_segment_index)
    {
      last_segment_index = g_VEHICLE_SIMULATION_STATES[PAUSE_GAME_PRIMARY_VEHICLE_INDEX]
                             ->simulation_source.track_segment_index;
    }
  if(last_segment_index < g_VEHICLE_SIMULATION_STATES[PAUSE_GAME_SECONDARY_VEHICLE_INDEX]
                            ->simulation_source.track_segment_index)
    {
      last_segment_index = g_VEHICLE_SIMULATION_STATES[PAUSE_GAME_SECONDARY_VEHICLE_INDEX]
                             ->simulation_source.track_segment_index;
    }

  for(segment_index = PAUSE_GAME_TRACK_FIRST_SEGMENT; segment_index <= last_segment_index;
      segment_index++)
    {
      segment   = &g_TRACK_SEGMENTS[segment_index];
      current_x = rectangle_->left + rectangle_->x_offset +
                  (MulSF16(segment->surface_reference.x, rectangle_->scaled_width) >>
                   PAUSE_GAME_FIXED16_FRACTIONAL_BITS);
      current_y = rectangle_->bottom - rectangle_->y_offset -
                  (MulSF16(segment->surface_reference.z, rectangle_->scaled_height) >>
                   PAUSE_GAME_FIXED16_FRACTIONAL_BITS);

      if((segment_index > PAUSE_GAME_TRACK_FIRST_SEGMENT) && (current_x >= rectangle_->left) &&
         (current_x <= rectangle_->right) && (current_y >= rectangle_->top) &&
         (current_y <= rectangle_->bottom) && (previous_x >= rectangle_->left) &&
         (previous_x <= rectangle_->right) && (previous_y >= rectangle_->top) &&
         (previous_y <= rectangle_->bottom))
        {
          draw_solid_line_segment(previous_x,
                                  previous_y,
                                  current_x,
                                  current_y,
                                  color_,
                                  PAUSE_GAME_TRACK_LINE_THICKNESS);
          if((segment_index % PAUSE_GAME_TRACK_BATCH_DIVIDEND) == 0)
            finalize_allocated_ccb_batch();
        }
      previous_x = current_x;
      previous_y = current_y;
    }
}

static const uint32 *
_get_pause_game_vehicle_map_shape(const VehicleSimulationState *state_)
{
  const RenderResourceRecord *resource;

  resource = state_->render_resource_state;
  if(resource->map_shape != NULL)
    return (const uint32 *) resource->map_shape;

  return (const uint32 *) resource->high_detail_shape_variants[PAUSE_GAME_MAP_SHAPE_FALLBACK_INDEX];
}

static void
_get_pause_game_map_shape_half_size(const uint32 *shape_, int32 *half_width_, int32 *half_height_)
{
  uint32 dimensions;

  if(shape_ == NULL)
    {
      *half_width_  = 0;
      *half_height_ = 0;
      return;
    }

  dimensions    = shape_[PAUSE_GAME_MAP_SHAPE_DIMENSIONS_WORD];
  *half_width_  = (int32) dimensions >> PAUSE_GAME_MAP_SHAPE_WIDTH_SHIFT;
  *half_height_ = (int32) (int16) dimensions >> PAUSE_GAME_MAP_SHAPE_HEIGHT_SHIFT;
}

static int32
_is_pause_game_map_marker_inside(const MenuRectangle     *rectangle_,
                                 const PauseGameMapPoint *point_,
                                 int32                    half_width_,
                                 int32                    half_height_)
{
  return ((point_->x - half_width_) >= rectangle_->left) &&
         ((point_->x + half_width_) <= rectangle_->right) &&
         ((point_->y - half_height_) >= rectangle_->top) &&
         ((point_->y + half_height_) <= rectangle_->bottom);
}

static void
_prepare_pause_game_map_marker(const VehicleSimulationState  *state_,
                               const MenuRectangle           *rectangle_,
                               const PauseGameOptionSelector *selector_,
                               int32                          direction_,
                               PauseGameMapMarker            *marker_)
{
  const SimulationSource *source;
  const TrackSegment     *segment;
  const uint8            *segment_bytes;
  int32                   track_direction_x;
  int32                   track_direction_z;
  int32                   marker_direction_x;
  int32                   marker_direction_z;
  int32                   position_x;
  int32                   position_z;

  source        = &state_->simulation_source;
  segment       = &g_TRACK_SEGMENTS[source->track_segment_index];
  segment_bytes = (const uint8 *) segment;
  track_direction_x =
    (int32) (int16) (((uint16) segment_bytes[offsetof(TrackSegment, surface_offset_x)]
                      << PAUSE_GAME_TRACK_DIRECTION_HIGH_BYTE_SHIFT) |
                     segment_bytes[offsetof(TrackSegment, surface_offset_x) + 1]);
  track_direction_x <<= 1;
  track_direction_z =
    (int32) (int16) (((uint16) segment_bytes[offsetof(TrackSegment, surface_offset_z)]
                      << PAUSE_GAME_TRACK_DIRECTION_HIGH_BYTE_SHIFT) |
                     segment_bytes[offsetof(TrackSegment, surface_offset_z) + 1]);
  track_direction_z <<= 1;
  position_x = source->position.x - MulSF16(source->lateral_position, track_direction_x);
  position_z = source->position.z - MulSF16(source->lateral_position, track_direction_z);

  marker_->center.x =
    rectangle_->left + rectangle_->x_offset +
    (MulSF16(position_x, rectangle_->scaled_width) >> PAUSE_GAME_FIXED16_FRACTIONAL_BITS);
  marker_->center.y =
    rectangle_->bottom - rectangle_->y_offset -
    (MulSF16(position_z, rectangle_->scaled_height) >> PAUSE_GAME_FIXED16_FRACTIONAL_BITS);

  if(selector_->options[selector_->selected_index] == 0)
    {
      marker_direction_x = PAUSE_GAME_MARKER_DEFAULT_DIRECTION;
      marker_direction_z = 0;
    }
  else
    {
      marker_direction_x = track_direction_x;
      marker_direction_z = track_direction_z;
    }
  marker_direction_x  = MulSF16(marker_direction_x, PAUSE_GAME_MARKER_DIRECTION_SCALE) >>
                        PAUSE_GAME_FIXED16_FRACTIONAL_BITS;
  marker_direction_z  = MulSF16(marker_direction_z, PAUSE_GAME_MARKER_DIRECTION_SCALE) >>
                        PAUSE_GAME_FIXED16_FRACTIONAL_BITS;
  marker_->endpoint.x = marker_->center.x + direction_ * marker_direction_x;
  marker_->endpoint.y = marker_->center.y - direction_ * marker_direction_z;
  marker_->shape      = _get_pause_game_vehicle_map_shape(state_);
  _get_pause_game_map_shape_half_size(marker_->shape, &marker_->half_width, &marker_->half_height);
}

static void
_find_pause_game_marker_edge_candidates(const MenuRectangle      *rectangle_,
                                        const PauseGameMapMarker *marker_,
                                        int32                     direction_,
                                        PauseGameMapPoint        *negative_,
                                        PauseGameMapPoint        *positive_)
{
  int32 direction_x;
  int32 direction_y;
  int32 angle;

  direction_x = marker_->endpoint.x - marker_->center.x;
  direction_y = marker_->endpoint.y - marker_->center.y;
  angle       = 0;
  do
    {
      calculate_rotated_menu_coordinates(&negative_->x,
                                         &negative_->y,
                                         angle,
                                         direction_ * direction_x,
                                         direction_ * direction_y);
      negative_->x = marker_->center.x + direction_ * negative_->x;
      negative_->y = marker_->center.y + direction_ * negative_->y;
      if(_is_pause_game_map_marker_inside(rectangle_,
                                          negative_,
                                          marker_->half_width,
                                          marker_->half_height))
        break;
      angle -= PAUSE_GAME_MARKER_ROTATION_STEP;
    }
  while(angle > -PAUSE_GAME_MARKER_ROTATION_LIMIT);

  angle = 0;
  do
    {
      calculate_rotated_menu_coordinates(&positive_->x,
                                         &positive_->y,
                                         angle,
                                         direction_ * direction_x,
                                         direction_ * direction_y);
      positive_->x = marker_->center.x + direction_ * positive_->x;
      positive_->y = marker_->center.y + direction_ * positive_->y;
      if(_is_pause_game_map_marker_inside(rectangle_,
                                          positive_,
                                          marker_->half_width,
                                          marker_->half_height))
        break;
      angle += PAUSE_GAME_MARKER_ROTATION_STEP;
    }
  while(angle < PAUSE_GAME_MARKER_ROTATION_LIMIT);
}

static int32
_pause_game_marker_separation_score(const PauseGameMapPoint  *candidate_,
                                    const PauseGameMapMarker *other_)
{
  int32  delta;
  uint32 score;

  delta = candidate_->x - other_->endpoint.x;
  score = (uint32) delta * (uint32) delta;
  delta = candidate_->y - other_->endpoint.y;
  score += (uint32) delta * (uint32) delta;
  delta = candidate_->x - other_->center.x;
  score += (uint32) delta * (uint32) delta;
  delta = candidate_->y - other_->center.y;
  score += (uint32) delta * (uint32) delta;
  return (int32) score;
}

static void
_fit_pause_game_map_marker(const MenuRectangle      *rectangle_,
                           PauseGameMapMarker       *marker_,
                           int32                     direction_,
                           const PauseGameMapMarker *other_)
{
  PauseGameMapPoint negative;
  PauseGameMapPoint positive;
  int32             negative_score;
  int32             positive_score;

  if(_is_pause_game_map_marker_inside(rectangle_,
                                      &marker_->endpoint,
                                      marker_->half_width,
                                      marker_->half_height))
    return;

  _find_pause_game_marker_edge_candidates(rectangle_, marker_, direction_, &negative, &positive);
  if(other_ == NULL)
    {
      marker_->endpoint = negative;
      return;
    }

  negative_score = _pause_game_marker_separation_score(&negative, other_);
  positive_score = _pause_game_marker_separation_score(&positive, other_);
  if(positive_score <= negative_score)
    marker_->endpoint = negative;
  else
    marker_->endpoint = positive;
}

static void
_draw_pause_game_map_marker(const PauseGameMapMarker *marker_, uint32 color_)
{
  draw_window_line(marker_->center.x + PAUSE_GAME_MARKER_OUTLINE_OFFSET,
                   marker_->center.y,
                   marker_->endpoint.x + PAUSE_GAME_MARKER_OUTLINE_OFFSET,
                   marker_->endpoint.y,
                   color_);
  draw_window_line(marker_->center.x - PAUSE_GAME_MARKER_OUTLINE_OFFSET,
                   marker_->center.y,
                   marker_->endpoint.x - PAUSE_GAME_MARKER_OUTLINE_OFFSET,
                   marker_->endpoint.y,
                   color_);
  draw_window_line(marker_->center.x,
                   marker_->center.y + PAUSE_GAME_MARKER_OUTLINE_OFFSET,
                   marker_->endpoint.x,
                   marker_->endpoint.y + PAUSE_GAME_MARKER_OUTLINE_OFFSET,
                   color_);
  draw_window_line(marker_->center.x,
                   marker_->center.y - PAUSE_GAME_MARKER_OUTLINE_OFFSET,
                   marker_->endpoint.x,
                   marker_->endpoint.y - PAUSE_GAME_MARKER_OUTLINE_OFFSET,
                   color_);
  set_ccb_pixel_control(PAUSE_GAME_MARKER_PIXEL_CONTROL);
  draw_window_line(marker_->center.x,
                   marker_->center.y,
                   marker_->endpoint.x,
                   marker_->endpoint.y,
                   color_);
  if(marker_->shape != NULL)
    draw_cel_header_relative_to_embedded_origin(marker_->shape,
                                                marker_->endpoint.x,
                                                marker_->endpoint.y);
}

static void
_draw_pause_game_finish_marker(const MenuViewport *viewport_, const MenuRectangle *rectangle_)
{
  const TrackSegment *segment;
  PauseGameMapPoint   point;

  if(viewport_->reserved_00[1] >= viewport_->reserved_00[0])
    return;

  segment = &g_TRACK_SEGMENTS[viewport_->reserved_00[1]];
  point.x = rectangle_->left + rectangle_->x_offset +
            (MulSF16(segment->surface_reference.x, rectangle_->scaled_width) >>
             PAUSE_GAME_FIXED16_FRACTIONAL_BITS);
  point.y = rectangle_->bottom - rectangle_->y_offset -
            (MulSF16(segment->surface_reference.z, rectangle_->scaled_height) >>
             PAUSE_GAME_FIXED16_FRACTIONAL_BITS);
  if((point.x < rectangle_->left) || (point.x > rectangle_->right) || (point.y < rectangle_->top) ||
     (point.y > rectangle_->bottom))
    return;

  draw_window_line(point.x - PAUSE_GAME_FINISH_MARKER_INNER_OFFSET,
                   point.y + PAUSE_GAME_FINISH_MARKER_MIDDLE_OFFSET,
                   point.x,
                   point.y,
                   PAUSE_GAME_FINISH_MARKER_COLOR);
  draw_window_line(point.x - PAUSE_GAME_FINISH_MARKER_MIDDLE_OFFSET,
                   point.y + PAUSE_GAME_FINISH_MARKER_INNER_OFFSET,
                   point.x + PAUSE_GAME_FINISH_MARKER_INNER_OFFSET,
                   point.y,
                   PAUSE_GAME_FINISH_MARKER_COLOR);
  draw_window_line(point.x - PAUSE_GAME_FINISH_MARKER_OUTER_OFFSET,
                   point.y,
                   point.x + PAUSE_GAME_FINISH_MARKER_MIDDLE_OFFSET,
                   point.y,
                   PAUSE_GAME_FINISH_MARKER_COLOR);
  draw_window_line(point.x - PAUSE_GAME_FINISH_MARKER_MIDDLE_OFFSET,
                   point.y - PAUSE_GAME_FINISH_MARKER_INNER_OFFSET,
                   point.x + PAUSE_GAME_FINISH_MARKER_INNER_OFFSET,
                   point.y,
                   PAUSE_GAME_FINISH_MARKER_COLOR);
  draw_window_line(point.x - PAUSE_GAME_FINISH_MARKER_INNER_OFFSET,
                   point.y - PAUSE_GAME_FINISH_MARKER_MIDDLE_OFFSET,
                   point.x,
                   point.y,
                   PAUSE_GAME_FINISH_MARKER_COLOR);
}

void
render_pause_game_vehicles(const MenuViewport  *viewport_,
                           const MenuRectangle *rectangle_,
                           const MenuScale     *scale_)
{
  const PauseGameOptionSelector *selector;
  PauseGameMapMarker             primary;
  PauseGameMapMarker             secondary;
  int32                          secondary_active;

  selector = (const PauseGameOptionSelector *) scale_;
  _draw_pause_game_finish_marker(viewport_, rectangle_);
  secondary_active =
    (g_STARTUP_CONFIGURATION.opponent_vehicle_index != PAUSE_GAME_DISABLED_SIMULATION_MODE);
  if(secondary_active != 0)
    {
      _prepare_pause_game_map_marker(
        g_VEHICLE_SIMULATION_STATES[PAUSE_GAME_SECONDARY_VEHICLE_INDEX],
        rectangle_,
        selector,
        1,
        &secondary);
      if((secondary.center.x - PAUSE_GAME_MARKER_EDGE_MARGIN < rectangle_->left) ||
         (secondary.center.x + PAUSE_GAME_MARKER_EDGE_MARGIN > rectangle_->right) ||
         (secondary.center.y - PAUSE_GAME_MARKER_EDGE_MARGIN < rectangle_->top) ||
         (secondary.center.y + PAUSE_GAME_MARKER_EDGE_MARGIN > rectangle_->bottom))
        secondary_active = 0;
    }

  _prepare_pause_game_map_marker(g_VEHICLE_SIMULATION_STATES[PAUSE_GAME_PRIMARY_VEHICLE_INDEX],
                                 rectangle_,
                                 selector,
                                 -1,
                                 &primary);
  _fit_pause_game_map_marker(rectangle_, &primary, -1, (secondary_active != 0) ? &secondary : NULL);
  if(secondary_active != 0)
    {
      _fit_pause_game_map_marker(rectangle_, &secondary, 1, &primary);
      set_ccb_pixel_control(g_FRONTEND_INPUT_STATE.reserved_04);
      _draw_pause_game_map_marker(&secondary, PAUSE_GAME_SECONDARY_MARKER_COLOR);
    }
  finalize_allocated_ccb_batch();

  set_ccb_pixel_control(g_FRONTEND_INPUT_STATE.reserved_04);
  _draw_pause_game_map_marker(&primary, PAUSE_GAME_PRIMARY_MARKER_COLOR);
  finalize_allocated_ccb_batch();
}

void
render_pause_game_menu_contents(void)
{
  MenuRectangle *rectangle;
  MenuScale     *scale;
  int32          entry_index;

  update_pause_game_menu_viewport(&g_PAUSE_GAME_MENU_VIEWPORT);
  entry_index = 0;
  while(entry_index < PAUSE_GAME_DISPLAY_ENTRY_COUNT)
    {
      rectangle = &g_PAUSE_GAME_MENU_RECTANGLES[entry_index];
      scale     = (MenuScale *) &g_PAUSE_GAME_OPTION_SELECTORS[entry_index];
      update_menu_scale_coordinates(&g_PAUSE_GAME_MENU_VIEWPORT, scale);
      fit_menu_scale_to_rectangle(rectangle, scale);
      update_menu_rectangle_transform(rectangle, scale);
      if(g_PAUSE_GAME_MENU_GRID_FLAGS[entry_index] != 0)
        {
          draw_menu_rectangle_grid(&g_PAUSE_GAME_MENU_VIEWPORT, rectangle, scale);
        }
      render_pause_game_track_map(&g_PAUSE_GAME_MENU_VIEWPORT,
                                  rectangle,
                                  scale,
                                  PAUSE_GAME_TRACK_MAP_COLOR);
      render_pause_game_vehicles(&g_PAUSE_GAME_MENU_VIEWPORT, rectangle, scale);
      entry_index++;
    }
}

void
render_pause_game_menu_value(int32 value_)
{
  const uint32 *shape;
  char          digit_name[PAUSE_GAME_VALUE_DIGIT_NAME_BYTES] = "    ";
  int32         first_digit;
  int32         second_digit;
  int32         third_digit;
  int32         remainder;

  first_digit  = value_ / PAUSE_GAME_VALUE_DIVISOR_FIRST;
  remainder    = value_ - (first_digit * PAUSE_GAME_VALUE_DIVISOR_FIRST);
  second_digit = remainder / PAUSE_GAME_VALUE_DIVISOR_SECOND;
  third_digit  = (remainder / PAUSE_GAME_VALUE_DIVISOR_THIRD) % PAUSE_GAME_VALUE_DIGIT_RADIX;

  digit_name[PAUSE_GAME_VALUE_DIGIT_FIRST_INDEX] =
    (char) (first_digit + PAUSE_GAME_VALUE_DIGIT_CHARACTER_BIAS);
  shape = (const uint32 *) require_shape(G_PAUSE_GAME_SHAPE, (const uint32 *) digit_name);
  blit_shape_at(shape,
                g_PAUSE_GAME_VALUE_X[PAUSE_GAME_VALUE_DIGIT_FIRST_INDEX],
                g_PAUSE_GAME_VALUE_Y[PAUSE_GAME_VALUE_DIGIT_FIRST_INDEX]);

  digit_name[PAUSE_GAME_VALUE_DIGIT_FIRST_INDEX] =
    (char) (second_digit + PAUSE_GAME_VALUE_DIGIT_CHARACTER_BIAS);
  shape = (const uint32 *) require_shape(G_PAUSE_GAME_SHAPE, (const uint32 *) digit_name);
  blit_shape_at(shape,
                g_PAUSE_GAME_VALUE_X[PAUSE_GAME_VALUE_DIGIT_SECOND_INDEX],
                g_PAUSE_GAME_VALUE_Y[PAUSE_GAME_VALUE_DIGIT_SECOND_INDEX]);

  digit_name[PAUSE_GAME_VALUE_DIGIT_FIRST_INDEX] =
    (char) (third_digit + PAUSE_GAME_VALUE_DIGIT_CHARACTER_BIAS);
  shape = (const uint32 *) require_shape(G_PAUSE_GAME_SHAPE, (const uint32 *) digit_name);
  blit_shape_at(shape,
                g_PAUSE_GAME_VALUE_X[PAUSE_GAME_VALUE_DIGIT_THIRD_INDEX],
                g_PAUSE_GAME_VALUE_Y[PAUSE_GAME_VALUE_DIGIT_THIRD_INDEX]);
}

void
render_pause_game_menu_initial(void)
{
  const uint32 *shape;
  uint32        packed_position;
  int32         value;

  advance_display_buffer();
  advance_display_buffer();
  prepare_pending_video_window();
  finalize_allocated_ccb_batch();
  wait_for_frontend_input();
  wait_for_frontend_input();
  g_PAUSE_GAME_SECONDARY_DISPLAY_DESCRIPTOR =
    &g_DISPLAY_BUFFER_DESCRIPTORS[g_CURRENT_DISPLAY_BUFFER_INDEX];
  g_PAUSE_GAME_DISPLAY_DESCRIPTOR = &g_DISPLAY_BUFFER_DESCRIPTORS[g_PENDING_VIDEO_WINDOW_INDEX];
  set_ccb_pixel_control(PAUSE_GAME_BACKGROUND_PIXEL_CONTROL);
  draw_cel_header_pixels_at_embedded_position(
    (const uint32 *) g_PAUSE_GAME_SECONDARY_DISPLAY_DESCRIPTOR
      ->words[DISPLAY_BUFFER_DESCRIPTOR_BITMAP_HEADER_WORD]);
  set_ccb_pixel_control(PAUSE_GAME_FOREGROUND_PIXEL_CONTROL);
  shape =
    (const uint32 *) require_shape(G_PAUSE_GAME_SHAPE, (const uint32 *) PAUSE_GAME_MENU_SHAPE_NAME);
  packed_position = shape[PAUSE_GAME_SHAPE_PACKED_POSITION_WORD];
  draw_cel_header(shape,
                  (int32) (packed_position >> PAUSE_GAME_SHAPE_X_POSITION_SHIFT),
                  (int32) (int16) packed_position);
  render_pause_game_menu_contents();
  value = DivSF16(g_PAUSE_DISPLAY_PROFILE_VALUES[g_STARTUP_CONFIGURATION.track_index +
                                                 PAUSE_GAME_SECONDARY_PROFILE_OFFSET]
                                                [g_STARTUP_CONFIGURATION.event_index],
                  g_VEHICLE_RENDER_TUNING_SCALES[g_STARTUP_CONFIGURATION.skill_level]
                                                [g_STARTUP_CONFIGURATION.player_vehicle_index]);
  render_pause_game_menu_value(value);
  finalize_allocated_ccb_batch();
}

void
render_pause_game_menu_selection_change(void)
{
  const uint32 *shape;
  uint32        packed_position;
  int32         value;

  prepare_pending_video_window();
  wait_for_frontend_input();
  shape =
    (const uint32 *) require_shape(G_PAUSE_GAME_SHAPE, (const uint32 *) PAUSE_GAME_MENU_SHAPE_NAME);
  packed_position = shape[PAUSE_GAME_SHAPE_PACKED_POSITION_WORD];
  draw_cel_header(shape,
                  (int32) (packed_position >> PAUSE_GAME_SHAPE_X_POSITION_SHIFT),
                  (int32) (int16) packed_position);
  render_pause_game_menu_contents();
  value = DivSF16(g_PAUSE_DISPLAY_PROFILE_VALUES[g_STARTUP_CONFIGURATION.track_index +
                                                 PAUSE_GAME_SECONDARY_PROFILE_OFFSET]
                                                [g_STARTUP_CONFIGURATION.event_index],
                  g_VEHICLE_RENDER_TUNING_SCALES[g_STARTUP_CONFIGURATION.skill_level]
                                                [g_STARTUP_CONFIGURATION.player_vehicle_index]);
  render_pause_game_menu_value(value);
  finalize_allocated_ccb_batch();
  wait_for_frontend_input();
}

void
render_pause_game_menu_selection(int32 selected_option_)
{
  const uint32 *shape;

  store_current_display_buffer_descriptor_snapshot();
  wait_for_frontend_input();
  draw_cel_header_pixels_at_embedded_position(
    (const uint32 *)
      g_PAUSE_GAME_DISPLAY_DESCRIPTOR->words[DISPLAY_BUFFER_DESCRIPTOR_BITMAP_HEADER_WORD]);
  shape =
    (const uint32 *) require_shape(G_PAUSE_GAME_SHAPE, (const uint32 *) PAUSE_GAME_HIGHLIGHT_NAME);
  draw_cel_header(shape,
                  g_PAUSE_GAME_SELECTION_X[selected_option_],
                  g_PAUSE_GAME_SELECTION_Y[selected_option_]);
  finalize_allocated_ccb_batch();
}

int32
run_frontend_menu(void)
{
  uint32 control_input;
  int32  selected_option;

  selected_option                               = 0;
  g_FRONTEND_SAVED_SOUND_EFFECTS                = g_STARTUP_CONFIGURATION.sound_effects_enabled;
  g_STARTUP_CONFIGURATION.sound_effects_enabled = 0;
  stop_all_game_audio_channels();
  reset_menu_option_selector((MenuOptionSelector *) &g_PAUSE_GAME_OPTION_SELECTORS[0]);
  render_pause_game_menu_initial();
  render_pause_game_menu_selection(selected_option);

  control_input = read_frontend_control_input();
  while((control_input & PAUSE_GAME_MENU_CONTROL_MASK) != 0)
    {
      wait_for_frontend_input();
      control_input = read_frontend_control_input();
    }

  for(;;)
    {
      do
        {
          control_input = wait_for_frontend_control_press(PAUSE_GAME_MENU_CONTROL_MASK);
        }
      while(control_input == 0);

      if((control_input & PAUSE_GAME_MENU_NAVIGATE_DOWN) != 0)
        selected_option = (selected_option + 1) % PAUSE_GAME_MENU_OPTION_COUNT;

      if((control_input & PAUSE_GAME_MENU_NAVIGATE_UP) != 0)
        {
          if(selected_option == 0)
            selected_option = PAUSE_GAME_MENU_OPTION_COUNT - 1;
          else
            selected_option--;
        }

      if((control_input & PAUSE_GAME_MENU_CHANGE_OPTION) != 0)
        {
          g_PAUSE_GAME_OPTION_SELECTORS[0].selected_index++;
          if(g_PAUSE_GAME_OPTION_SELECTORS[0].selected_index > PAUSE_GAME_MENU_OPTION_LAST_INDEX)
            g_PAUSE_GAME_OPTION_SELECTORS[0].selected_index = 0;
          g_PAUSE_GAME_OPTION_SELECTORS[0].selected_value =
            g_PAUSE_GAME_OPTION_SELECTORS[0]
              .options[g_PAUSE_GAME_OPTION_SELECTORS[0].selected_index];
          render_pause_game_menu_selection_change();
        }

      if((control_input & PAUSE_GAME_MENU_EXIT_MASK) != 0)
        break;

      render_pause_game_menu_selection(selected_option);
    }

  while((control_input & PAUSE_GAME_MENU_RELEASE_MASK) != 0)
    control_input = read_frontend_control_input();

  g_STARTUP_CONFIGURATION.sound_effects_enabled = g_FRONTEND_SAVED_SOUND_EFFECTS;
  if(selected_option != PAUSE_GAME_MENU_SOUND_OPTION)
    configure_stereo_panning_from_startup();

  return selected_option + 1;
}
