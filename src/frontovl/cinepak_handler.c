/* Frontovl Cinepak decoder-handler allocation and table initialization. */

#include "cinepak_handler.h"

#include "tracked_memory.h"

#define CINEPAK_HANDLER_BYTE_COUNT 0x3C00
#define CINEPAK_HANDLER_DISPATCH_TABLE_OFFSET 0x3600
#define CINEPAK_COLOR_DITHER_ENTRY_COUNT 16
#define CINEPAK_COLOR_INTENSITY_START (-64)
#define CINEPAK_COLOR_INTENSITY_LIMIT 0x140
#define CINEPAK_COLOR_VALUE_MAXIMUM 0xFF
#define CINEPAK_COLOR_VALUE_SHIFT 3
#define CINEPAK_BIG_ENDIAN_HIGH_BYTE_SHIFT 8
#define CINEPAK_DITHER_COLUMN_MASK 3
#define CINEPAK_DITHER_ROW_SHIFT 2
#define CINEPAK_DITHER_ROW_HIGH_BIT 2
#define CINEPAK_DITHER_ROW_HIGH_BIT_SHIFT 1
#define CINEPAK_DITHER_GRAY_CODE_SHIFT 1
#define CINEPAK_DITHER_GRAY_CODE_MASK 1
#define CINEPAK_DITHER_COLUMN_HIGH_WEIGHT 5
#define CINEPAK_SOLID_ENTRY_WORD_COUNT 4
#define CINEPAK_DISPATCH_ZERO_ENTRY_COUNT 0x40
#define CINEPAK_DISPATCH_FIRST_RANGE_COUNT 0x80
#define CINEPAK_DISPATCH_SECOND_RANGE_START 0x80
#define CINEPAK_DISPATCH_SECOND_RANGE_END 0x100
#define CINEPAK_DISPATCH_FIRST_ROW_MAXIMUM 0x1F
#define CINEPAK_DISPATCH_SECOND_ROW_MAXIMUM 0x1E
#define CINEPAK_DISPATCH_COLUMN_MASK 7
#define CINEPAK_DISPATCH_ROW_SHIFT 3
#define CINEPAK_DISPATCH_ROW_VALUE_SHIFT 16
#define CINEPAK_DISPATCH_SECOND_COLUMN_OFFSET 8
#define CINEPAK_DISPATCH_FINAL_ENTRY_COUNT 0x40
#define CINEPAK_DISPATCH_FINAL_VALUE 0x000F000F

static int32
_clamp_cinepak_color_value(int32 value_);

static int16
_read_cinepak_dither_offset(const int16 *value_);

static int32
_get_cinepak_dither_offset(int32 index_);

static int32
_clamp_cinepak_color_value(int32 value_)
{
  if(value_ < 0)
    return 0;

  if(value_ > CINEPAK_COLOR_VALUE_MAXIMUM)
    return CINEPAK_COLOR_VALUE_MAXIMUM;

  return value_;
}

static int16
_read_cinepak_dither_offset(const int16 *value_)
{
  const uint8 *bytes;

  bytes = (const uint8 *) value_;
  return (int16) (((uint16) bytes[0] << CINEPAK_BIG_ENDIAN_HIGH_BYTE_SHIFT) | bytes[1]);
}

static int32
_get_cinepak_dither_offset(int32 index_)
{
  int32 column;
  int32 column_offset;
  int32 row;
  int32 row_offset;

  row        = (index_ >> CINEPAK_DITHER_ROW_SHIFT);
  column     = (index_ & CINEPAK_DITHER_COLUMN_MASK);
  row_offset = row | ((row & CINEPAK_DITHER_ROW_HIGH_BIT) << CINEPAK_DITHER_ROW_HIGH_BIT_SHIFT);
  column_offset =
    (((column ^ (column >> CINEPAK_DITHER_GRAY_CODE_SHIFT)) & CINEPAK_DITHER_GRAY_CODE_MASK) *
     CINEPAK_DITHER_COLUMN_HIGH_WEIGHT) +
    (column & CINEPAK_DITHER_ROW_HIGH_BIT);
  return (row_offset ^ column_offset);
}

void
initialize_cinepak_color_tables(uint16 *table_)
{
  int32 color_value;
  int32 dither_offset;
  int32 dither_index;
  int16 dither_offsets[CINEPAK_COLOR_DITHER_ENTRY_COUNT];

  for(dither_index = 0; dither_index < CINEPAK_COLOR_DITHER_ENTRY_COUNT; dither_index += 1)
    dither_offsets[dither_index] = (int16) _get_cinepak_dither_offset(dither_index);

  for(color_value = CINEPAK_COLOR_INTENSITY_START; color_value < CINEPAK_COLOR_INTENSITY_LIMIT;
      color_value += 1)
    {
      for(dither_index = 0; dither_index < CINEPAK_COLOR_DITHER_ENTRY_COUNT; dither_index += 1)
        {
          dither_offset = _read_cinepak_dither_offset(&dither_offsets[dither_index]);
          *table_++     = (uint16) (_clamp_cinepak_color_value(color_value + dither_offset) >>
                                    CINEPAK_COLOR_VALUE_SHIFT);
        }
    }

  for(color_value = CINEPAK_COLOR_INTENSITY_START; color_value < CINEPAK_COLOR_INTENSITY_LIMIT;
      color_value += 1)
    {
      *table_++ = 0;
      *table_++ = (uint16) (_clamp_cinepak_color_value(color_value) >> CINEPAK_COLOR_VALUE_SHIFT);
    }
}

void
initialize_cinepak_dispatch_table(uint32 *table_)
{
  int32 index;

  for(index = 0; index < CINEPAK_DISPATCH_ZERO_ENTRY_COUNT; ++index)
    *table_++ = 0;

  for(index = 0; index < CINEPAK_DISPATCH_FIRST_RANGE_COUNT; ++index)
    {
      *table_++ =
        ((uint32) (index & CINEPAK_DISPATCH_COLUMN_MASK)) |
        ((uint32) (CINEPAK_DISPATCH_FIRST_ROW_MAXIMUM - (index >> CINEPAK_DISPATCH_ROW_SHIFT))
         << CINEPAK_DISPATCH_ROW_VALUE_SHIFT);
    }

  for(index = CINEPAK_DISPATCH_SECOND_RANGE_START; index < CINEPAK_DISPATCH_SECOND_RANGE_END;
      ++index)
    {
      *table_++ = ((uint32) ((index & CINEPAK_DISPATCH_COLUMN_MASK) +
                             CINEPAK_DISPATCH_SECOND_COLUMN_OFFSET)) |
                  ((uint32) (CINEPAK_DISPATCH_SECOND_ROW_MAXIMUM -
                             ((index & (CINEPAK_DISPATCH_SECOND_RANGE_END - 1)) >>
                              CINEPAK_DISPATCH_ROW_SHIFT))
                   << CINEPAK_DISPATCH_ROW_VALUE_SHIFT);
    }

  for(index = 0; index < CINEPAK_DISPATCH_FINAL_ENTRY_COUNT; ++index)
    *table_++ = CINEPAK_DISPATCH_FINAL_VALUE;
}

void *
create_cinepak_handler(void)
{
  uint8 *handler;

  handler = (uint8 *) malloc(CINEPAK_HANDLER_BYTE_COUNT);
  if(handler == NULL)
    return NULL;

  initialize_cinepak_color_tables((uint16 *) handler);
  initialize_cinepak_dispatch_table((uint32 *) (handler + CINEPAK_HANDLER_DISPATCH_TABLE_OFFSET));
  return handler;
}

void
delete_cinepak_handler(void *handler_)
{
  free(handler_);
}
