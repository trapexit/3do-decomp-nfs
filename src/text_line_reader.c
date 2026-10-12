#include "text_line_reader.h"

#include "fixed_math.h"

#include "filestreamfunctions.h"

#include "stdlib.h"

#define TEXT_COMMENT_MARKER '#'
#define TEXT_NEWLINE '\n'
#define TEXT_CARRIAGE_RETURN '\r'
#define TEXT_END_OF_STREAM 0xff
#define TEXT_VALUE_LINE_BUFFER_SIZE 200
#define TEXT_VALUE_LINE_READ_LIMIT 99

char *
read_text_line(char *destination_, int32 byte_count_, void *stream_)
{
  int32 byte_index;
  uint8 character;

  byte_index = 0;
  ReadDiskStream((Stream *) stream_, (char *) &character, 1);
  if(character == TEXT_END_OF_STREAM)
    return NULL;

  while(1)
    {
      if(character == TEXT_CARRIAGE_RETURN)
        {
          destination_[byte_index] = TEXT_NEWLINE;
          byte_index++;
          break;
        }
      if(byte_index >= byte_count_)
        break;
      if(character == TEXT_END_OF_STREAM)
        break;

      destination_[byte_index] = character;
      byte_index++;
      ReadDiskStream((Stream *) stream_, (char *) &character, 1);
    }

  destination_[byte_index] = '\0';
  return destination_;
}

char *
read_noncomment_text_line(char *destination_, int32 byte_count_, void *stream_)
{
  char *result;

  do
    {
      result = read_text_line(destination_, byte_count_, stream_);
      if(result == NULL)
        return NULL;
    }
  while((destination_[0] == TEXT_COMMENT_MARKER) || (destination_[0] == TEXT_NEWLINE));

  return result;
}

int32
read_second_fixed16_text_line(void *stream_)
{
  char text[TEXT_VALUE_LINE_BUFFER_SIZE];

  if(read_text_line(text, TEXT_VALUE_LINE_READ_LIMIT, stream_) == NULL)
    {
      return 0;
    }
  if(read_text_line(text, TEXT_VALUE_LINE_READ_LIMIT, stream_) == NULL)
    {
      return 0;
    }

  return parse_fixed16_decimal(text);
}

int32
read_noncomment_fixed16_text_line(void *stream_)
{
  char *result;
  char  text[TEXT_VALUE_LINE_BUFFER_SIZE];

  do
    {
      result = read_text_line(text, TEXT_VALUE_LINE_READ_LIMIT, stream_);
      if(result == NULL)
        {
          return 0;
        }
    }
  while((text[0] == TEXT_COMMENT_MARKER) || (text[0] == TEXT_NEWLINE));

  return parse_fixed16_decimal(text);
}

int32
read_noncomment_integer_text_line(void *stream_)
{
  char *result;
  char  text[TEXT_VALUE_LINE_BUFFER_SIZE];

  do
    {
      result = read_text_line(text, TEXT_VALUE_LINE_READ_LIMIT, stream_);
      if(result == NULL)
        {
          return 0;
        }
    }
  while((text[0] == TEXT_COMMENT_MARKER) || (text[0] == TEXT_NEWLINE));

  return atoi(text);
}
