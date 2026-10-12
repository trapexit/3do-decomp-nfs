/* Frontovl alternate Cinepak palette-handler allocation. */

#include "cinepak_palette_handler.h"

#include "kernel_memory.h"
#include "kernel_memory_list.h"

#include "kernel.h"

#define CINEPAK_PALETTE_HANDLER_BYTE_COUNT 0x3C00
#define CINEPAK_PALETTE_INTENSITY_TABLE_OFFSET 0x3000
#define CINEPAK_PALETTE_VALUE_FIRST (-64)
#define CINEPAK_PALETTE_VALUE_LIMIT 0x140
#define CINEPAK_PALETTE_VALUE_MAXIMUM 0xFF
#define CINEPAK_PALETTE_GREEN_TEST_BIT 4
#define CINEPAK_PALETTE_GREEN_BIT 0x8000
#define CINEPAK_PALETTE_COLOR_MASK 0xF8
#define CINEPAK_PALETTE_COLOR_SHIFT 2
#define CINEPAK_PALETTE_INTENSITY_SHIFT 3
#define CINEPAK_PALETTE_MEMORY_TYPE 0x80000

void *
create_cinepak_palette_handler(CinepakPaletteAllocateFunction allocate_)
{
  int32   clamped_value;
  int32   value;
  uint32  color;
  uint32 *color_table;
  uint32 *intensity_table;
  uint8  *handler;

  if(allocate_ != NULL)
    {
      handler = (uint8 *) allocate_(CINEPAK_PALETTE_HANDLER_BYTE_COUNT);
    }
  else
    {
      handler = (uint8 *) allocate_memory_from_lists(CURRENTTASK->t_FreeMemoryLists,
                                                     CINEPAK_PALETTE_HANDLER_BYTE_COUNT,
                                                     CINEPAK_PALETTE_MEMORY_TYPE);
    }
  if(handler == NULL)
    return NULL;

  color_table     = (uint32 *) handler;
  intensity_table = (uint32 *) (handler + CINEPAK_PALETTE_INTENSITY_TABLE_OFFSET);
  for(value = CINEPAK_PALETTE_VALUE_FIRST; value < CINEPAK_PALETTE_VALUE_LIMIT; ++value)
    {
      if(value < 0)
        clamped_value = 0;
      else if(value > CINEPAK_PALETTE_VALUE_MAXIMUM)
        clamped_value = CINEPAK_PALETTE_VALUE_MAXIMUM;
      else
        clamped_value = value;

      if((clamped_value & CINEPAK_PALETTE_GREEN_TEST_BIT) != 0)
        color = CINEPAK_PALETTE_GREEN_BIT;
      else
        color = 0;
      *color_table++     = color | ((uint32) (clamped_value & CINEPAK_PALETTE_COLOR_MASK)
                                    << CINEPAK_PALETTE_COLOR_SHIFT);
      *intensity_table++ = (uint32) clamped_value >> CINEPAK_PALETTE_INTENSITY_SHIFT;
    }
  return handler;
}

void
delete_cinepak_palette_handler(void *handler_, CinepakPaletteFreeFunction free_)
{
  if(free_ != NULL)
    {
      free_(handler_, CINEPAK_PALETTE_HANDLER_BYTE_COUNT);
    }
  else
    {
      free_memory_to_lists(CURRENTTASK->t_FreeMemoryLists,
                           handler_,
                           CINEPAK_PALETTE_HANDLER_BYTE_COUNT);
    }
}
