#pragma once

#include "filefunctions.h"

Item
open_disk_file(char *path_);
int32
close_disk_file(Item file_item_);
int32
can_open_disk_file(char *path_);
int32
write_native_file(Item file_item_, const void *source_, int32 byte_count_);
int32
create_native_file(char *path_, int32 byte_count_);
