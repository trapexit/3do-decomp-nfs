#pragma once

#include "types.h"

Item
create_runtime_thread(const char *name_,
                      uint8       priority_,
                      void        (*entry_point_)(void),
                      int32       stack_byte_count_);
Err
delete_runtime_thread(Item thread_item_);
