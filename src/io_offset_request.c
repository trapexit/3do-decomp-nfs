#include "io_offset_request.h"

#include "legacy_io_wait.h"

#include "io.h"

#include "string.h"

#define IO_OFFSET_COMMAND 3

Err
send_io_offset_request(Item io_request_, int32 offset_)
{
  Err result;

  result = send_io_offset_request_async(io_request_, offset_);
  if(result < 0)
    return result;

  result = wait_for_io_completion(io_request_);
  return result;
}

Err
send_io_offset_request_async(Item io_request_, int32 offset_)
{
  IOInfo io_info;

  memset(&io_info, 0, sizeof(io_info));
  io_info.ioi_Command = IO_OFFSET_COMMAND;
  io_info.ioi_Offset  = offset_;
  return SendIO(io_request_, &io_info);
}
