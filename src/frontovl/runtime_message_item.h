#pragma once

#include "types.h"

Item
create_runtime_message_port(const char *name_, uint8 priority_, int32 signal_);
Item
create_runtime_message(const char *name_, uint8 priority_, Item reply_port_);
