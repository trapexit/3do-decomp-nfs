#include "file_block.h"

void *
get_file_block_data(const FileBlock *file_block_)
{
  return file_block_->data;
}

void *
get_file_block_primary_pointer(const FileBlock *file_block_)
{
  return file_block_->data;
}

uint32
get_file_block_size(const FileBlock *file_block_)
{
  return file_block_->byte_count;
}

void *
get_file_block_metadata(FileBlock *file_block_)
{
  return file_block_->metadata;
}

uint32
get_file_block_auxiliary_value(const FileBlock *file_block_)
{
  return file_block_->auxiliary_value;
}
