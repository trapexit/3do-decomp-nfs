#pragma once

#include "types.h"

void *
fill_runtime_memory(void *destination_, int32 value_, uint32 byte_count_);
void *
copy_runtime_memory(void *destination_, const void *source_, uint32 byte_count_);
