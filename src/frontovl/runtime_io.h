#pragma once

#include "types.h"

int32
check_runtime_io_completion(Item request_);
int32
wait_for_io_completion(Item request_);
