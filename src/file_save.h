#pragma once

#include "file_block.h"

#include "types.h"

int32
save_file_with_error(const char *path_, const void *source_, uint32 byte_count_);
int32
save_file(const char *path_, const void *source_, uint32 byte_count_);
int32
save_file_data(const char *path_, const void *source_, uint32 byte_count_, int32 report_error_);
int32
save_file_blocks_with_error(const char *path_, const FileBlock *file_blocks_);
int32
save_file_blocks(const char *path_, const FileBlock *file_blocks_);
int32
save_file_blocks_with_error_policy(const char      *path_,
                                   const FileBlock *file_blocks_,
                                   int32            report_error_);
