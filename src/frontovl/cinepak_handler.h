#pragma once

#include "types.h"

void
initialize_cinepak_color_tables(uint16 *table_);
void
initialize_cinepak_dispatch_table(uint32 *table_);
void *
create_cinepak_handler(void);
void
delete_cinepak_handler(void *handler_);
