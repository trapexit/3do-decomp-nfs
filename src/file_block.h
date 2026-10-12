#pragma once

#include "types.h"

typedef struct FileBlock
{
  void  *data;
  uint8  metadata[0x10];
  uint32 byte_count;
  uint32 auxiliary_value;
} FileBlock;

void *
get_file_block_data(const FileBlock *file_block_);
void *
get_file_block_primary_pointer(const FileBlock *file_block_);
uint32
get_file_block_size(const FileBlock *file_block_);
void *
get_file_block_metadata(FileBlock *file_block_);
uint32
get_file_block_auxiliary_value(const FileBlock *file_block_);
