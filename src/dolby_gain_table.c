#include "sound_effect_resources.h"

#define DOLBY_FULL_GAIN 0x7FFF
#define DOLBY_NEGATIVE_FULL_GAIN (-DOLBY_FULL_GAIN)
#define DOLBY_GAIN_SCALE 0x01FF0000
#define DOLBY_GAIN_QUADRANT_SAMPLES 0x80
#define DOLBY_GAIN_MIDPOINT 0x40
#define DOLBY_GAIN_THREE_QUARTERS 0xC0
#define DOLBY_GAIN_TABLE_SECOND_HALF (DOLBY_GAIN_QUADRANT_SAMPLES * 2)
#define DOLBY_FIXED_SHIFT 16

StereoGain g_DOLBY_GAIN_TABLE[DOLBY_GAIN_TABLE_ENTRY_COUNT];

void
initialize_dolby_gain_table(void)
{
  int32 gain;
  int32 table_index;

  for(table_index = 0; table_index < DOLBY_GAIN_QUADRANT_SAMPLES; table_index++)
    {
      gain = ((DOLBY_GAIN_MIDPOINT - table_index) * DOLBY_GAIN_SCALE) >> DOLBY_FIXED_SHIFT;
      g_DOLBY_GAIN_TABLE[table_index].right = gain;
      g_DOLBY_GAIN_TABLE[table_index].left  = DOLBY_FULL_GAIN;
      g_DOLBY_GAIN_TABLE[table_index + DOLBY_GAIN_TABLE_SECOND_HALF].left =
        DOLBY_NEGATIVE_FULL_GAIN;
      g_DOLBY_GAIN_TABLE[table_index + DOLBY_GAIN_TABLE_SECOND_HALF].right = -gain;
    }

  for(table_index = DOLBY_GAIN_QUADRANT_SAMPLES; table_index < DOLBY_GAIN_TABLE_SECOND_HALF;
      table_index++)
    {
      gain = ((DOLBY_GAIN_THREE_QUARTERS - table_index) * DOLBY_GAIN_SCALE) >> DOLBY_FIXED_SHIFT;
      g_DOLBY_GAIN_TABLE[table_index].left  = gain;
      g_DOLBY_GAIN_TABLE[table_index].right = DOLBY_NEGATIVE_FULL_GAIN;
      g_DOLBY_GAIN_TABLE[table_index + DOLBY_GAIN_TABLE_SECOND_HALF].left  = -gain;
      g_DOLBY_GAIN_TABLE[table_index + DOLBY_GAIN_TABLE_SECOND_HALF].right = DOLBY_FULL_GAIN;
    }
}
