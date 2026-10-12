#pragma once

#include "kernel.h"
#include "types.h"

Err
send_io_offset_request(Item io_request_, int32 offset_);
Err
send_io_offset_request_async(Item io_request_, int32 offset_);
