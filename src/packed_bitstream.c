#include "packed_bitstream.h"

#define PACKED_BIT_WORD_BITS 32
#define PACKED_BIT_REFILL_THRESHOLD 16
#define PACKED_BIT_WIDE_CHUNK_BITS 16
#define PACKED_BIT_BYTE_BITS 8
#define PACKED_BIT_BYTE_HIGH_SHIFT 24
#define PACKED_VARIABLE_INITIAL_VALUE 2
#define PACKED_VARIABLE_INITIAL_BITS 1
#define PACKED_VARIABLE_BIAS 4
#define PACKED_FORMAT_SIZE_BYTES 3
#define PACKED_FORMAT_SIZE_FLAG 0x0100
#define PACKED_FORMAT_FLAG_CLEAR_MASK 0xFFFFFEFF
#define PACKED_OUTPUT_SIZE_BITS 24
#define PACKED_TERMINATOR_BITS 8
#define PACKED_LITERAL_BITS 8
#define PACKED_CODE_TABLE_SIZE 16
#define PACKED_SYMBOL_COUNT 256
#define PACKED_CODE_COMPARE_SHIFT 16
#define PACKED_CODE_MASK 0xFFFF
#define PACKED_SYMBOL_MASK 0xFFU
#define PACKED_SYMBOL_BEFORE_FIRST 0xFFFFFFFFU
#define PACKED_FORMAT_DELTA 0x32FB
#define PACKED_FORMAT_DOUBLE_DELTA 0x34FB

uint32       g_PACKED_BITSTREAM_LAYOUT_PREFIX[3];
const uint8 *g_PACKED_INPUT_CURSOR       = NULL;
uint8       *g_PACKED_OUTPUT_CURSOR      = NULL;
int32        g_PACKED_BUFFERED_BIT_COUNT = 0;
uint32       g_PACKED_BIT_BUFFER         = 0;

uint32
read_packed_bits(int32 bit_count_)
{
  uint32 upper_bits;
  uint32 lower_bits;
  uint32 bits;
  uint8  next_byte;

  if(bit_count_ > PACKED_BIT_WIDE_CHUNK_BITS)
    {
      upper_bits = read_packed_bits(bit_count_ - PACKED_BIT_WIDE_CHUNK_BITS);
      lower_bits = read_packed_bits(PACKED_BIT_WIDE_CHUNK_BITS);
      return ((upper_bits << PACKED_BIT_WIDE_CHUNK_BITS) | lower_bits);
    }

  if(bit_count_ == 0)
    bits = 0;
  else
    bits = (g_PACKED_BIT_BUFFER >> (PACKED_BIT_WORD_BITS - bit_count_));

  g_PACKED_BIT_BUFFER <<= bit_count_;
  g_PACKED_BUFFERED_BIT_COUNT -= bit_count_;
  while(g_PACKED_BUFFERED_BIT_COUNT < PACKED_BIT_REFILL_THRESHOLD)
    {
      next_byte = *g_PACKED_INPUT_CURSOR;
      g_PACKED_INPUT_CURSOR++;
      g_PACKED_BIT_BUFFER |=
        ((uint32) next_byte << (PACKED_BIT_BYTE_HIGH_SHIFT - g_PACKED_BUFFERED_BIT_COUNT));
      g_PACKED_BUFFERED_BIT_COUNT += PACKED_BIT_BYTE_BITS;
    }

  return bits;
}

int32
read_packed_variable_length(void)
{
  int32 value_base;
  int32 value_bits;

  value_base = PACKED_VARIABLE_INITIAL_VALUE;
  value_bits = PACKED_VARIABLE_INITIAL_BITS;
  do
    {
      value_base <<= 1;
      value_bits++;
    }
  while(read_packed_bits(1) == 0);

  return ((int32) read_packed_bits(value_bits) + value_base - PACKED_VARIABLE_BIAS);
}

int32
decode_huffman_packed_bytes(const uint8 *source_, uint8 *destination_, int32 unpack_)
{
  uint8  symbols[PACKED_SYMBOL_COUNT];
  uint8  symbol_used[PACKED_SYMBOL_COUNT];
  uint32 code_thresholds[PACKED_CODE_TABLE_SIZE];
  int32  first_code_offsets[PACKED_CODE_TABLE_SIZE];
  int32  code_counts[PACKED_CODE_TABLE_SIZE];
  uint32 packed_format;
  uint32 threshold;
  uint32 current_symbol;
  uint32 symbol_index;
  uint8  terminator;
  uint8  symbol;
  uint8  delta;
  uint8  accumulator;
  uint8 *output_end;
  int32  output_size;
  int32  symbol_count;
  int32  assigned_count;
  int32  canonical_end;
  int32  code_length;
  int32  max_code_length;
  int32  count;
  int32  skip_count;
  int32  repeat_count;

  output_size            = 0;
  g_PACKED_INPUT_CURSOR  = source_;
  g_PACKED_OUTPUT_CURSOR = destination_;
  if(source_ == NULL)
    return output_size;

  packed_format = ((uint32) *g_PACKED_INPUT_CURSOR << PACKED_LITERAL_BITS);
  g_PACKED_INPUT_CURSOR++;
  packed_format |= *g_PACKED_INPUT_CURSOR;
  g_PACKED_INPUT_CURSOR++;
  if((packed_format & PACKED_FORMAT_SIZE_FLAG) != 0)
    g_PACKED_INPUT_CURSOR += PACKED_FORMAT_SIZE_BYTES;
  packed_format &= PACKED_FORMAT_FLAG_CLEAR_MASK;

  g_PACKED_BUFFERED_BIT_COUNT = 0;
  g_PACKED_BIT_BUFFER         = 0;
  read_packed_bits(0);
  output_size = (int32) read_packed_bits(PACKED_OUTPUT_SIZE_BITS);
  if(unpack_ == 0)
    return output_size;

  terminator    = (uint8) read_packed_bits(PACKED_TERMINATOR_BITS);
  symbol_count  = 0;
  canonical_end = 0;
  code_length   = 1;
  do
    {
      first_code_offsets[code_length] = ((canonical_end << 1) - symbol_count);
      count                           = read_packed_variable_length();
      code_counts[code_length]        = count;
      symbol_count += count;
      canonical_end = ((canonical_end << 1) + count);
      threshold     = 0;
      if(count != 0)
        {
          threshold = ((uint32) canonical_end << (PACKED_CODE_COMPARE_SHIFT - code_length));
          threshold &= PACKED_CODE_MASK;
        }
      code_thresholds[code_length] = threshold;
      max_code_length              = code_length;
      code_length++;
    }
  while((count == 0) || (threshold != 0));

  for(symbol_index = 0; symbol_index < PACKED_SYMBOL_COUNT; symbol_index++)
    {
      symbol_used[symbol_index] = 0;
    }

  current_symbol = PACKED_SYMBOL_BEFORE_FIRST;
  assigned_count = 0;
  while(assigned_count < symbol_count)
    {
      skip_count = (read_packed_variable_length() + 1);
      while(skip_count != 0)
        {
          current_symbol = ((current_symbol + 1U) & PACKED_SYMBOL_MASK);
          if(symbol_used[current_symbol] == 0)
            skip_count--;
        }
      symbol_used[current_symbol] = 1;
      symbols[assigned_count]     = (uint8) current_symbol;
      assigned_count++;
    }

  for(;;)
    {
      code_length = 1;
      while((code_thresholds[code_length] <= (g_PACKED_BIT_BUFFER >> PACKED_CODE_COMPARE_SHIFT)) &&
            (code_length < max_code_length))
        {
          code_length++;
        }
      symbol_index = (read_packed_bits(code_length) - first_code_offsets[code_length]);
      symbol       = symbols[symbol_index];
      if(symbol != terminator)
        {
          *g_PACKED_OUTPUT_CURSOR++ = symbol;
          continue;
        }

      repeat_count = read_packed_variable_length();
      if(repeat_count != 0)
        {
          symbol = g_PACKED_OUTPUT_CURSOR[-1];
          while(repeat_count != 0)
            {
              *g_PACKED_OUTPUT_CURSOR++ = symbol;
              repeat_count--;
            }
          continue;
        }

      if(read_packed_bits(1) == 0)
        {
          *g_PACKED_OUTPUT_CURSOR++ = (uint8) read_packed_bits(PACKED_LITERAL_BITS);
          continue;
        }
      break;
    }

  if(packed_format == PACKED_FORMAT_DELTA)
    {
      accumulator   = 0;
      g_PACKED_OUTPUT_CURSOR = destination_;
      output_end    = destination_ + output_size;
      while(g_PACKED_OUTPUT_CURSOR < output_end)
        {
          accumulator += *g_PACKED_OUTPUT_CURSOR;
          *g_PACKED_OUTPUT_CURSOR++ = accumulator;
        }
    }
  else if(packed_format == PACKED_FORMAT_DOUBLE_DELTA)
    {
      delta         = 0;
      accumulator   = 0;
      g_PACKED_OUTPUT_CURSOR = destination_;
      output_end    = destination_ + output_size;
      while(g_PACKED_OUTPUT_CURSOR < output_end)
        {
          delta += *g_PACKED_OUTPUT_CURSOR;
          accumulator += delta;
          *g_PACKED_OUTPUT_CURSOR++ = accumulator;
        }
    }

  return output_size;
}
