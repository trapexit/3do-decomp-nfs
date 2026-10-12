/* Frontovl runtime error reporting. */

#include "runtime_error.h"

#include "operror.h"

#include "stdio.h"

#define RUNTIME_ERROR_TEXT_BYTE_COUNT 128
#define RUNTIME_ERROR_TEXT_FORMAT "%s\n"

void
print_runtime_error(Item error_)
{
  char error_text[RUNTIME_ERROR_TEXT_BYTE_COUNT];

  GetSysErr(error_text, RUNTIME_ERROR_TEXT_BYTE_COUNT, error_);
  printf(RUNTIME_ERROR_TEXT_FORMAT, error_text);
}
