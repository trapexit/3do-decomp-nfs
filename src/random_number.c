#include "random_number.h"

#define RANDOM_ACCUMULATOR_WORD_INDEX 4
#define RANDOM_COUNTER_WORD_INDEX 5
#define RANDOM_FRACTION_MASK 0xffff
#define RANDOM_FRACTION_SHIFT 16
#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

COMPILE_TIME_SIZE_ASSERT(uint32_size, sizeof(uint32) == 4);

/* Exact six-word initializer stored in the retail AIF at g_RANDOM_STATE. */
uint32 g_RANDOM_STATE[RANDOM_STATE_WORD_COUNT] = { 0xf22d0e56, 0x883126e9, 0xc624dd2f,
                                                   0x0702c49c, 0x9e353f7d, 0x6fdf3b64 };

uint32
next_random_word(void)
{
  int32  word_index;
  uint32 addend;
  uint32 carry;
  uint32 next_word;
  uint32 old_word;
  uint32 sum;

  addend     = g_RANDOM_STATE[RANDOM_COUNTER_WORD_INDEX];
  carry      = 0;
  word_index = RANDOM_ACCUMULATOR_WORD_INDEX;
  while(word_index >= 0)
    {
      old_word                   = g_RANDOM_STATE[word_index];
      sum                        = (old_word + addend);
      next_word                  = (sum + carry);
      carry                      = ((sum < old_word) || (next_word < sum));
      g_RANDOM_STATE[word_index] = next_word;
      addend                     = next_word;
      word_index--;
    }

  word_index = RANDOM_COUNTER_WORD_INDEX;
  while(word_index >= 0)
    {
      g_RANDOM_STATE[word_index] = (g_RANDOM_STATE[word_index] + 1);
      if(g_RANDOM_STATE[word_index] != 0)
        break;
      word_index--;
    }

  return g_RANDOM_STATE[0];
}

int32
scale_by_random_fraction(int32 value_)
{
  uint32 product;

  product = (uint32) value_ * (next_random_word() & RANDOM_FRACTION_MASK);
  return ((int32) product) >> RANDOM_FRACTION_SHIFT;
}
