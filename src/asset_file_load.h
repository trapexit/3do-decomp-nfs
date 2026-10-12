#pragma once

#include "memory.h"

MemoryBlockDescriptor *
load_asset_file_record(const char *path_, uint32 memory_flags_, int32 report_error_);
void *
load_asset_file_with_error_reporting(const char *path_, uint32 memory_flags_);
void *
load_asset_file_without_error_reporting(const char *path_, uint32 memory_flags_);
void *
load_asset_file(const char *path_, uint32 memory_flags_, int32 report_error_);
