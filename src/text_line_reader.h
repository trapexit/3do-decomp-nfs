#pragma once

#include "types.h"

char *
read_text_line(char *destination_, int32 byte_count_, void *stream_);
char *
read_noncomment_text_line(char *destination_, int32 byte_count_, void *stream_);
int32
read_second_fixed16_text_line(void *stream_);
int32
read_noncomment_fixed16_text_line(void *stream_);
int32
read_noncomment_integer_text_line(void *stream_);
