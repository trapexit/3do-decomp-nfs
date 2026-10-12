#include "string_token_normalization.h"

#include "types.h"

#include "ctype.h"

#define TOKEN_HORIZONTAL_TAB 0x09
#define TOKEN_LINE_FEED 0x0A
#define TOKEN_LOWERCASE_R 0x72
#define TOKEN_SPACE 0x20

void
normalize_lowercase_token(char *text_)
{
  uint8  character;
  uint32 index;

  index = 0;
  if(text_[index] == '\0')
    return;

  do
    {
      character    = (uint8) text_[index];
      text_[index] = (char) tolower(character);
      if((character == TOKEN_SPACE) || (character == TOKEN_HORIZONTAL_TAB) ||
         (character == TOKEN_LINE_FEED) || (character == TOKEN_LOWERCASE_R))
        {
          text_[index] = '\0';
          return;
        }
      index++;
    }
  while(text_[index] != '\0');
}
