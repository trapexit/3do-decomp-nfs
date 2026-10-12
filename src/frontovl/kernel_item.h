#pragma once

#include "types.h"

void *
lookup_kernel_item(Item item_);
void *
check_kernel_item(Item item_, uint8 folio_type_, uint8 node_type_);
int32
get_kernel_page_size(uint32 type_bits_);
Err
is_kernel_item_opened(Item task_, Item item_);
