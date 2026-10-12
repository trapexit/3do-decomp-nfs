#include "file_save.h"

#include "camera_io.h"
#include "diagnostics.h"
#include "disk_file.h"
#include "memory.h"
#include "string_utilities.h"

#include "filefunctions.h"
#include "io.h"
#include "operror.h"

#define FILE_SAVE_NVRAM_PREFIX_BYTES 6
#define FILE_SAVE_NATIVE_ALIGNMENT_PADDING 3U
#define FILE_SAVE_NATIVE_ALIGN_MASK 0xFFFFFFFCU
#define FILE_SAVE_BLOCK_COUNT 2

int32
save_file_with_error(const char *path_, const void *source_, uint32 byte_count_)
{
  return save_file_data(path_, source_, byte_count_, 1);
}

int32
save_file(const char *path_, const void *source_, uint32 byte_count_)
{
  return save_file_data(path_, source_, byte_count_, 0);
}

int32
save_file_data(const char *path_, const void *source_, uint32 byte_count_, int32 report_error_)
{
  FileBlock file_blocks[FILE_SAVE_BLOCK_COUNT];

  file_blocks[0].data       = (void *) source_;
  file_blocks[0].byte_count = byte_count_;
  file_blocks[1].data       = NULL;
  return save_file_blocks_with_error_policy(path_, file_blocks, report_error_);
}

int32
save_file_blocks_with_error(const char *path_, const FileBlock *file_blocks_)
{
  return save_file_blocks_with_error_policy(path_, file_blocks_, 1);
}

int32
save_file_blocks(const char *path_, const FileBlock *file_blocks_)
{
  return save_file_blocks_with_error_policy(path_, file_blocks_, 0);
}

static int32
_save_camera_file(const char *path_, const FileBlock *file_blocks_, int32 report_error_)
{
  IOInfo io_info;
  int32  result;

  if(g_CAMERA_IO_STATE.io_request == 0)
    {
      if(report_error_ != 0)
        fatal_error("savefileblocka - INITFILE HAS NOT BEEN CALLED\n");
      return 0;
    }

  clear_bytes((uint8 *) &io_info, sizeof(io_info));
  io_info.ioi_Command         = CMD_WRITE;
  io_info.ioi_Unit            = 0;
  io_info.ioi_Offset          = 0;
  io_info.ioi_Send.iob_Buffer = get_file_block_data(file_blocks_);
  io_info.ioi_Send.iob_Len =
    (int32) (((uint32) get_file_block_size(file_blocks_) + FILE_SAVE_NATIVE_ALIGNMENT_PADDING) &
             FILE_SAVE_NATIVE_ALIGN_MASK);
  io_info.ioi_Recv.iob_Buffer = (void *) path_;
  io_info.ioi_Recv.iob_Len    = string_length(path_) + 1;
  result                      = DoIO(g_CAMERA_IO_STATE.io_request, &io_info);
  if(result >= 0)
    return 1;

  if(report_error_ != 0)
    {
      PrintfSysErr(result);
      fatal_error("savefile - ERROR WRITING CAM FILE %s", path_);
    }
  return 0;
}

static int32
_save_native_file(const char *path_, const FileBlock *file_blocks_, int32 report_error_)
{
  const void *source;
  Item        file_item;
  int32       byte_count;
  int32       result;

  byte_count = (int32) get_file_block_size(file_blocks_);
  source     = get_file_block_data(file_blocks_);
  if(can_open_disk_file((char *) path_) == 1)
    DeleteFile((char *) path_);

  result = create_native_file((char *) path_, byte_count);
  if(result < 0)
    {
      if(report_error_ != 0)
        {
          PrintfSysErr(result);
          fatal_error("savefile - ERROR CREATING NVRAM FILE %s\n", path_);
        }
      return 0;
    }

  file_item = open_disk_file((char *) path_);
  if(file_item < 0)
    {
      if(report_error_ != 0)
        {
          PrintfSysErr(result);
          fatal_error("savefile - ERROR OPENING NVRAM FILE %s\n", path_);
        }
      return 0;
    }

  result = write_native_file(file_item, source, byte_count);
  if(result >= 0)
    {
      close_disk_file(file_item);
      return 1;
    }

  close_disk_file(file_item);
  DeleteFile((char *) path_);
  if(report_error_ != 0)
    {
      PrintfSysErr(result);
      fatal_error("savefile - ERROR WRITING NVRAM FILE %s\n", path_);
    }
  return 0;
}

int32
save_file_blocks_with_error_policy(const char      *path_,
                                   const FileBlock *file_blocks_,
                                   int32            report_error_)
{
  if((compare_strings_limited((const uint8 *) path_,
                              (const uint8 *) "/nvram",
                              FILE_SAVE_NVRAM_PREFIX_BYTES) == 0) ||
     (compare_strings_limited((const uint8 *) path_,
                              (const uint8 *) "/NVRAM",
                              FILE_SAVE_NVRAM_PREFIX_BYTES) == 0))
    {
      return _save_native_file(path_, file_blocks_, report_error_);
    }

  return _save_camera_file(path_, file_blocks_, report_error_);
}
