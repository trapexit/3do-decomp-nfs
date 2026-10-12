#pragma once

#include "memory.h"

#define SHAPE_COLOR_TABLE_BYTES 256
#define SHAPE_COLOR_TABLE_TAG_BYTES 4
#define SHAPE_FILE_HEADER_BYTES 16

extern uint8 g_SHAPE_COLOR_TABLE[SHAPE_COLOR_TABLE_BYTES];
extern uint8 g_SHAPE_COLOR_TABLE_TAG[SHAPE_COLOR_TABLE_TAG_BYTES];
extern uint8 g_SHAPE_FILE_HEADER[SHAPE_FILE_HEADER_BYTES];

int32
get_shape_conversion_size(const void *source_);
void
convert_shape_file_data(const void *source_, void *destination_);
MemoryBlockDescriptor *
load_shape_file_record(const char *path_, uint32 memory_flags_, int32 report_error_);
MemoryBlockDescriptor *
load_shape_file_record_alias(const char *path_, uint32 memory_flags_, int32 report_error_);
MemoryBlockDescriptor *
load_shape_file_record_with_error(const char *path_, uint32 memory_flags_);
MemoryBlockDescriptor *
load_shape_file_record_quiet(const char *path_, uint32 memory_flags_);
void *
load_shape_file_data(const char *path_, uint32 memory_flags_, int32 report_error_);
void *
load_shape_file_data_alias(const char *path_, uint32 memory_flags_, int32 report_error_);
void *
load_shape_file_data_with_error_alias(const char *path_, uint32 memory_flags_);
void *
load_shape_file_data_quiet_alias(const char *path_, uint32 memory_flags_);
void *
load_shape_file_data_with_error(const char *path_, uint32 memory_flags_);
void *
load_shape_file_data_quiet(const char *path_, uint32 memory_flags_);
void
load_shape_color_table(const uint8 *source_);
void
remap_bytes_with_table(uint8 *bytes_, const uint8 *table_, int32 byte_count_);
void
resolve_shape_file_directory(void *directory_);
