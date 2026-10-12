#include "block_file_status.h"

#include "runtime_io.h"

uint8
is_block_file_read_done(Item request_item_)
{
  return (uint8) check_runtime_io_completion(request_item_);
}
