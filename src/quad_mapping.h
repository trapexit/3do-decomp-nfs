#pragma once

#include "ccb_manager.h"
#include "geometry.h"

/* Maps four fixed16 corners using a cel's base-two width and height shifts. */
void
map_fixed16_quad_to_ccb(CcbTemplate  *ccb_,
                        const Point2 *quad_,
                        int32         width_shift_,
                        int32         height_shift_);
