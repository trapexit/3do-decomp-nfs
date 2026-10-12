#pragma once

#include "types.h"

int32
read_nvram_directory(void);
int32
delete_nvram_entry(int32 entry_index_);
void
draw_nvram_browser(int32 entry_count_, int32 selected_index_, int32 first_visible_index_);
int32
run_nvram_recovery_browser(void);
