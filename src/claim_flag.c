#include "claim_flag.h"

int32
claim_flag(int32 *flag_)
{
  if(*flag_ == 1)
    {
      return 0;
    }
  *flag_ = 1;
  return 1;
}

int32
clear_claim_flag(int32 *flag_)
{
  *flag_ = 0;
  return 0;
}
