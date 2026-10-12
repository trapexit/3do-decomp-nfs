#pragma once

#include "types.h"

void *
load_file_into_buffer_reported(const char *path_, void *buffer_, int32 report_error_);
void *
load_file_into_buffer_with_error(const char *path_, void *buffer_);
void *
load_file_into_buffer_quiet(const char *path_, void *buffer_);
