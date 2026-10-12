#pragma once

#include "types.h"

void *
allocate_dsp_memory(uint32 byte_count_);
void
free_dsp_memory(void *memory_);
