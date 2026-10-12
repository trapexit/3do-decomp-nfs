#include "debug.h"

#include "stdarg.h"
#include "stdio.h"

#define KPRINTF_MESSAGE_BYTES 1024

extern void
svc_kprintf(const char *format_, ...);

void
kprintf(const char *format_, ...)
{
  char    message[KPRINTF_MESSAGE_BYTES];
  va_list arguments;

  va_start(arguments, format_);
  vsprintf(message, format_, arguments);
  va_end(arguments);
  svc_kprintf("%s", message);
}
