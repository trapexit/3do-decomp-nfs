#pragma once

#include "types.h"

void
clear_byte_range(uint8 *destination_, int32 byte_count_);
void
copy_byte_range_forward(const uint8 *source_, uint8 *destination_, int32 byte_count_);
