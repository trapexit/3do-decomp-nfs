#include "file_information.h"

#include "camera_io.h"
#include "diagnostics.h"
#include "legacy_io_wait.h"
#include "string_utilities.h"

#include "directory.h"
#include "directoryfunctions.h"
#include "filefunctions.h"
#include "filestreamfunctions.h"
#include "io.h"
#include "kernel.h"

#include "string.h"

#define FILE_INFORMATION_PATH_BYTES 256
#define FILE_INFORMATION_NVRAM_PREFIX_BYTES 6
#define FILE_INFORMATION_NVRAM_LEAF_OFFSET 7
#define FILE_INFORMATION_SLOT_TYPE_STREAM 1
#define FILE_INFORMATION_SLOT_TYPE_DISK_FILE 2
#define NATIVE_FILE_ALIGNMENT_PADDING 3U
#define NATIVE_FILE_ALIGNMENT_MASK 0xFFFFFFFCU

FileInformationState g_FILE_INFORMATION_STATE;

static FileInformationSlot *
_find_available_file_information_slot(void)
{
  int32 slot_index;

  slot_index = 1;
  while(g_FILE_INFORMATION_STATE.slots[slot_index].type != 0)
    slot_index++;

  return &g_FILE_INFORMATION_STATE.slots[slot_index];
}

static int32
_get_file_information_handle(const FileInformationSlot *slot_)
{
  return (int32) (slot_ - g_FILE_INFORMATION_STATE.slots);
}

static void
_report_file_open_error(const char *path_, int32 report_error_)
{
  if(report_error_ != 0)
    fatal_error("open handle - %s FILE ERROR\n", path_);
}

static int32
_get_native_file_block_size(Item file_item_, int32 *block_size_)
{
  FileStatus file_status;
  IOInfo     io_info;
  Item       io_request;
  int32      result;

  *block_size_ = 0;
  io_request   = CreateIOReq(NULL, 0, file_item_, 0);
  if(io_request < 0)
    return io_request;

  memset(&io_info, 0, sizeof(io_info));
  io_info.ioi_Command         = CMD_STATUS;
  io_info.ioi_Recv.iob_Buffer = &file_status;
  io_info.ioi_Recv.iob_Len    = sizeof(file_status);
  result                      = SendIO(io_request, &io_info);
  if(result >= 0)
    {
      wait_for_io_completion(io_request);
      result = DeleteItem(io_request);
      if(result >= 0)
        {
          *block_size_ = file_status.fs.ds_DeviceBlockSize;
          return 0;
        }
    }

  return result;
}

int32
get_native_file_block_count(Item file_item_, int32 byte_count_)
{
  int32 block_size;
  int32 rounded_byte_count;

  _get_native_file_block_size(file_item_, &block_size);
  rounded_byte_count =
    (int32) (((uint32) byte_count_ + NATIVE_FILE_ALIGNMENT_PADDING) & NATIVE_FILE_ALIGNMENT_MASK);
  return (rounded_byte_count + block_size - 1) / block_size;
}

int32
read_native_file(Item file_item_, void *buffer_, int32 byte_count_)
{
  IOInfo io_info;
  Item   io_request;
  int32  result;

  io_request = CreateIOReq(NULL, 0, file_item_, 0);
  if(io_request < 0)
    return io_request;

  memset(&io_info, 0, sizeof(io_info));
  io_info.ioi_Command         = CMD_READ;
  io_info.ioi_Recv.iob_Buffer = buffer_;
  io_info.ioi_Recv.iob_Len    = get_native_file_block_count(file_item_, byte_count_);
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
delete_native_file(char *path_)
{
  int32 result;

  result = DeleteFile(path_);
  if(result >= 0)
    return 0;
  return result;
}

void
initialize_file_information(void)
{
  char current_directory[FILE_INFORMATION_PATH_BYTES];

  if(g_FILE_INFORMATION_STATE.initialized != 0)
    return;

  /* Preserve an inherited directory; use the boot volume only when unset. */
  current_directory[0] = '\0';
  GetDirectory(current_directory, sizeof(current_directory));
  if((current_directory[0] == '\0') && (ChangeDirectory("$boot") < 0))
    print_debug_message("initfile - WARNING CANNOT SET INITIAL WORKING DIRECTORY\n");

  if(initialize_camera_io() == 0)
    print_debug_message("initfile - WARNING CANNOT COMMUNICATE WITH MAC\n");

  g_FILE_INFORMATION_STATE.initialized = 1;
}

void
open_file_information(const char *path_,
                      int32      *handle_,
                      int32      *file_offset_,
                      int32      *size_,
                      int32       report_error_)
{
  Directory           *directory;
  DirectoryEntry       directory_entry;
  Item                 file_item;
  FileInformationSlot *slot;
  Stream              *stream;
  char                 path[FILE_INFORMATION_PATH_BYTES];
  int32                directory_result;

  initialize_file_information();
  *handle_      = 0;
  *file_offset_ = 0;
  *size_        = 0;
  copy_string(path, path_);

  if((compare_strings_limited((const uint8 *) path_,
                              (const uint8 *) "/nvram",
                              FILE_INFORMATION_NVRAM_PREFIX_BYTES) != 0) &&
     (compare_strings_limited((const uint8 *) path_,
                              (const uint8 *) "/NVRAM",
                              FILE_INFORMATION_NVRAM_PREFIX_BYTES) != 0))
    {
      stream = OpenDiskStream(path, 0);
      if(stream == NULL)
        {
          _report_file_open_error(path_, report_error_);
          return;
        }

      slot         = _find_available_file_information_slot();
      slot->type   = FILE_INFORMATION_SLOT_TYPE_STREAM;
      slot->stream = stream;
      *handle_     = _get_file_information_handle(slot);
      *size_       = SeekDiskStream(stream, 0, SEEK_END);
      SeekDiskStream(stream, 0, SEEK_SET);
      ReadDiskStream(stream, path, 0);
      return;
    }

  file_item = OpenDiskFile((char *) path_);
  if(file_item < 0)
    {
      _report_file_open_error(path_, report_error_);
      return;
    }

  slot             = _find_available_file_information_slot();
  slot->file_item  = file_item;
  slot->type       = FILE_INFORMATION_SLOT_TYPE_DISK_FILE;
  *handle_         = _get_file_information_handle(slot);
  directory        = OpenDirectoryPath("/nvram");
  directory_result = ReadDirectory(directory, &directory_entry);
  while((directory_result >= 0) &&
        (compare_strings((const uint8 *) directory_entry.de_FileName,
                         (const uint8 *) path_ + FILE_INFORMATION_NVRAM_LEAF_OFFSET) != 0))
    {
      directory_result = ReadDirectory(directory, &directory_entry);
    }

  if(directory_result < 0)
    {
      _report_file_open_error(path_, report_error_);
      return;
    }

  CloseDirectory(directory);
  *size_ = directory_entry.de_ByteCount;
}

void
open_file_information_with_error(const char *path_,
                                 int32      *handle_,
                                 int32      *file_offset_,
                                 int32      *size_)
{
  open_file_information(path_, handle_, file_offset_, size_, 1);
}

void
open_file_information_default(const char *path_, int32 *handle_, int32 *file_offset_, int32 *size_)
{
  open_file_information(path_, handle_, file_offset_, size_, 0);
}

void
close_file_information(int32 handle_)
{
  FileInformationSlot *slot;

  slot = &g_FILE_INFORMATION_STATE.slots[handle_];
  if(slot->type == 0)
    return;

  if(slot->type == FILE_INFORMATION_SLOT_TYPE_STREAM)
    CloseDiskStream(slot->stream);
  else
    CloseDiskFile(slot->file_item);

  slot->type = 0;
}

int32
seek_file_information(int32 handle_, int32 offset_)
{
  FileInformationSlot *slot;

  slot = &g_FILE_INFORMATION_STATE.slots[handle_];
  if(slot->type != FILE_INFORMATION_SLOT_TYPE_STREAM)
    return 0;

  return SeekDiskStream(slot->stream, offset_, SEEK_SET);
}

int32
read_file_information(int32 handle_, void *buffer_, int32 byte_count_)
{
  uint8               *buffer;
  int32                bytes_read;
  FileInformationSlot *slot;

  slot = &g_FILE_INFORMATION_STATE.slots[handle_];
  if(slot->type == 0)
    return 0;

  buffer = (uint8 *) buffer_;
  while(byte_count_ != 0)
    {
      if(slot->type == FILE_INFORMATION_SLOT_TYPE_STREAM)
        bytes_read = ReadDiskStream(slot->stream, (char *) buffer, byte_count_);
      else
        {
          read_native_file(slot->file_item, buffer, byte_count_);
          bytes_read = byte_count_;
        }

      if(bytes_read <= 0)
        return 0;

      buffer += bytes_read;
      byte_count_ -= bytes_read;
    }

  return 1;
}

int32
file_exists(const char *path_)
{
  int32 file_offset;
  int32 handle;
  int32 size;

  open_file_information_default(path_, &handle, &file_offset, &size);
  close_file_information(handle);
  if(handle != 0)
    return 1;

  return 0;
}
