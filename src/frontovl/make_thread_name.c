#include "streaming_thread.h"

#define THREAD_NAME_MINIMUM_SIZE 9
#define THREAD_NAME_SUFFIX_SIZE 8
#define THREAD_NAME_HEX_MASK 0x0F
#define THREAD_NAME_HEX_SHIFT 4

char g_THREAD_NAME_HEX_DIGITS[20] = "0123456789ABCDEF";

char *
make_thread_name(char       *destination_,
                 int32       destination_size_,
                 const char *prefix_,
                 int32       identifier_)
{
  char *suffix;
  int32 prefix_index;
  int32 suffix_index;
  char  prefix_character;

  destination_[0] = 0;
  if(destination_size_ < THREAD_NAME_MINIMUM_SIZE)
    {
      return NULL;
    }

  suffix       = destination_;
  prefix_index = 0;
  while(prefix_index < destination_size_ - THREAD_NAME_SUFFIX_SIZE)
    {
      prefix_character = prefix_[prefix_index++];
      *suffix++        = prefix_character;
      if(prefix_character == 0)
        {
          break;
        }
    }

  --suffix;
  suffix[THREAD_NAME_SUFFIX_SIZE] = 0;
  suffix_index                    = THREAD_NAME_SUFFIX_SIZE - 1;
  while(suffix_index >= 0)
    {
      suffix[suffix_index] = g_THREAD_NAME_HEX_DIGITS[identifier_ & THREAD_NAME_HEX_MASK];
      identifier_ >>= THREAD_NAME_HEX_SHIFT;
      --suffix_index;
    }

  return destination_;
}
