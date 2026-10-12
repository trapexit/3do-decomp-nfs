#pragma once

#include "types.h"

void *
locate_shape(const void *collection_, const uint32 *shape_name_);
void *
require_shape(const void *collection_, const uint32 *shape_name_);
void *
locate_shape_entry(const void *collection_, const uint32 *shape_name_, const char *error_format_);
