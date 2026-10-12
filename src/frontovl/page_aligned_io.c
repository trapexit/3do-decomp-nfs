/* Frontovl page-aligned asynchronous I/O request construction. */

#include "page_aligned_io.h"

#include "runtime_io.h"

#include "io.h"
#include "mem.h"

#define PAGE_ALIGNED_IO_COMMAND 5

int32
send_page_aligned_io(Item  io_request_,
                     void *receive_buffer_,
                     void *send_buffer_,
                     int32 page_count_,
                     int32 offset_)
{
  IOInfo info;
  int32  page_mask;
  int32  page_size;

  page_size                = GetPageSize(MEMTYPE_VRAM);
  page_mask                = -page_size;
  info.ioi_Command         = PAGE_ALIGNED_IO_COMMAND;
  info.ioi_Flags           = 0;
  info.ioi_Unit            = 0;
  info.ioi_Flags2          = 0;
  info.ioi_CmdOptions      = 0;
  info.ioi_User            = 0;
  info.ioi_Offset          = offset_;
  info.ioi_Send.iob_Buffer = (void *) ((uint32) send_buffer_ & (uint32) page_mask);
  info.ioi_Send.iob_Len    = (page_size * page_count_);
  info.ioi_Recv.iob_Buffer = (void *) ((uint32) receive_buffer_ & (uint32) page_mask);
  info.ioi_Recv.iob_Len    = (page_size * page_count_);
  return SendIO(io_request_, &info);
}

int32
perform_page_aligned_io_sync(Item  io_request_,
                             void *receive_buffer_,
                             void *send_buffer_,
                             int32 page_count_,
                             int32 offset_)
{
  int32 status;

  status = send_page_aligned_io(io_request_, receive_buffer_, send_buffer_, page_count_, offset_);
  if(status < 0)
    return status;

  return wait_for_io_completion(io_request_);
}
