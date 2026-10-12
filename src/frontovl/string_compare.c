#include "string_compare.h"

int32
compare_byte_strings(const uint8 *left_, const uint8 *right_)
{
  int32 difference;
  uint8 left_byte;

  do
    {
      left_byte  = *left_++;
      difference = (int32) left_byte - (int32) *right_++;
      if(difference != 0)
        {
          return difference;
        }
    }
  while(left_byte != 0);

  return 0;
}
