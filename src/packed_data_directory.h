#pragma once

#include "types.h"

uint32
get_packed_directory_entry_count(const void *directory_);
const uint8 *
get_packed_directory_entry(const void *directory_, uint32 index_);
void
write_packed_directory_entry_tag(const void *directory_, uint32 index_, uint8 *destination_);
