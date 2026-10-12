#include "string_utilities.h"

#define COPYRIGHT_PREFIX "Copyright"
#define COPYRIGHT_PREFIX_BYTES 9
#define PATH_SEPARATOR_BACKSLASH '\\'
#define PATH_SEPARATOR_COLON ':'
#define PATH_SEPARATOR_SLASH '/'
#define ASCII_CASE_BIT 0x20
#define TEXT_SEPARATOR_SPACE ' '
#define TEXT_SEPARATOR_TAB '\t'
#define TEXT_SEPARATOR_NEWLINE '\n'

int32
string_length(const char *string_)
{
  const char *cursor;

  cursor = string_;
  while(*cursor != '\0')
    cursor++;

  return (int32) (cursor - string_);
}

int32
compare_strings_limited(const uint8 *left_, const uint8 *right_, int32 byte_count_)
{
  int32 difference;
  uint8 left_byte;

  while(byte_count_ > 0)
    {
      left_byte  = *left_++;
      difference = ((int32) left_byte - (int32) *right_++);
      if(difference != 0)
        return difference;
      if(left_byte == 0)
        return 0;
      byte_count_--;
    }

  return 0;
}

int32
compare_strings(const uint8 *left_, const uint8 *right_)
{
  int32 difference;
  uint8 left_byte;

  do
    {
      left_byte  = *left_++;
      difference = (int32) left_byte - (int32) *right_++;
      if(difference != 0)
        return difference;
    }
  while(left_byte != 0);

  return 0;
}

int32
compare_strings_ignore_case(const char *left_, const char *right_)
{
  uint8 left_byte;
  uint8 right_byte;

  for(;;)
    {
      left_byte = (uint8) *left_;
      if(left_byte == 0)
        break;
      if(((left_byte | ASCII_CASE_BIT) != ((uint8) *right_ | ASCII_CASE_BIT)))
        break;

      left_++;
      right_++;
    }

  left_byte  = (uint8) *left_;
  right_byte = (uint8) *right_;
  if(left_byte < right_byte)
    return -1;
  if(left_byte > right_byte)
    return 1;

  return 0;
}

int32
compare_strings_limited_ignore_case(const char *left_, const char *right_, int32 byte_count_)
{
  uint8 left_byte;
  uint8 right_byte;

  while(byte_count_ > 0)
    {
      left_byte = (uint8) *left_;
      if(left_byte == 0)
        break;
      if((left_byte | ASCII_CASE_BIT) != ((uint8) *right_ | ASCII_CASE_BIT))
        {
          break;
        }
      left_++;
      right_++;
      byte_count_--;
    }

  if(byte_count_ == 0)
    return 0;

  left_byte  = (uint8) *left_;
  right_byte = (uint8) *right_;
  if(left_byte < right_byte)
    return -1;
  if(left_byte > right_byte)
    return 1;
  return 0;
}

const uint8 *
skip_copyright_preamble(const uint8 *source_)
{
  if(compare_strings_limited(source_, (const uint8 *) COPYRIGHT_PREFIX, COPYRIGHT_PREFIX_BYTES) !=
     0)
    {
      return source_;
    }

  return source_ + string_length((const char *) source_) + 1;
}

char *
copy_string_padded(char *destination_, const char *source_, int32 byte_count_)
{
  char *result;
  char  source_byte;

  result = destination_;
  while(byte_count_ > 0)
    {
      source_byte     = *source_++;
      *destination_++ = source_byte;
      byte_count_--;
      if(source_byte == '\0')
        {
          while(byte_count_ > 0)
            {
              *destination_++ = '\0';
              byte_count_--;
            }
          break;
        }
    }

  return result;
}

char *
copy_fixed_string(char *destination_, const char *source_, int32 byte_count_)
{
  return copy_string_padded(destination_, source_, byte_count_);
}

const char *
find_path_leaf(const char *path_)
{
  const char *cursor;
  const char *leaf;

  cursor = path_;
  leaf   = path_;
  while(*cursor != '\0')
    {
      if((*cursor == PATH_SEPARATOR_BACKSLASH) || (*cursor == PATH_SEPARATOR_COLON) ||
         (*cursor == PATH_SEPARATOR_SLASH))
        {
          leaf = cursor + 1;
        }
      cursor++;
    }

  return leaf;
}

const char *
get_path_leaf(const char *path_)
{
  return find_path_leaf(path_);
}

int32
compare_fixed_strings(const uint8 *left_, const uint8 *right_, int32 byte_count_)
{
  return compare_strings_limited(left_, right_, byte_count_);
}

char *
find_last_character(const char *string_, uint32 character_)
{
  const char *cursor;
  const char *last_match;
  uint8       character;

  cursor     = string_;
  last_match = NULL;
  character  = (uint8) character_;
  while(*cursor != '\0')
    {
      if((uint8) *cursor == character)
        last_match = cursor;
      cursor++;
    }

  return (char *) last_match;
}

char *
append_string(char *destination_, const char *source_)
{
  char *result;

  result = destination_;
  while(*destination_ != '\0')
    destination_++;
  while((*destination_++ = *source_++) != '\0')
    {
    }

  return result;
}

char *
copy_string(char *destination_, const char *source_)
{
  char *result;

  result = destination_;
  while((*destination_++ = *source_++) != '\0')
    {
    }

  return result;
}

char *
append_c_string(char *destination_, const char *source_)
{
  return append_string(destination_, source_);
}

char *
copy_c_string(char *destination_, const char *source_)
{
  return copy_string(destination_, source_);
}

char *
find_character(const char *string_, uint32 character_)
{
  uint8 character;

  character = (uint8) character_;
  do
    {
      if((uint8) *string_ == character)
        return (char *) string_;
    }
  while(*string_++ != '\0');

  return NULL;
}

const char *
skip_text_token(const char *text_)
{
  while((*text_ == TEXT_SEPARATOR_SPACE) || (*text_ == TEXT_SEPARATOR_TAB) ||
        (*text_ == TEXT_SEPARATOR_NEWLINE))
    {
      text_++;
    }

  while((*text_ != TEXT_SEPARATOR_SPACE) && (*text_ != TEXT_SEPARATOR_TAB) &&
        (*text_ != TEXT_SEPARATOR_NEWLINE) && (*text_ != '\0'))
    {
      text_++;
    }

  while((*text_ == TEXT_SEPARATOR_SPACE) || (*text_ == TEXT_SEPARATOR_TAB) ||
        (*text_ == TEXT_SEPARATOR_NEWLINE))
    {
      text_++;
    }

  return text_;
}

void
copy_text_token(char *destination_, const char *text_)
{
  int32 token_length;

  while((*text_ == TEXT_SEPARATOR_SPACE) || (*text_ == TEXT_SEPARATOR_TAB) ||
        (*text_ == TEXT_SEPARATOR_NEWLINE))
    {
      text_++;
    }

  token_length = 0;
  while((text_[token_length] != TEXT_SEPARATOR_SPACE) &&
        (text_[token_length] != TEXT_SEPARATOR_TAB) &&
        (text_[token_length] != TEXT_SEPARATOR_NEWLINE) && (text_[token_length] != '\0'))
    {
      destination_[token_length] = text_[token_length];
      token_length++;
    }
  destination_[token_length] = '\0';
}
