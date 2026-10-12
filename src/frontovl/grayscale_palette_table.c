#include "types.h"

/* This linker-visible initializer has no direct C caller. */
void
initialize_grayscale_palette_table(uint32 *palette_table_);

#define GRAYSCALE_PALETTE_HEADER_WORD_COUNT 2
#define GRAYSCALE_PALETTE_ENTRY_COUNT 384
#define GRAYSCALE_PALETTE_FIRST_VALUE (-64)
#define GRAYSCALE_PALETTE_MAXIMUM_VALUE 255
#define GRAYSCALE_PALETTE_GREEN_BIT 0x8000
#define GRAYSCALE_PALETTE_COLOR_MASK 0xF8
#define GRAYSCALE_PALETTE_GREEN_TEST_BIT 4
#define GRAYSCALE_PALETTE_COLOR_SHIFT 2

void
initialize_grayscale_palette_table(uint32 *palette_table_)
{
  uint32 color;
  uint32 intensity;
  int32  palette_value;

  palette_table_ += GRAYSCALE_PALETTE_HEADER_WORD_COUNT;
  palette_value = GRAYSCALE_PALETTE_FIRST_VALUE;
  while(palette_value < GRAYSCALE_PALETTE_FIRST_VALUE + GRAYSCALE_PALETTE_ENTRY_COUNT)
    {
      if(palette_value < 0)
        {
          intensity = 0;
        }
      else if(palette_value > GRAYSCALE_PALETTE_MAXIMUM_VALUE)
        {
          intensity = GRAYSCALE_PALETTE_MAXIMUM_VALUE;
        }
      else
        {
          intensity = (uint32) palette_value;
        }

      if((intensity & GRAYSCALE_PALETTE_GREEN_TEST_BIT) != 0)
        {
          color = GRAYSCALE_PALETTE_GREEN_BIT;
        }
      else
        {
          color = 0;
        }
      color |= (intensity & GRAYSCALE_PALETTE_COLOR_MASK) << GRAYSCALE_PALETTE_COLOR_SHIFT;
      *palette_table_ = color;

      ++palette_table_;
      ++palette_value;
    }
}
