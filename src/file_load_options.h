#pragma once

#include "memory.h"

void *
load_file_with_options(const char *path_, int32 options_);
void *
load_file_with_options_with_error(const char *path_, int32 options_);
MemoryBlockDescriptor *
load_file_record_with_options(const char *path_, int32 options_, int32 report_error_);
