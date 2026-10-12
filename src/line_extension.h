#pragma once

#include "geometry.h"

/* Extends both endpoints by one approximate fixed16 unit; outputs may alias inputs. */
void
extend_fixed16_line_segment(const Point2 *start_,
                            const Point2 *end_,
                            Point2       *extended_start_,
                            Point2       *extended_end_);
