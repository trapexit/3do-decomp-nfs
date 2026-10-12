#pragma once

#include "mem.h"

void *
allocate_memory_from_lists(List *lists_, int32 byte_count_, uint32 type_bits_);
