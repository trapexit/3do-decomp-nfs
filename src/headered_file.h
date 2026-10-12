#pragma once

#include "types.h"

void *
resolve_headered_file_offsets(void *file_data_, int32 selector_);
void *
load_headered_file_with_default_options(const char *path_);
void *
load_headered_resource_file(const char *path_, void *resource_);
