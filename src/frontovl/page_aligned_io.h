#pragma once

#include "types.h"

int32
send_page_aligned_io(Item  io_request_,
                     void *receive_buffer_,
                     void *send_buffer_,
                     int32 page_count_,
                     int32 offset_);
int32
perform_page_aligned_io_sync(Item  io_request_,
                             void *receive_buffer_,
                             void *send_buffer_,
                             int32 page_count_,
                             int32 offset_);
