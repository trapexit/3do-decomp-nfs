#pragma once

#include "types.h"

void
blit_shape_at(const uint32 *shape_, int32 x_, int32 y_);
void
blit_shape_at_alias(const uint32 *shape_, int32 x_, int32 y_);
void
blit_shape_at_embedded_position(const uint32 *shape_);
void
blit_shape_at_embedded_position_alias(const uint32 *shape_);
void
blit_shape_relative_to_embedded_origin(const uint32 *shape_, int32 x_, int32 y_);
void
blit_shape_relative_to_embedded_origin_alias(const uint32 *shape_, int32 x_, int32 y_);
