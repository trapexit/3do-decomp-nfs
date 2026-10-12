#include "legacy_io_wait.h"

#include "graphics.h"
#include "io.h"
#include "mem.h"

#include "string.h"

static uint32
_align_vram_address(uint32 address_, uint32 page_bytes_)
{
  return address_ & (uint32) - (int32) page_bytes_;
}

Err
SetVRAMPages(Item ioreq_, void *destination_, int32 value_, int32 page_count_, int32 mask_)
{
  Err result;

  result = SetVRAMPagesDefer(ioreq_, destination_, value_, page_count_, mask_);
  if(result < 0)
    return result;
  return wait_for_io_completion(ioreq_);
}

Err
SetVRAMPagesDefer(Item ioreq_, void *destination_, int32 value_, int32 page_count_, int32 mask_)
{
  IOInfo io_info;
  uint32 page_bytes;
  uint32 destination;

  page_bytes  = (uint32) GetPageSize(MEMTYPE_VRAM);
  destination = _align_vram_address((uint32) destination_, page_bytes);
  memset(&io_info, 0, sizeof(io_info));
  io_info.ioi_Command         = FLASHWRITE_CMD;
  io_info.ioi_CmdOptions      = (uint32) mask_;
  io_info.ioi_Offset          = value_;
  io_info.ioi_Send.iob_Buffer = (void *) destination;
  io_info.ioi_Recv.iob_Buffer = (void *) destination;
  io_info.ioi_Recv.iob_Len    = (int32) (page_bytes * (uint32) page_count_);
  return SendIO(ioreq_, &io_info);
}

Err
CopyVRAMPages(Item ioreq_, void *destination_, void *source_, uint32 page_count_, uint32 mask_)
{
  Err result;

  result = CopyVRAMPagesDefer(ioreq_, destination_, source_, page_count_, mask_);
  if(result < 0)
    return result;
  return wait_for_io_completion(ioreq_);
}

Err
CopyVRAMPagesDefer(Item ioreq_, void *destination_, void *source_, uint32 page_count_, uint32 mask_)
{
  IOInfo io_info;
  uint32 page_bytes;
  uint32 destination;
  uint32 source;
  int32  byte_count;

  page_bytes  = (uint32) GetPageSize(MEMTYPE_VRAM);
  destination = _align_vram_address((uint32) destination_, page_bytes);
  source      = _align_vram_address((uint32) source_, page_bytes);
  byte_count  = (int32) (page_bytes * page_count_);
  memset(&io_info, 0, sizeof(io_info));
  io_info.ioi_Command         = SPORTCMD_COPY;
  io_info.ioi_CmdOptions      = mask_;
  io_info.ioi_Send.iob_Buffer = (void *) source;
  io_info.ioi_Send.iob_Len    = byte_count;
  io_info.ioi_Recv.iob_Buffer = (void *) destination;
  io_info.ioi_Recv.iob_Len    = byte_count;
  return SendIO(ioreq_, &io_info);
}
