#include "diagnostics.h"

#include "shutdown_callbacks.h"
#include "string_utilities.h"

#include "debug.h"

#include "stdarg.h"
#include "stdio.h"

#define DEBUG_OUTPUT_PENDING 1
#define DEBUG_OUTPUT_ENABLED 2

int32              g_DEBUG_OUTPUT_LEVEL = DEBUG_OUTPUT_ENABLED;
uint32             g_DEBUG_MONITOR_SWI_TRACE_MASK;
const char        *g_FATAL_ERROR_OVERRIDE = NULL;
FatalErrorCallback g_FATAL_ERROR_CALLBACK = run_shutdown_callbacks;

void
debug_log(const char *format_, ...)
{
  (void) format_;
}

int32
print_debug_text_line(const char *text_)
{
  print_debug_message("%s\n", text_);
  return 1;
}

void
clear_debug_text_output(void)
{
  print_debug_message("\n\n\n\n\n\n\n\n\n\n\n\n\n"
                      "\n\n\n\n\n\n\n\n\n\n\n\n");
}

void
print_debug_message(const char *format_, ...)
{
  char    message[DIAGNOSTIC_BUFFER_BYTES];
  va_list arguments;

  va_start(arguments, format_);
  vsprintf(message, format_, arguments);
  va_end(arguments);

  if(g_DEBUG_OUTPUT_LEVEL >= DEBUG_OUTPUT_ENABLED)
    kprintf("%s", message);
}

void
print_debug_message_with_leading_newline(int32 context_, int32 detail_, const char *format_, ...)
{
  char    message[DIAGNOSTIC_BUFFER_BYTES];
  va_list arguments;

  (void) context_;
  (void) detail_;
  va_start(arguments, format_);
  vsprintf(message, format_, arguments);
  va_end(arguments);

  if(g_DEBUG_OUTPUT_LEVEL >= DEBUG_OUTPUT_ENABLED)
    kprintf("\n%s", message);
}

void
print_debug_hex_byte(uint32 value_)
{
  print_debug_message("%02x", value_);
}

void
print_debug_hex_byte_with_leading_newline(int32 context_, int32 detail_, uint32 value_)
{
  print_debug_message_with_leading_newline(context_, detail_, "%02x", value_);
}

void
print_debug_character(uint32 value_)
{
  print_debug_message("%c", value_);
}

void
print_debug_character_with_leading_newline(int32 context_, int32 detail_, uint32 value_)
{
  print_debug_message_with_leading_newline(context_, detail_, "%c", value_);
}

void
fatal_error(const char *format_, ...)
{
  char    message[DIAGNOSTIC_BUFFER_BYTES];
  va_list arguments;

  va_start(arguments, format_);
  vsprintf(message, format_, arguments);
  va_end(arguments);

  if(g_FATAL_ERROR_OVERRIDE != NULL)
    copy_string(message, g_FATAL_ERROR_OVERRIDE);

  if(g_DEBUG_OUTPUT_LEVEL == DEBUG_OUTPUT_PENDING)
    g_DEBUG_OUTPUT_LEVEL = DEBUG_OUTPUT_ENABLED;
  if(g_DEBUG_OUTPUT_LEVEL >= DEBUG_OUTPUT_ENABLED)
    kprintf("%s", message);

  g_FATAL_ERROR_CALLBACK();
}

void
set_fatal_error_callback(FatalErrorCallback callback_)
{
  g_FATAL_ERROR_CALLBACK = callback_;
}
