/* Frontovl C-library integer conversion with the original radix behavior. */

#include "runtime_integer.h"

#include "runtime_digit.h"

#include "stdlib.h"

#define RUNTIME_ASCII_NUL 0
#define RUNTIME_ASCII_TAB '\t'
#define RUNTIME_ASCII_LINE_FEED '\n'
#define RUNTIME_ASCII_VERTICAL_TAB '\v'
#define RUNTIME_ASCII_FORM_FEED '\f'
#define RUNTIME_ASCII_CARRIAGE_RETURN '\r'
#define RUNTIME_ASCII_SPACE ' '
#define RUNTIME_ASCII_ZERO '0'
#define RUNTIME_ASCII_LOWER_X 'x'
#define RUNTIME_ASCII_UPPER_X 'X'
#define RUNTIME_OCTAL_BASE 8
#define RUNTIME_DECIMAL_BASE 10
#define RUNTIME_HEXADECIMAL_BASE 16

static int32
_is_runtime_whitespace(uint8 character_);

long
strtol(const char *text_, char **end_, int base_)
{
  const char   *cursor;
  char         *parsed_end;
  int32         is_negative;
  unsigned long value;

  cursor = text_;
  while(_is_runtime_whitespace((uint8) *cursor))
    ++cursor;

  is_negative = (*cursor == '-');
  if((*cursor == '+') || (is_negative != 0))
    ++cursor;

  value = _strtoul(cursor, &parsed_end, base_);
  if(end_ != NULL)
    {
      if(parsed_end == cursor)
        *end_ = (char *) text_;
      else
        *end_ = parsed_end;
    }

  if(is_negative != 0)
    return -(long) value;

  return (long) value;
}

unsigned long
_strtoul(const char *text_, char **end_, int32 base_)
{
  const char   *cursor;
  int32         digit;
  unsigned long result;
  int32         has_digits;

  cursor = text_;
  while(_is_runtime_whitespace((uint8) *cursor))
    ++cursor;

  has_digits = 0;
  if(*cursor == RUNTIME_ASCII_ZERO)
    {
      has_digits = 1;
      ++cursor;
      if((*cursor == RUNTIME_ASCII_LOWER_X) || (*cursor == RUNTIME_ASCII_UPPER_X))
        {
          if((base_ == 0) || (base_ == RUNTIME_HEXADECIMAL_BASE))
            {
              has_digits = 0;
              base_      = RUNTIME_HEXADECIMAL_BASE;
              ++cursor;
            }
        }
      else if(base_ == 0)
        base_ = RUNTIME_OCTAL_BASE;
    }

  if(base_ == 0)
    base_ = RUNTIME_DECIMAL_BASE;

  result = 0;
  digit  = _chval((uint8) *cursor, base_);
  while(digit >= 0)
    {
      has_digits = 1;
      result     = (result * (unsigned long) base_) + (unsigned long) digit;
      ++cursor;
      digit = _chval((uint8) *cursor, base_);
    }

  if(end_ != NULL)
    {
      if(has_digits == 0)
        *end_ = (char *) text_;
      else
        *end_ = (char *) cursor;
    }

  return result;
}

unsigned long
strtoul(const char *text_, char **end_, int base_)
{
  const char   *cursor;
  char         *parsed_end;
  int32         is_negative;
  unsigned long value;

  cursor = text_;
  while(_is_runtime_whitespace((uint8) *cursor))
    ++cursor;

  is_negative = (*cursor == '-');
  if((*cursor == '+') || (is_negative != 0))
    ++cursor;

  value = _strtoul(cursor, &parsed_end, base_);
  if(end_ != NULL)
    {
      if(parsed_end == cursor)
        *end_ = (char *) text_;
      else
        *end_ = parsed_end;
    }

  if(is_negative != 0)
    return (0UL - value);

  return value;
}

static int32
_is_runtime_whitespace(uint8 character_)
{
  return (character_ == RUNTIME_ASCII_TAB) || (character_ == RUNTIME_ASCII_LINE_FEED) ||
         (character_ == RUNTIME_ASCII_VERTICAL_TAB) || (character_ == RUNTIME_ASCII_FORM_FEED) ||
         (character_ == RUNTIME_ASCII_CARRIAGE_RETURN) || (character_ == RUNTIME_ASCII_SPACE);
}
