#include "runtime_string.h"

#include "types.h"

#include "stdlib.h"
#include "string.h"

char *
copy_null_terminated_string(char *destination_, const char *source_)
{
  char *original_destination;
  uint8 value;

  original_destination = destination_;
  do
    {
      value           = *source_++;
      *destination_++ = value;
    }
  while(value != 0);
  return original_destination;
}

int32
runtime_string_length(const char *text_)
{
  /* Preserve the original normal-return wrapper around the C runtime call. */
  int32 volatile length;

  length = (int32) strlen(text_);
  return length;
}

int32
runtime_string_to_integer(const char *text_)
{
  return atoi(text_);
}

char *
copy_runtime_string_n(char *destination_, const char *source_, uint32 byte_count_)
{
  char *original_destination;
  uint8 value;

  original_destination = destination_;
  while(byte_count_ > 0)
    {
      value = *source_;
      ++source_;
      *destination_ = value;
      ++destination_;
      --byte_count_;
      if(value == 0)
        {
          while(byte_count_ > 0)
            {
              *destination_ = 0;
              ++destination_;
              --byte_count_;
            }

          return original_destination;
        }
    }

  return original_destination;
}

char *
append_runtime_string(char *destination_, const char *source_)
{
  /* Preserve the original normal-return wrapper around the C runtime call. */
  char *volatile destination;

  destination = strcat(destination_, source_);
  return destination;
}
