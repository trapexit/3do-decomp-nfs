/* Frontovl standard-stream buffering and Macintosh host-device output. */

#include "disk_file.h"
#include "find_kernel_item.h"
#include "kernel_item.h"
#include "runtime_memory_buffer.h"
#include "runtime_memory_ops.h"
#include "runtime_string.h"

#include "debug.h"
#include "io.h"
#include "item.h"

#include "stdio.h"

/* Private standard-library provider entry points. */
int32
InitMacIO(void);
int32
internalfwrite(const void *buffer_, int32 item_size_, int32 item_count_, FILE *stream_);

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

enum
{
  MAC_IO_DEVICE_TAG_INDEX = 0,
  MAC_IO_END_TAG_INDEX    = 1,
  MAC_IO_TAG_COUNT        = 2
};

Item   _stdlib_MacItem   = 0;
Item   _stdlib_MacIORItm = 0;
IOReq *_stdlib_MacIOReq  = NULL;

FILE _stdin;
FILE _stdout;
FILE _stderr;

FILE *stdin  = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;

TagArg g_MAC_IO_REQUEST_TAGS[MAC_IO_TAG_COUNT] = { { CREATEIOREQ_TAG_DEVICE, NULL },
                                                   { TAG_END, NULL } };

COMPILE_TIME_SIZE_ASSERT(mac_io_request_tags_size, sizeof(g_MAC_IO_REQUEST_TAGS) == 16);
COMPILE_TIME_SIZE_ASSERT(file_control_block_size, sizeof(FILE) == 0xA0);

int32
InitMacIO(void)
{
  Item device_item;

  if(_stdlib_MacIOReq != NULL)
    {
      return 1;
    }

  device_item     = find_named_kernel_item(MKNODEID(KERNELNODE, DEVICENODE), "mac");
  _stdlib_MacItem = OpenItem(device_item, NULL);
  if(_stdlib_MacItem < 0)
    {
      return 0;
    }

  g_MAC_IO_REQUEST_TAGS[MAC_IO_DEVICE_TAG_INDEX].ta_Arg = (void *) _stdlib_MacItem;
  _stdlib_MacIORItm = CreateSizedItem(MKNODEID(KERNELNODE, IOREQNODE), g_MAC_IO_REQUEST_TAGS, 0);
  if(_stdlib_MacIORItm < 0)
    {
      return 0;
    }

  _stdlib_MacIOReq = (IOReq *) lookup_kernel_item(_stdlib_MacIORItm);
  if(AllocBuffer((RuntimeMemoryBuffer *) stdin) == NULL)
    {
      return 0;
    }
  stdin->fcb_mode = FCB_READ_MODE;

  if(AllocBuffer((RuntimeMemoryBuffer *) stdout) == NULL)
    {
      return 0;
    }
  stdout->fcb_mode     = FCB_WRITE_MODE;
  stdout->fcb_numinbuf = stdout->fcb_buffsize;

  if(AllocBuffer((RuntimeMemoryBuffer *) stderr) == NULL)
    {
      return 0;
    }
  stderr->fcb_mode     = FCB_WRITE_MODE;
  stderr->fcb_numinbuf = stderr->fcb_buffsize;

  return 1;
}

int32
internalfwrite(const void *buffer_, int32 item_size_, int32 item_count_, FILE *stream_)
{
  IOInfo info;
  uint32 byte_count;

  byte_count = (uint32) item_size_ * (uint32) item_count_;
  if(byte_count == 0)
    {
      return 0;
    }
  if(stream_ == NULL)
    {
      kprintf("Stream = 0 in %s!!!\n", "internalfwrite");
      return EOF;
    }
  if((_stdlib_MacIOReq == NULL) && !InitMacIO())
    {
      return EOF;
    }

  fill_runtime_memory(&info, 0, sizeof(info));
  info.ioi_Command         = CMD_WRITE;
  info.ioi_Offset          = stream_->fcb_currentpos;
  info.ioi_Send.iob_Buffer = (void *) buffer_;
  info.ioi_Send.iob_Len    = (int32) byte_count;
  info.ioi_Recv.iob_Buffer = stream_->fcb_filename;
  info.ioi_Recv.iob_Len    = runtime_string_length(stream_->fcb_filename) + 1;
  perform_io_sync(_stdlib_MacIORItm, &info);

  if(_stdlib_MacIOReq->io_Error != 0)
    {
      kprintf("error during fwrite=%d of %s\n", _stdlib_MacIOReq->io_Error, stream_->fcb_filename);
      return 0;
    }

  stream_->fcb_currentpos = (int32) ((uint32) stream_->fcb_currentpos + byte_count);
  if(stream_->fcb_filesize < stream_->fcb_currentpos)
    {
      stream_->fcb_filesize = stream_->fcb_currentpos;
    }

  return item_count_;
}

int32
putc(char character_, FILE *stream_)
{
  uint8 value;

  value = (uint8) character_;
  if((stream_ == stdout) || (stream_ == stderr))
    {
      kprintf("%c", value);
      return value;
    }

  --stream_->fcb_numinbuf;
  if(stream_->fcb_numinbuf < 0)
    {
      fflush(stream_);
    }
  *stream_->fcb_cp++ = value;
  return value;
}

int32
fflush(FILE *stream_)
{
  int32 byte_count;
  int32 written_item_count;

  if((stream_ == stdin) || (stream_ == stdout) || (stream_ == stderr))
    {
      return 0;
    }

  byte_count = stream_->fcb_buffsize - stream_->fcb_numinbuf;
  if(byte_count > stream_->fcb_buffsize)
    {
      byte_count = stream_->fcb_buffsize;
    }
  stream_->fcb_numinbuf += byte_count;

  if((byte_count > 0) && ((stream_->fcb_mode & FCB_WRITE_MODE) != 0))
    {
      written_item_count = internalfwrite(stream_->fcb_buffer, 1, byte_count, stream_);
      stream_->fcb_cp    = stream_->fcb_buffer;
      if(written_item_count != byte_count)
        {
          return EOF;
        }
    }

  return 0;
}
