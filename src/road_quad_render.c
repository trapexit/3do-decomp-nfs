/* Maps ordinary and extended road quads into the render CCB chain. */

#include "line_extension.h"
#include "quad_mapping.h"
#include "road_render.h"

#define ROAD_QUAD_CCB_NEXT_OFFSET 0x38

enum
{
  ROAD_QUAD_TOP_LEFT_INDEX     = 0,
  ROAD_QUAD_TOP_RIGHT_INDEX    = 1,
  ROAD_QUAD_BOTTOM_RIGHT_INDEX = 2,
  ROAD_QUAD_BOTTOM_LEFT_INDEX  = 3,
  ROAD_QUAD_EXTENSION_TYPE_MIN = 5,
  ROAD_QUAD_EXTENSION_TYPE_MAX = 6
};

void
render_road_quad_corners(const RoadCelTemplate *source_ccb_,
                         const Point2          *top_left_,
                         const Point2          *top_right_,
                         const Point2          *bottom_right_,
                         const Point2          *bottom_left_)
{
  RenderCcb *ccb;
  Point2     quad[ROAD_RENDER_REFERENCE_POINT_COUNT];

  ccb                                = allocate_partial_copy_ccb((const RenderCcb *) source_ccb_);
  ccb->fields.next_offset            = ROAD_QUAD_CCB_NEXT_OFFSET;
  quad[ROAD_QUAD_TOP_LEFT_INDEX]     = *top_left_;
  quad[ROAD_QUAD_TOP_RIGHT_INDEX]    = *top_right_;
  quad[ROAD_QUAD_BOTTOM_RIGHT_INDEX] = *bottom_right_;
  quad[ROAD_QUAD_BOTTOM_LEFT_INDEX]  = *bottom_left_;
  map_fixed16_quad_to_ccb(&ccb->fields, quad, source_ccb_->width, source_ccb_->height);
}

void
render_extended_road_quad(const RoadCelTemplate *source_ccb_, Point2 *quad_, int32 render_type_)
{
  RenderCcb *ccb;

  ccb                     = allocate_partial_copy_ccb((const RenderCcb *) source_ccb_);
  ccb->fields.next_offset = ROAD_QUAD_CCB_NEXT_OFFSET;
  if((render_type_ > ROAD_QUAD_EXTENSION_TYPE_MAX) ||
     ((render_type_ > ROAD_QUAD_TOP_RIGHT_INDEX) && (render_type_ < ROAD_QUAD_EXTENSION_TYPE_MIN)))
    {
      extend_fixed16_line_segment(&quad_[ROAD_QUAD_TOP_LEFT_INDEX],
                                  &quad_[ROAD_QUAD_BOTTOM_RIGHT_INDEX],
                                  &quad_[ROAD_QUAD_TOP_LEFT_INDEX],
                                  &quad_[ROAD_QUAD_BOTTOM_RIGHT_INDEX]);
      extend_fixed16_line_segment(&quad_[ROAD_QUAD_TOP_RIGHT_INDEX],
                                  &quad_[ROAD_QUAD_BOTTOM_LEFT_INDEX],
                                  &quad_[ROAD_QUAD_TOP_RIGHT_INDEX],
                                  &quad_[ROAD_QUAD_BOTTOM_LEFT_INDEX]);
    }

  map_fixed16_quad_to_ccb(&ccb->fields, quad_, source_ccb_->width, source_ccb_->height);
}
