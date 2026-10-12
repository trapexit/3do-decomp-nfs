#pragma once

#include "types.h"

Item
find_named_item(int32 item_type_, const char *name_);
Item
find_versioned_item(int32 item_type_, const char *name_, uint8 version_, uint8 revision_);
