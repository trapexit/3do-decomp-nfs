#pragma once

#include "types.h"

#include "stdarg.h"

int32
floating_point_display_disabled(void);
int32
sprintf(char *buffer_, const char *format_, ...);
int32
vsprintf(char *buffer_, const char *format_, va_list arguments_);
void
print_formatted_kernel_message(const char *format_, ...);
