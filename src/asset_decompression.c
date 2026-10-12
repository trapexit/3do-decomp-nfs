#include "asset_decompression.h"

#include "diagnostics.h"
#include "memory.h"
#include "packed_bitstream.h"
#include "packed_format.h"
#include "string_utilities.h"

#define DELTA_TAG_COMPACT 0x62FB
#define DELTA_TAG_EXTENDED 0x66FB
#define DELTA_DEFAULT_HEADER_BYTES 2
#define DELTA_COMPACT_HEADER_BYTES 5
#define DELTA_EXTENDED_HEADER_BYTES 6
#define DELTA_LENGTH_HIGH_BYTE 0
#define DELTA_LENGTH_MIDDLE_BYTE 1
#define DELTA_LENGTH_LOW_BYTE 2
#define DELTA_LENGTH_HIGH_SHIFT 16
#define DELTA_LENGTH_MIDDLE_SHIFT 8
#define DELTA_LENGTH_BYTES 3
#define RLE_TAG_EXTENDED 0x7BFB
#define RLE_DEFAULT_HEADER_BYTES 2
#define RLE_EXTENDED_HEADER_BYTES 5
#define RLE_BLOCK_WIDTH_BYTE 3
#define RLE_CONTROL_WIDTH_BYTE 4
#define RLE_STREAM_HEADER_BYTES 5
#define REFPACK_OPTIONAL_HEADER 0x01
#define REFPACK_SIZE_HIGH_SHIFT 16
#define REFPACK_SIZE_MIDDLE_SHIFT 8
#define REFPACK_TOKEN_MEDIUM 0x80
#define REFPACK_TOKEN_LONG 0xC0
#define REFPACK_TOKEN_LITERAL 0xE0
#define REFPACK_TOKEN_END 0xFC
#define REFPACK_LITERAL_MASK 0x03
#define REFPACK_SHORT_LENGTH_MASK 0x1C
#define REFPACK_SHORT_OFFSET_MASK 0x60
#define REFPACK_MEDIUM_LENGTH_MASK 0x3F
#define REFPACK_MEDIUM_OFFSET_MASK 0x3F
#define REFPACK_LONG_LENGTH_MASK 0x0C
#define REFPACK_LONG_OFFSET_MASK 0x10
#define REFPACK_RUN_LENGTH_MASK 0x1F
#define REFPACK_SHORT_OFFSET_SHIFT 3
#define REFPACK_SHORT_LENGTH_SHIFT 2
#define REFPACK_MEDIUM_OFFSET_SHIFT 8
#define REFPACK_MEDIUM_LITERAL_SHIFT 6
#define REFPACK_LONG_OFFSET_HIGH_SHIFT 12
#define REFPACK_LONG_OFFSET_MIDDLE_SHIFT 8
#define REFPACK_LONG_LENGTH_SHIFT 6
#define REFPACK_RUN_LENGTH_SHIFT 2
#define REFPACK_SHORT_LENGTH_BASE 3
#define REFPACK_MEDIUM_LENGTH_BASE 4
#define REFPACK_LONG_LENGTH_BASE 5
#define REFPACK_OFFSET_BASE 1
#define REFPACK_HEADER_BYTES 2
#define REFPACK_OPTIONAL_BYTES 3
#define REFPACK_SIZE_BYTES 3
#define BYTE_PAIR_TAG_COMPACT 0x46
#define BYTE_PAIR_TAG_EXTENDED 0x47
#define BYTE_PAIR_TAG_SUFFIX 0xFB
#define BYTE_PAIR_TABLE_ENTRIES 256
#define BYTE_PAIR_LITERAL 0
#define BYTE_PAIR_SUBSTITUTION 1
#define BYTE_PAIR_ESCAPE 2
#define BYTE_PAIR_HEADER_BYTES 2
#define BYTE_PAIR_EXTENDED_BYTES 3
#define BYTE_PAIR_SIZE_BYTES 3
#define PACKED_DECODE_CALLBACK_INDEX 6
#define PACKED_DECODE_MARKER_OFFSET 1
#define PACKED_DECODE_MARKER 0xFB
#define PACKED_DECODE_TYPE_MASK 0xFE
#define PACKED_DECODE_RAW_OFFSET 5
#define PACKED_DECODE_TYPE_REFPACK 0x10
#define PACKED_DECODE_TYPE_HUFFMAN_1 0x30
#define PACKED_DECODE_TYPE_HUFFMAN_2 0x32
#define PACKED_DECODE_TYPE_HUFFMAN_3 0x34
#define PACKED_DECODE_TYPE_BYTE_PAIR 0x46
#define PACKED_DECODE_TYPE_DELTA_1 0x60
#define PACKED_DECODE_TYPE_DELTA_2 0x62
#define PACKED_DECODE_TYPE_DELTA_3 0x66
#define PACKED_DECODE_TYPE_RAW_1 0x6A
#define PACKED_DECODE_TYPE_RAW_2 0x6E
#define PACKED_DECODE_TYPE_DELTA_4 0x72
#define PACKED_DECODE_TYPE_RLE_1 0x7A
#define PACKED_DECODE_TYPE_RLE_2 0x7B

const char G_INVALID_PACK_CODE_MESSAGE[INVALID_PACK_CODE_MESSAGE_BYTES] =
  "unpack - INVALID PACK CODE (%x)\n";

int32
decode_delta_bytes(const uint8 *source_, uint8 *destination_)
{
  const uint8 *payload;
  uint32       output_bytes;
  uint32       output_index;
  uint32       tag;
  uint8        accumulator;

  if(source_ == NULL)
    return 0;

  tag     = (((uint32) source_[0] << DELTA_LENGTH_MIDDLE_SHIFT) | (uint32) source_[1]);
  payload = source_ + DELTA_DEFAULT_HEADER_BYTES;
  if(tag == DELTA_TAG_COMPACT)
    payload = source_ + DELTA_COMPACT_HEADER_BYTES;
  else if(tag == DELTA_TAG_EXTENDED)
    payload = source_ + DELTA_EXTENDED_HEADER_BYTES;

  output_bytes = (((uint32) payload[DELTA_LENGTH_HIGH_BYTE] << DELTA_LENGTH_HIGH_SHIFT) |
                  ((uint32) payload[DELTA_LENGTH_MIDDLE_BYTE] << DELTA_LENGTH_MIDDLE_SHIFT) |
                  (uint32) payload[DELTA_LENGTH_LOW_BYTE]);
  payload += DELTA_LENGTH_BYTES;
  accumulator = 0;
  for(output_index = 0; output_index < output_bytes; output_index++)
    {
      accumulator                = (uint8) (accumulator + payload[output_index]);
      destination_[output_index] = accumulator;
    }

  return (int32) output_bytes;
}

int32
decode_run_length_blocks(const uint8 *source_, uint8 *destination_)
{
  const uint8 *block;
  const uint8 *payload;
  uint32       block_bytes;
  uint32       block_index;
  uint32       control_index;
  uint32       control_bytes;
  uint32       copy_bytes;
  uint32       output_bytes;
  uint32       repeat_count;
  uint32       tag;
  int32        control;

  if(source_ == NULL)
    return 0;

  tag     = (((uint32) source_[0] << DELTA_LENGTH_MIDDLE_SHIFT) | (uint32) source_[1]);
  payload = source_ + RLE_DEFAULT_HEADER_BYTES;
  if(tag == RLE_TAG_EXTENDED)
    payload = source_ + RLE_EXTENDED_HEADER_BYTES;

  output_bytes  = (((uint32) payload[DELTA_LENGTH_HIGH_BYTE] << DELTA_LENGTH_HIGH_SHIFT) |
                   ((uint32) payload[DELTA_LENGTH_MIDDLE_BYTE] << DELTA_LENGTH_MIDDLE_SHIFT) |
                   (uint32) payload[DELTA_LENGTH_LOW_BYTE]);
  block_bytes   = payload[RLE_BLOCK_WIDTH_BYTE];
  control_bytes = payload[RLE_CONTROL_WIDTH_BYTE];
  payload += RLE_STREAM_HEADER_BYTES;

  do
    {
      control = (int32) (int8) *payload++;
      for(control_index = 1; control_index < control_bytes; control_index++)
        control = (int32) (((uint32) control << DELTA_LENGTH_MIDDLE_SHIFT) | (uint32) *payload++);

      if(control < 0)
        {
          copy_bytes = (block_bytes * (0 - (uint32) control));
          while(copy_bytes != 0)
            {
              *destination_++ = *payload++;
              copy_bytes--;
            }
        }
      else if(control != 0)
        {
          block        = payload;
          repeat_count = ((uint32) control + 1);
          while(repeat_count != 0)
            {
              for(block_index = 0; block_index < block_bytes; block_index++)
                *destination_++ = block[block_index];
              repeat_count--;
            }
          payload += block_bytes;
        }
    }
  while(control != 0);

  return (int32) output_bytes;
}

int32
decode_ref_pack_bytes(const uint8 *source_, uint8 *destination_, int32 decode_)
{
  const uint8 *copy_source;
  uint32       control;
  uint32       copy_bytes;
  uint32       literal_bytes;
  uint32       offset;
  uint32       output_bytes;
  uint32       stream_byte;

  if(source_ == NULL)
    return 0;

  control = source_[0];
  source_ += REFPACK_HEADER_BYTES;
  if((control & REFPACK_OPTIONAL_HEADER) != 0)
    source_ += REFPACK_OPTIONAL_BYTES;

  output_bytes = (((uint32) source_[DELTA_LENGTH_HIGH_BYTE] << REFPACK_SIZE_HIGH_SHIFT) |
                  ((uint32) source_[DELTA_LENGTH_MIDDLE_BYTE] << REFPACK_SIZE_MIDDLE_SHIFT) |
                  (uint32) source_[DELTA_LENGTH_LOW_BYTE]);
  source_ += REFPACK_SIZE_BYTES;
  if(decode_ == 0)
    return (int32) output_bytes;

  for(;;)
    {
      control = *source_++;
      if(control < REFPACK_TOKEN_MEDIUM)
        {
          stream_byte   = *source_++;
          literal_bytes = (control & REFPACK_LITERAL_MASK);
          while(literal_bytes != 0)
            {
              *destination_++ = *source_++;
              literal_bytes--;
            }

          offset =
            (stream_byte + ((control & REFPACK_SHORT_OFFSET_MASK) << REFPACK_SHORT_OFFSET_SHIFT) +
             REFPACK_OFFSET_BASE);
          copy_bytes = ((control & REFPACK_SHORT_LENGTH_MASK) >> REFPACK_SHORT_LENGTH_SHIFT) +
                       REFPACK_SHORT_LENGTH_BASE;
        }
      else if(control < REFPACK_TOKEN_LONG)
        {
          stream_byte = *source_++;
          offset = (((stream_byte & REFPACK_MEDIUM_OFFSET_MASK) << REFPACK_MEDIUM_OFFSET_SHIFT) |
                    (uint32) *source_++);
          offset += REFPACK_OFFSET_BASE;
          literal_bytes = (stream_byte >> REFPACK_MEDIUM_LITERAL_SHIFT);
          while(literal_bytes != 0)
            {
              *destination_++ = *source_++;
              literal_bytes--;
            }

          copy_bytes = (control & REFPACK_MEDIUM_LENGTH_MASK) + REFPACK_MEDIUM_LENGTH_BASE;
        }
      else if(control < REFPACK_TOKEN_LITERAL)
        {
          offset = ((control & REFPACK_LONG_OFFSET_MASK) << REFPACK_LONG_OFFSET_HIGH_SHIFT);
          offset |= ((uint32) *source_++ << REFPACK_LONG_OFFSET_MIDDLE_SHIFT);
          offset |= (uint32) *source_++;
          offset += REFPACK_OFFSET_BASE;
          stream_byte   = *source_++;
          literal_bytes = (control & REFPACK_LITERAL_MASK);
          while(literal_bytes != 0)
            {
              *destination_++ = *source_++;
              literal_bytes--;
            }

          copy_bytes =
            (((control & REFPACK_LONG_LENGTH_MASK) << REFPACK_LONG_LENGTH_SHIFT) | stream_byte);
          copy_bytes += REFPACK_LONG_LENGTH_BASE;
        }
      else
        {
          if(control >= REFPACK_TOKEN_END)
            {
              literal_bytes = (control & REFPACK_LITERAL_MASK);
              while(literal_bytes != 0)
                {
                  *destination_++ = *source_++;
                  literal_bytes--;
                }
              return (int32) output_bytes;
            }

          literal_bytes = ((control & REFPACK_RUN_LENGTH_MASK) + 1) << REFPACK_RUN_LENGTH_SHIFT;
          while(literal_bytes != 0)
            {
              *destination_++ = *source_++;
              literal_bytes--;
            }
          continue;
        }

      copy_source = destination_ - offset;
      while(copy_bytes != 0)
        {
          *destination_++ = *copy_source++;
          copy_bytes--;
        }
    }
}

static void
_expand_byte_pair_symbol(uint32       symbol_,
                         uint8      **destination_,
                         const uint8 *symbol_types_,
                         const uint8 *left_symbols_,
                         const uint8 *right_symbols_)
{
  if(symbol_types_[symbol_] >= BYTE_PAIR_SUBSTITUTION)
    {
      _expand_byte_pair_symbol(left_symbols_[symbol_],
                               destination_,
                               symbol_types_,
                               left_symbols_,
                               right_symbols_);
      _expand_byte_pair_symbol(right_symbols_[symbol_],
                               destination_,
                               symbol_types_,
                               left_symbols_,
                               right_symbols_);
    }
  else
    {
      *(*destination_)++ = (uint8) symbol_;
    }
}

int32
decode_byte_pair_bytes(const uint8 *source_, uint8 *destination_)
{
  uint8  left_symbols[BYTE_PAIR_TABLE_ENTRIES];
  uint8  right_symbols[BYTE_PAIR_TABLE_ENTRIES];
  uint8  symbol_types[BYTE_PAIR_TABLE_ENTRIES];
  uint32 entry_count;
  uint32 entry_index;
  uint32 output_bytes;
  uint32 symbol;
  uint32 tag;

  tag = *source_++;
  if(*source_++ != BYTE_PAIR_TAG_SUFFIX)
    return 0;
  if(tag == BYTE_PAIR_TAG_EXTENDED)
    source_ += BYTE_PAIR_EXTENDED_BYTES;
  else if(tag != BYTE_PAIR_TAG_COMPACT)
    return 0;

  output_bytes = (((uint32) source_[DELTA_LENGTH_HIGH_BYTE] << REFPACK_SIZE_HIGH_SHIFT) |
                  ((uint32) source_[DELTA_LENGTH_MIDDLE_BYTE] << REFPACK_SIZE_MIDDLE_SHIFT) |
                  (uint32) source_[DELTA_LENGTH_LOW_BYTE]);
  source_ += BYTE_PAIR_SIZE_BYTES;

  for(entry_index = 0; entry_index < BYTE_PAIR_TABLE_ENTRIES; entry_index++)
    symbol_types[entry_index] = BYTE_PAIR_LITERAL;

  symbol               = *source_++;
  symbol_types[symbol] = BYTE_PAIR_ESCAPE;
  entry_count          = (uint32) *source_++ + 1;
  for(entry_index = 0; entry_index < entry_count; entry_index++)
    {
      symbol                = *source_++;
      symbol_types[symbol]  = BYTE_PAIR_SUBSTITUTION;
      left_symbols[symbol]  = *source_++;
      right_symbols[symbol] = *source_++;
    }

  for(;;)
    {
      symbol = *source_++;
      if(symbol_types[symbol] == BYTE_PAIR_LITERAL)
        *destination_++ = (uint8) symbol;
      else if(symbol_types[symbol] == BYTE_PAIR_SUBSTITUTION)
        _expand_byte_pair_symbol(symbol, &destination_, symbol_types, left_symbols, right_symbols);
      else
        {
          symbol = *source_++;
          if(symbol == 0)
            return (int32) output_bytes;
          *destination_++ = (uint8) symbol;
        }
    }
}

int32
decode_packed_data(const uint8 *source_,
                   uint8       *destination_,
                   int32        source_size_,
                   int32        report_invalid_)
{
  const uint8 *header;
  int32        callback_result;
  int32        output_bytes;
  int32        result;
  uint32       type;

  callback_result = g_PACKED_CALLBACKS[PACKED_DECODE_CALLBACK_INDEX]();
  if(callback_result != 0)
    return callback_result;

  output_bytes = get_packed_data_size(source_, source_size_);
  header       = skip_copyright_preamble(source_);
  if(header[PACKED_DECODE_MARKER_OFFSET] == PACKED_DECODE_MARKER)
    {
      type = ((uint32) header[0] & PACKED_DECODE_TYPE_MASK);
      switch(type)
        {
          case PACKED_DECODE_TYPE_REFPACK:
            result = decode_ref_pack_bytes(header, destination_, 1);
            break;

          case PACKED_DECODE_TYPE_HUFFMAN_1:
          case PACKED_DECODE_TYPE_HUFFMAN_2:
          case PACKED_DECODE_TYPE_HUFFMAN_3:
            result = decode_huffman_packed_bytes(header, destination_, 1);
            break;

          case PACKED_DECODE_TYPE_BYTE_PAIR:
            result = decode_byte_pair_bytes(header, destination_);
            break;

          case PACKED_DECODE_TYPE_DELTA_1:
          case PACKED_DECODE_TYPE_DELTA_2:
          case PACKED_DECODE_TYPE_DELTA_3:
          case PACKED_DECODE_TYPE_DELTA_4:
            result = decode_delta_bytes(header, destination_);
            break;

          case PACKED_DECODE_TYPE_RAW_1:
          case PACKED_DECODE_TYPE_RAW_2:
            move_bytes(header + PACKED_DECODE_RAW_OFFSET, destination_, (uint32) output_bytes);
            return output_bytes;

          case PACKED_DECODE_TYPE_RLE_1:
          case PACKED_DECODE_TYPE_RLE_2:
            output_bytes = decode_run_length_blocks(header, destination_);
            result       = 0;
            break;

          default:
            if(report_invalid_ != 0)
              fatal_error(G_INVALID_PACK_CODE_MESSAGE, header[0]);
            result = 0;
            break;
        }

      if(result != 0)
        return result;
    }

  move_bytes(header, destination_, (uint32) output_bytes);
  return output_bytes;
}

int32
decode_packed_data_checked(const uint8 *source_, uint8 *destination_, int32 source_size_)
{
  return decode_packed_data(source_, destination_, source_size_, 1);
}

int32
decode_packed_data_quiet(const uint8 *source_, uint8 *destination_, int32 source_size_)
{
  return decode_packed_data(source_, destination_, source_size_, 0);
}
