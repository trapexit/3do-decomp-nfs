#pragma once

#include "types.h"

extern Item g_VRAM_IO_REQUEST;

void
initialize_vram_transfer(void);
Err
copy_vram_bytes_deferred(void *destination_, const void *source_, int32 byte_count_);
Err
clear_vram_bytes(void *destination_, int32 byte_count_);
Err
set_vram_bytes(void *destination_, int32 byte_count_, int32 value_);
Err
wait_for_vram_transfer(void);
Err
copy_display_bitmap_deferred(uint32 *destination_header_);
