#include "random_number.h"

#include "runtime_memory.h"
#include "sdk_veneers.h"

#define RANDOM_FRACTION_MASK 0xFFFF
#define RANDOM_FRACTION_BITS 16
#define FRONTEND_RANDOM_VALUE_COUNT 6
#define FRONTEND_RANDOM_MIX_MULTIPLIER 9

uint32 *g_FRONTEND_RANDOM_VALUE_DESTINATION = NULL;

void
generate_frontend_random_values(void)
{
  uint32 values[FRONTEND_RANDOM_VALUE_COUNT];
  uint32 first_product;
  uint32 second_product;
  int32  value_index;

  for(value_index = 0; value_index < FRONTEND_RANDOM_VALUE_COUNT; value_index++)
    {
      first_product = get_runtime_random();
      first_product *= GetAudioTime();

      second_product = get_runtime_random();
      second_product *= GetAudioTime();
      values[value_index] = (first_product * second_product * FRONTEND_RANDOM_MIX_MULTIPLIER);
    }

  RUNTIME_MEMCPY_VENEER(g_FRONTEND_RANDOM_VALUE_DESTINATION, values, sizeof(values));
}

int32
random_below(int32 upper_bound_)
{
  uint32 fraction;

  fraction = get_runtime_random() & RANDOM_FRACTION_MASK;
  return (int32) (upper_bound_ * fraction) >> RANDOM_FRACTION_BITS;
}
