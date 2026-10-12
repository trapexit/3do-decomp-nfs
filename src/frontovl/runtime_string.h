#pragma once

#include "types.h"

char *
copy_null_terminated_string(char *destination_, const char *source_);
int32
runtime_string_length(const char *text_);
int32
runtime_string_to_integer(const char *text_);
char *
copy_runtime_string_n(char *destination_, const char *source_, uint32 byte_count_);
char *
append_runtime_string(char *destination_, const char *source_);
