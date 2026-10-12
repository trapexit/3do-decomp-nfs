#pragma once

#include "filesystem.h"

typedef struct DiskFileHandle
{
  Item       item;
  FileStatus status;
} DiskFileHandle;

int32
close_disk_file_handle(DiskFileHandle *file_);
uint32
get_disk_file_byte_count(const DiskFileHandle *file_);
Item
create_disk_file_io_request(Item device_item_, Item reply_port_);
int32
open_disk_file_handle(char *name_, DiskFileHandle *file_);
int32
perform_io_sync(Item io_request_, IOInfo *info_);
int32
read_disk_file_async(DiskFileHandle *file_,
                     Item            io_request_,
                     void           *buffer_,
                     int32           byte_count_,
                     int32           byte_offset_);
int32
wait_for_disk_file_read(Item io_request_);
