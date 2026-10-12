#include "disk_file.h"

#include "file_information.h"
#include "legacy_io_wait.h"

#include "filesystem.h"
#include "io.h"
#include "kernel.h"

#include "string.h"

Item
open_disk_file(char *path_)
{
  return OpenDiskFile(path_);
}

int32
close_disk_file(Item file_item_)
{
  return CloseDiskFile(file_item_);
}

int32
can_open_disk_file(char *path_)
{
  Item file_item;

  file_item = open_disk_file(path_);
  if(file_item < 0)
    return 0;

  close_disk_file(file_item);
  return 1;
}

int32
write_native_file(Item file_item_, const void *source_, int32 byte_count_)
{
  IOInfo io_info;
  Item   io_request;
  int32  result;

  io_request = CreateIOReq(NULL, 0, file_item_, 0);
  if(io_request < 0)
    return io_request;

  memset(&io_info, 0, sizeof(io_info));
  io_info.ioi_Command         = CMD_WRITE;
  io_info.ioi_Send.iob_Buffer = (void *) source_;
  io_info.ioi_Send.iob_Len    = get_native_file_block_count(file_item_, byte_count_);
  result                      = SendIO(io_request, &io_info);
  if(result < 0)
    return result;

  wait_for_io_completion(io_request);
  result = DeleteItem(io_request);
  if(result >= 0)
    return 0;

  return result;
}

int32
create_native_file(char *path_, int32 byte_count_)
{
  IOInfo io_info;
  Item   file_item;
  Item   io_request;
  int32  result;

  result = CreateFile(path_);
  if(result < 0)
    return -1;

  file_item = OpenDiskFile(path_);
  if(file_item < 0)
    return -1;

  io_request = CreateIOReq(NULL, 0, file_item, 0);
  if(io_request < 0)
    return io_request;

  memset(&io_info, 0, sizeof(io_info));
  io_info.ioi_Command = FILECMD_ALLOCBLOCKS;
  io_info.ioi_Offset  = get_native_file_block_count(file_item, byte_count_);
  result              = SendIO(io_request, &io_info);
  if(result < 0)
    return -1;

  wait_for_io_completion(io_request);
  result = DeleteItem(io_request);
  if(result >= 0)
    result = CloseDiskFile(file_item);
  if(result >= 0)
    return 0;

  return result;
}
