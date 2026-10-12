#pragma once

#include "types.h"

#define DIAGNOSTIC_BUFFER_BYTES 256

typedef void (*FatalErrorCallback)(void);

extern int32              g_DEBUG_OUTPUT_LEVEL;
extern uint32             g_DEBUG_MONITOR_SWI_TRACE_MASK;
extern const char        *g_FATAL_ERROR_OVERRIDE;
extern FatalErrorCallback g_FATAL_ERROR_CALLBACK;

void
debug_log(const char *format_, ...);
int32
print_debug_text_line(const char *text_);
void
clear_debug_text_output(void);
void
print_debug_message(const char *format_, ...);
void
print_debug_message_with_leading_newline(int32 context_, int32 detail_, const char *format_, ...);
void
print_debug_hex_byte(uint32 value_);
void
print_debug_hex_byte_with_leading_newline(int32 context_, int32 detail_, uint32 value_);
void
print_debug_character(uint32 value_);
void
print_debug_character_with_leading_newline(int32 context_, int32 detail_, uint32 value_);
void
fatal_error(const char *format_, ...);
void
set_fatal_error_callback(FatalErrorCallback callback_);
