#pragma once

#include "types.h"

int32
string_length(const char *string_);
int32
compare_strings_limited(const uint8 *left_, const uint8 *right_, int32 byte_count_);
int32
compare_strings(const uint8 *left_, const uint8 *right_);
int32
compare_strings_ignore_case(const char *left_, const char *right_);
int32
compare_strings_limited_ignore_case(const char *left_, const char *right_, int32 byte_count_);
const uint8 *
skip_copyright_preamble(const uint8 *source_);
char *
copy_string_padded(char *destination_, const char *source_, int32 byte_count_);
char *
copy_fixed_string(char *destination_, const char *source_, int32 byte_count_);
const char *
find_path_leaf(const char *path_);
const char *
get_path_leaf(const char *path_);
int32
compare_fixed_strings(const uint8 *left_, const uint8 *right_, int32 byte_count_);
char *
find_last_character(const char *string_, uint32 character_);
char *
append_string(char *destination_, const char *source_);
char *
copy_string(char *destination_, const char *source_);
char *
append_c_string(char *destination_, const char *source_);
char *
copy_c_string(char *destination_, const char *source_);
char *
find_character(const char *string_, uint32 character_);
const char *
skip_text_token(const char *text_);
void
copy_text_token(char *destination_, const char *text_);
