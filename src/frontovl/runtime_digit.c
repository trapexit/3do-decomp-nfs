/* Frontovl numeric-text digit decoding. */

#include "runtime_digit.h"

#define RUNTIME_ASCII_DIGIT_FIRST '0'
#define RUNTIME_ASCII_DIGIT_LAST '9'
#define RUNTIME_ASCII_UPPER_FIRST 'A'
#define RUNTIME_ASCII_UPPER_LAST 'Z'
#define RUNTIME_ASCII_LOWER_FIRST 'a'
#define RUNTIME_ASCII_LOWER_LAST 'z'
#define RUNTIME_ALPHABETIC_DIGIT_OFFSET 10

int32
_chval(int32 character_, int32 base_)
{
  int32 digit;

  if((character_ >= RUNTIME_ASCII_DIGIT_FIRST) && (character_ <= RUNTIME_ASCII_DIGIT_LAST))
    digit = (character_ - RUNTIME_ASCII_DIGIT_FIRST);
  else if((character_ >= RUNTIME_ASCII_UPPER_FIRST) && (character_ <= RUNTIME_ASCII_UPPER_LAST))
    digit = (character_ - RUNTIME_ASCII_UPPER_FIRST) + RUNTIME_ALPHABETIC_DIGIT_OFFSET;
  else if((character_ >= RUNTIME_ASCII_LOWER_FIRST) && (character_ <= RUNTIME_ASCII_LOWER_LAST))
    digit = (character_ - RUNTIME_ASCII_LOWER_FIRST) + RUNTIME_ALPHABETIC_DIGIT_OFFSET;
  else
    return -1;

  if(digit >= base_)
    return -1;

  return digit;
}
