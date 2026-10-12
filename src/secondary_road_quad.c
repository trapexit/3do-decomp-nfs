/* Copies a road cel and maps it onto four projected screen points. */

#include "quad_mapping.h"
#include "road_render.h"

#define SECONDARY_ROAD_CCB_NEXT_OFFSET 0x38

typedef char SecondaryRoadCelTemplateSize[(sizeof(RoadCelTemplate) == 0x44) ? 1 : -1];

void
render_secondary_road_quad(const RoadCelTemplate *source_ccb_, const Point2 *quad_)
{
  RenderCcb *ccb;

  ccb                     = allocate_partial_copy_ccb((const RenderCcb *) source_ccb_);
  ccb->fields.next_offset = SECONDARY_ROAD_CCB_NEXT_OFFSET;
  ccb->fields.preamble_0  = source_ccb_->fields.preamble_0;
  ccb->fields.preamble_1  = source_ccb_->fields.preamble_1;
  map_fixed16_quad_to_ccb(&ccb->fields, quad_, source_ccb_->width, source_ccb_->height);
}
