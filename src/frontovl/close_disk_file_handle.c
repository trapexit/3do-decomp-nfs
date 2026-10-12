#include "disk_file.h"
#include "runtime_io.h"

#include "filefunctions.h"

enum
{
  IO_REQUEST_DEVICE_TAG_INDEX     = 0,
  IO_REQUEST_REPLY_PORT_TAG_INDEX = 1,
  IO_REQUEST_END_TAG_INDEX        = 2,
  IO_REQUEST_TAG_COUNT            = 3
};

int32
close_disk_file_handle(DiskFileHandle *file_)
{
  return CloseDiskFile(file_->item);
}

uint32
get_disk_file_byte_count(const DiskFileHandle *file_)
{
  return file_->status.fs_ByteCount;
}

Item
create_disk_file_io_request(Item device_item_, Item reply_port_)
{
  TagArg tags[IO_REQUEST_TAG_COUNT];

  tags[IO_REQUEST_DEVICE_TAG_INDEX].ta_Tag     = CREATEIOREQ_TAG_DEVICE;
  tags[IO_REQUEST_DEVICE_TAG_INDEX].ta_Arg     = (void *) device_item_;
  tags[IO_REQUEST_REPLY_PORT_TAG_INDEX].ta_Tag = CREATEIOREQ_TAG_REPLYPORT;
  tags[IO_REQUEST_REPLY_PORT_TAG_INDEX].ta_Arg = (void *) reply_port_;
  tags[IO_REQUEST_END_TAG_INDEX].ta_Tag        = TAG_END;

  return CreateSizedItem(MKNODEID(KERNELNODE, IOREQNODE), tags, 0);
}

int32
open_disk_file_handle(char *name_, DiskFileHandle *file_)
{
  IOInfo info;
  Item   io_request;
  int32  result;

  io_request  = 0;
  file_->item = OpenDiskFile(name_);
  if(file_->item < 0)
    {
      result = file_->item;
    }
  else
    {
      io_request = create_disk_file_io_request(file_->item, 0);
      if(io_request < 0)
        {
          result = io_request;
        }
      else
        {
          info.ioi_Command         = CMD_STATUS;
          info.ioi_Unit            = 0;
          info.ioi_Flags           = IO_QUICK;
          info.ioi_Flags2          = 0;
          info.ioi_CmdOptions      = 0;
          info.ioi_Offset          = 0;
          info.ioi_Send.iob_Buffer = NULL;
          info.ioi_Send.iob_Len    = 0;
          info.ioi_Recv.iob_Buffer = &file_->status;
          info.ioi_Recv.iob_Len    = sizeof(file_->status);
          result                   = perform_io_sync(io_request, &info);
        }
    }

  if(io_request > 0)
    {
      DeleteItem(io_request);
    }
  if(result < 0)
    {
      close_disk_file_handle(file_);
    }

  return result;
}

int32
perform_io_sync(Item io_request_, IOInfo *info_)
{
  int32 result;

  info_->ioi_Flags |= IO_QUICK;
  result = SendIO(io_request_, info_);
  if(result >= 0)
    {
      result = wait_for_io_completion(io_request_);
    }

  return result;
}

int32
read_disk_file_async(DiskFileHandle *file_,
                     Item            io_request_,
                     void           *buffer_,
                     int32           byte_count_,
                     int32           byte_offset_)
{
  IOInfo info;

  info.ioi_Command         = CMD_READ;
  info.ioi_Unit            = 0;
  info.ioi_Flags           = 0;
  info.ioi_Flags2          = 0;
  info.ioi_CmdOptions      = 0;
  info.ioi_Offset          = byte_offset_ / file_->status.fs.ds_DeviceBlockSize;
  info.ioi_Send.iob_Buffer = NULL;
  info.ioi_Send.iob_Len    = 0;
  info.ioi_Recv.iob_Buffer = buffer_;
  info.ioi_Recv.iob_Len    = byte_count_;

  return SendIO(io_request_, &info);
}

int32
wait_for_disk_file_read(Item io_request_)
{
  return wait_for_io_completion(io_request_);
}
