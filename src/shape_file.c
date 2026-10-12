#include "shape_file.h"

#include "asset_file_load.h"
#include "memory.h"
#include "packed_data_directory.h"
#include "path_resolution.h"
#include "shape_lookup.h"
#include "string_utilities.h"

#define SHAPE_FILE_PATH_BYTES 100
#define SHAPE_BYTE_BITS 8
#define SHAPE_HALF_WORD_BITS 16
#define SHAPE_WORD_HIGH_BITS 24
#define SHAPE_WORD_BYTE_TWO 2
#define SHAPE_WORD_BYTE_THREE 3
#define SHAPE_BYTE_ALIGNMENT_MASK 3U
#define SHAPE_SOURCE_HEADER_BYTES 6
#define SHAPE_SOURCE_ENTRY_COUNT_OFFSET 4
#define SHAPE_SOURCE_TABLE_ENTRY_BYTES 4
#define SHAPE_OUTPUT_HEADER_BYTES SHAPE_FILE_HEADER_BYTES
#define SHAPE_OUTPUT_DIRECTORY_BYTES 8
#define SHAPE_OUTPUT_ENTRY_HEADER_BYTES 16
#define SHAPE_OUTPUT_PALETTE_BYTES 0x50
#define SHAPE_OUTPUT_ENTRY_TYPE_IMAGE 5
#define SHAPE_OUTPUT_ENTRY_TYPE_SPECIAL 0x22
#define SHAPE_OUTPUT_OFFSET_TABLE_OFFSET 20
#define SHAPE_OUTPUT_ENTRY_OFFSET_OFFSET 4
#define SHAPE_OUTPUT_ENTRY_REMAINING_HIGH_OFFSET 1
#define SHAPE_OUTPUT_ENTRY_REMAINING_MID_OFFSET 2
#define SHAPE_OUTPUT_ENTRY_REMAINING_LOW_OFFSET 3
#define SHAPE_OUTPUT_ENTRY_COUNT_HIGH_OFFSET 10
#define SHAPE_OUTPUT_ENTRY_COUNT_LOW_OFFSET 11
#define SHAPE_ENTRY_WIDTH_OFFSET 0
#define SHAPE_ENTRY_HEIGHT_OFFSET 2
#define SHAPE_MIN_PIXEL_WIDTH 8
#define SHAPE_ENTRY_PIXELS_SOURCE_OFFSET 16
#define SHAPE_PALETTE_SOURCE_OFFSET 0x2B0
#define SHAPE_PALETTE_HEADER_BYTES 16
#define SHAPE_PALETTE_COLOR_COUNT 32
#define SHAPE_PALETTE_COLOR_BYTES 3
#define SHAPE_PALETTE_OUTPUT_COLOR_BYTES 2
#define SHAPE_PALETTE_FILL_BYTES 0x40
#define SHAPE_PALETTE_FILL_VALUE 0xFBFBFBFB
#define SHAPE_PALETTE_HEADER_TYPE 0x20
#define SHAPE_PALETTE_HEADER_FORMAT 2
#define SHAPE_COLOR_RED_SHIFT 10
#define SHAPE_COLOR_GREEN_SHIFT 5
#define SHAPE_ENTRY_DIMENSIONS_OFFSET 4
#define SHAPE_ENTRY_PIXELS_OFFSET 16
#define SHAPE_ENTRY_SKIP_MARKER 0x21
#define SHAPE_DIMENSION_BITS 12
#define SHAPE_DIMENSION_SIGN_SHIFT 20
#define PACKED_DIRECTORY_OFFSET_BYTES 4

static const char G_SHAPE_FILE_EXTENSIONS[]         = ".3sh"
                                                      ".vsh"
                                                      ".qvs"
                                                      ".q3s";
static const char G_CONVERTED_SHAPE_EXTENSION_VSH[] = ".vsh";
static const char G_CONVERTED_SHAPE_EXTENSION_QVS[] = ".qvs";
static const char G_CONVERTED_SHAPE_EXTENSION_PQS[] = ".pqs";
static const char G_SHAPE_PALETTE_ENTRY_TAG[]       = "!3d0";

typedef struct ShapePaletteHeader
{
  uint8 type;
  uint8 reserved_01;
  uint8 reserved_02;
  uint8 reserved_03;
  uint8 reserved_04;
  uint8 type_05;
  uint8 reserved_06;
  uint8 format;
  uint8 reserved_08;
  uint8 reserved_09;
  uint8 reserved_0_a;
  uint8 type_0_b;
  uint8 reserved_0_c;
  uint8 reserved_0_d;
  uint8 reserved_0_e;
  uint8 reserved_0_f;
} ShapePaletteHeader;

uint8 g_SHAPE_COLOR_TABLE[SHAPE_COLOR_TABLE_BYTES] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
  0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
  0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
  0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
  0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
  0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
  0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};
uint8 g_SHAPE_COLOR_TABLE_TAG[SHAPE_COLOR_TABLE_TAG_BYTES] = { '!', '3', 'x', '0' };
uint8 g_SHAPE_FILE_HEADER[SHAPE_OUTPUT_HEADER_BYTES] = { 'S', 'H', 'P', 'M', 0,   0,   0,   0,
                                                         0,   0,   0,   0,   '.', 'V', 'S', 'H' };

typedef char SizeAssertShapeColorTableSize[(sizeof(g_SHAPE_COLOR_TABLE) == 256) ? 1 : -1];
typedef char SizeAssertShapeFileHeaderSize[(sizeof(g_SHAPE_FILE_HEADER) == 16) ? 1 : -1];

void
load_shape_color_table(const uint8 *source_)
{
  move_bytes(source_, g_SHAPE_COLOR_TABLE, SHAPE_COLOR_TABLE_BYTES);
}

static uint16
_read_shape_little_endian_unsigned_short(const uint8 *source_)
{
  return (uint16) (((uint16) source_[1] << SHAPE_BYTE_BITS) | source_[0]);
}

static uint32
_read_shape_little_endian_unsigned(const uint8 *source_)
{
  return ((uint32) source_[SHAPE_WORD_BYTE_THREE] << SHAPE_WORD_HIGH_BITS) |
         ((uint32) source_[SHAPE_WORD_BYTE_TWO] << SHAPE_HALF_WORD_BITS) |
         ((uint32) source_[1] << SHAPE_BYTE_BITS) | source_[0];
}

int32
get_shape_conversion_size(const void *source_)
{
  const uint8 *source;
  const uint8 *entry_data;
  const uint8 *entry_tag;
  uint32       entry_count;
  uint32       entry_index;
  uint32       output_byte_count;
  uint32       palette_byte_count;
  uint32       pixel_byte_count;
  uint32       width;
  uint32       height;

  source      = (const uint8 *) source_;
  entry_count = _read_shape_little_endian_unsigned_short(source + SHAPE_SOURCE_ENTRY_COUNT_OFFSET);
  output_byte_count  = SHAPE_OUTPUT_HEADER_BYTES + (entry_count * SHAPE_OUTPUT_DIRECTORY_BYTES);
  palette_byte_count = 0;
  entry_index        = 0;
  while(entry_index < entry_count)
    {
      entry_tag =
        source + SHAPE_SOURCE_HEADER_BYTES + (entry_index * SHAPE_SOURCE_TABLE_ENTRY_BYTES);
      entry_data = source + SHAPE_SOURCE_HEADER_BYTES +
                   (entry_count * SHAPE_OUTPUT_DIRECTORY_BYTES) +
                   _read_shape_little_endian_unsigned(
                     source + SHAPE_SOURCE_HEADER_BYTES +
                     ((entry_count + entry_index) * SHAPE_SOURCE_TABLE_ENTRY_BYTES));
      if(compare_strings_limited(entry_tag,
                                 (const uint8 *) G_SHAPE_PALETTE_ENTRY_TAG,
                                 SHAPE_SOURCE_TABLE_ENTRY_BYTES) == 0)
        {
          palette_byte_count = SHAPE_OUTPUT_PALETTE_BYTES;
        }

      width  = _read_shape_little_endian_unsigned_short(entry_data + SHAPE_ENTRY_WIDTH_OFFSET);
      height = _read_shape_little_endian_unsigned_short(entry_data + SHAPE_ENTRY_HEIGHT_OFFSET);
      if(entry_tag[0] != SHAPE_ENTRY_SKIP_MARKER)
        {
          width = (width + SHAPE_BYTE_ALIGNMENT_MASK) & ~SHAPE_BYTE_ALIGNMENT_MASK;
          if(width < SHAPE_MIN_PIXEL_WIDTH)
            width = SHAPE_MIN_PIXEL_WIDTH;
        }
      pixel_byte_count = width * height;
      output_byte_count +=
        (pixel_byte_count + SHAPE_BYTE_ALIGNMENT_MASK) & ~SHAPE_BYTE_ALIGNMENT_MASK;
      output_byte_count += SHAPE_OUTPUT_ENTRY_HEADER_BYTES;
      entry_index++;
    }

  return output_byte_count + palette_byte_count;
}

static void
_copy_shape_entry_header(const uint8 *source_, uint8 *destination_, uint8 entry_type_)
{
  uint32 word_index;
  uint32 byte_offset;

  destination_[0]                     = entry_type_;
  destination_[1]                     = 0;
  destination_[SHAPE_WORD_BYTE_TWO]   = 0;
  destination_[SHAPE_WORD_BYTE_THREE] = 0;
  word_index                          = 1;
  while(word_index < SHAPE_SOURCE_TABLE_ENTRY_BYTES)
    {
      byte_offset                   = word_index * SHAPE_SOURCE_TABLE_ENTRY_BYTES;
      destination_[byte_offset]     = source_[byte_offset + 1];
      destination_[byte_offset + 1] = source_[byte_offset];
      destination_[byte_offset + SHAPE_WORD_BYTE_TWO] =
        source_[byte_offset + SHAPE_WORD_BYTE_THREE];
      destination_[byte_offset + SHAPE_WORD_BYTE_THREE] =
        source_[byte_offset + SHAPE_WORD_BYTE_TWO];
      word_index++;
    }
}

static void
_write_shape_palette_header(uint8 *destination_)
{
  ShapePaletteHeader *header;

  header               = (ShapePaletteHeader *) destination_;
  header->type         = SHAPE_PALETTE_HEADER_TYPE;
  header->reserved_01  = 0;
  header->reserved_02  = 0;
  header->reserved_03  = 0;
  header->reserved_04  = 0;
  header->type_05      = SHAPE_PALETTE_HEADER_TYPE;
  header->reserved_06  = 0;
  header->format       = SHAPE_PALETTE_HEADER_FORMAT;
  header->reserved_08  = 0;
  header->reserved_09  = 0;
  header->reserved_0_a = 0;
  header->type_0_b     = SHAPE_PALETTE_HEADER_TYPE;
  header->reserved_0_c = 0;
  header->reserved_0_d = 0;
  header->reserved_0_e = 0;
  header->reserved_0_f = 0;
}

static void
_convert_shape_palette(const uint8 *source_, uint8 *destination_)
{
  uint32 color_index;
  uint32 source_offset;
  uint16 color;

  fill_bytes(destination_, SHAPE_PALETTE_FILL_BYTES, SHAPE_PALETTE_FILL_VALUE);
  color_index = 0;
  while(color_index < SHAPE_PALETTE_COLOR_COUNT)
    {
      source_offset = color_index * SHAPE_PALETTE_COLOR_BYTES;
      color = (uint16) (((uint16) (source_[source_offset] >> 1) << SHAPE_COLOR_RED_SHIFT) |
                        ((uint16) (source_[source_offset + 1] >> 1) << SHAPE_COLOR_GREEN_SHIFT) |
                        (source_[source_offset + SHAPE_WORD_BYTE_TWO] >> 1));
      destination_[color_index * SHAPE_PALETTE_OUTPUT_COLOR_BYTES] =
        (uint8) (color >> SHAPE_BYTE_BITS);
      destination_[(color_index * SHAPE_PALETTE_OUTPUT_COLOR_BYTES) + 1] = (uint8) color;
      color_index++;
    }
}

void
convert_shape_file_data(const void *source_, void *destination_)
{
  const uint8 *source;
  const uint8 *entry_data;
  const uint8 *entry_tag;
  const uint8 *palette_source;
  uint8       *destination;
  uint8       *output_entry;
  uint8       *output_offset;
  uint32       entry_count;
  uint32       entry_index;
  uint32       output_byte_offset;
  uint32       palette_byte_count;
  uint32       pixel_byte_count;
  uint32       source_width;
  uint32       output_width;
  uint32       height;
  uint32       row_index;
  uint32       remaining_byte_count;

  source      = (const uint8 *) source_;
  destination = (uint8 *) destination_;
  move_bytes(g_SHAPE_FILE_HEADER, destination, SHAPE_OUTPUT_HEADER_BYTES);
  destination[SHAPE_OUTPUT_ENTRY_COUNT_LOW_OFFSET]  = source[SHAPE_SOURCE_ENTRY_COUNT_OFFSET];
  destination[SHAPE_OUTPUT_ENTRY_COUNT_HIGH_OFFSET] = source[SHAPE_SOURCE_ENTRY_COUNT_OFFSET + 1];
  entry_count = _read_shape_little_endian_unsigned_short(source + SHAPE_SOURCE_ENTRY_COUNT_OFFSET);
  palette_byte_count = 0;
  entry_index        = 0;
  while(entry_index < entry_count)
    {
      entry_tag =
        source + SHAPE_SOURCE_HEADER_BYTES + (entry_index * SHAPE_SOURCE_TABLE_ENTRY_BYTES);
      if(compare_strings_limited(entry_tag,
                                 (const uint8 *) G_SHAPE_PALETTE_ENTRY_TAG,
                                 SHAPE_SOURCE_TABLE_ENTRY_BYTES) == 0)
        {
          palette_byte_count = SHAPE_OUTPUT_PALETTE_BYTES;
        }
      move_bytes(entry_tag,
                 destination + SHAPE_OUTPUT_HEADER_BYTES +
                   (entry_index * SHAPE_OUTPUT_DIRECTORY_BYTES),
                 SHAPE_SOURCE_TABLE_ENTRY_BYTES);
      entry_index++;
    }

  output_byte_offset = SHAPE_OUTPUT_HEADER_BYTES + (entry_count * SHAPE_OUTPUT_DIRECTORY_BYTES);
  output_entry       = destination + output_byte_offset;
  output_offset      = destination + SHAPE_OUTPUT_OFFSET_TABLE_OFFSET;
  palette_source     = NULL;
  entry_index        = 0;
  while(entry_index < entry_count)
    {
      entry_data = source + SHAPE_SOURCE_HEADER_BYTES +
                   (entry_count * SHAPE_OUTPUT_DIRECTORY_BYTES) +
                   _read_shape_little_endian_unsigned(
                     source + SHAPE_SOURCE_HEADER_BYTES +
                     ((entry_count + entry_index) * SHAPE_SOURCE_TABLE_ENTRY_BYTES));
      entry_tag =
        source + SHAPE_SOURCE_HEADER_BYTES + (entry_index * SHAPE_SOURCE_TABLE_ENTRY_BYTES);
      if(compare_strings_limited(entry_tag,
                                 (const uint8 *) G_SHAPE_PALETTE_ENTRY_TAG,
                                 SHAPE_SOURCE_TABLE_ENTRY_BYTES) == 0)
        {
          palette_source = entry_data;
        }

      if(entry_tag[0] == SHAPE_ENTRY_SKIP_MARKER)
        _copy_shape_entry_header(entry_data, output_entry, SHAPE_OUTPUT_ENTRY_TYPE_SPECIAL);
      else
        _copy_shape_entry_header(entry_data, output_entry, SHAPE_OUTPUT_ENTRY_TYPE_IMAGE);

      source_width =
        _read_shape_little_endian_unsigned_short(entry_data + SHAPE_ENTRY_WIDTH_OFFSET);
      height = _read_shape_little_endian_unsigned_short(entry_data + SHAPE_ENTRY_HEIGHT_OFFSET);
      if(palette_byte_count != 0)
        {
          remaining_byte_count =
            get_shape_conversion_size(source) - SHAPE_OUTPUT_PALETTE_BYTES - output_byte_offset;
          output_entry[SHAPE_OUTPUT_ENTRY_REMAINING_HIGH_OFFSET] =
            (uint8) (remaining_byte_count >> SHAPE_HALF_WORD_BITS);
          output_entry[SHAPE_OUTPUT_ENTRY_REMAINING_MID_OFFSET] =
            (uint8) (remaining_byte_count >> SHAPE_BYTE_BITS);
          output_entry[SHAPE_OUTPUT_ENTRY_REMAINING_LOW_OFFSET] = (uint8) remaining_byte_count;
        }

      if(entry_tag[0] == SHAPE_ENTRY_SKIP_MARKER)
        {
          pixel_byte_count = source_width * height;
          move_bytes(entry_data + SHAPE_ENTRY_PIXELS_SOURCE_OFFSET,
                     output_entry + SHAPE_OUTPUT_ENTRY_HEADER_BYTES,
                     pixel_byte_count);
        }
      else
        {
          output_width = (source_width + SHAPE_BYTE_ALIGNMENT_MASK) & ~SHAPE_BYTE_ALIGNMENT_MASK;
          if(output_width < SHAPE_MIN_PIXEL_WIDTH)
            output_width = SHAPE_MIN_PIXEL_WIDTH;
          pixel_byte_count = output_width * height;
          clear_bytes(output_entry + SHAPE_OUTPUT_ENTRY_HEADER_BYTES, pixel_byte_count);
          row_index = 0;
          while(row_index < height)
            {
              move_bytes(entry_data + SHAPE_ENTRY_PIXELS_SOURCE_OFFSET + (row_index * source_width),
                         output_entry + SHAPE_OUTPUT_ENTRY_HEADER_BYTES +
                           (row_index * output_width),
                         source_width);
              row_index++;
            }
        }

      *(uint32 *) output_offset = output_byte_offset;
      output_offset += SHAPE_OUTPUT_DIRECTORY_BYTES;
      pixel_byte_count =
        (pixel_byte_count + SHAPE_BYTE_ALIGNMENT_MASK) & ~SHAPE_BYTE_ALIGNMENT_MASK;
      output_byte_offset += pixel_byte_count + SHAPE_OUTPUT_ENTRY_HEADER_BYTES;
      output_entry += pixel_byte_count + SHAPE_OUTPUT_ENTRY_HEADER_BYTES;
      entry_index++;
    }

  if(palette_byte_count != 0)
    {
      _write_shape_palette_header(output_entry);
      output_entry += SHAPE_PALETTE_HEADER_BYTES;
      _convert_shape_palette(palette_source + SHAPE_PALETTE_SOURCE_OFFSET, output_entry);
    }
}

static int32
_sign_extend_shape_dimension(uint32 value_)
{
  return ((int32) (value_ << SHAPE_DIMENSION_SIGN_SHIFT) >> SHAPE_DIMENSION_SIGN_SHIFT);
}

void
remap_bytes_with_table(uint8 *bytes_, const uint8 *table_, int32 byte_count_)
{
  while(byte_count_-- != 0)
    {
      *bytes_ = table_[*bytes_];
      bytes_++;
    }
}

static void
_remap_shape_directory_entry(uint8 *entry_)
{
  uint32 dimensions;
  int32  pixel_count;
  uint8 *pixels;

  dimensions = *(const uint32 *) (entry_ + SHAPE_ENTRY_DIMENSIONS_OFFSET);
  pixel_count =
    _sign_extend_shape_dimension(dimensions >> (SHAPE_DIMENSION_BITS + SHAPE_DIMENSION_BITS));
  pixel_count *= _sign_extend_shape_dimension(dimensions);
  pixels = entry_ + SHAPE_ENTRY_PIXELS_OFFSET;
  remap_bytes_with_table(pixels, g_SHAPE_COLOR_TABLE, pixel_count);
}

void
resolve_shape_file_directory(void *directory_)
{
  uint32       entry_count;
  uint32       entry_index;
  uint8        offset_bytes[PACKED_DIRECTORY_OFFSET_BYTES];
  const uint8 *entry;

  entry_index = 0;
  entry_count = get_packed_directory_entry_count(directory_);
  while(entry_index < entry_count)
    {
      write_packed_directory_entry_tag(directory_, entry_index, offset_bytes);
      if(offset_bytes[0] != SHAPE_ENTRY_SKIP_MARKER)
        {
          entry = get_packed_directory_entry(directory_, entry_index);
          _remap_shape_directory_entry((uint8 *) entry);
        }
      entry_index++;
      entry_count = get_packed_directory_entry_count(directory_);
    }
}

MemoryBlockDescriptor *
load_shape_file_record(const char *path_, uint32 memory_flags_, int32 report_error_)
{
  char                   resolved_path[SHAPE_FILE_PATH_BYTES];
  const char            *extension;
  MemoryBlockDescriptor *record;
  MemoryBlockDescriptor *source_record;
  void                  *color_shape;

  extension = find_path_with_extensions(path_, resolved_path, G_SHAPE_FILE_EXTENSIONS);
  record    = acquire_named_memory_record(resolved_path);
  if(record != NULL)
    return record;

  record = load_asset_file_record(resolved_path, memory_flags_, report_error_);
  if(record == NULL)
    return NULL;

  if((compare_strings_ignore_case(extension, G_CONVERTED_SHAPE_EXTENSION_VSH) != 0) &&
     (compare_strings_ignore_case(extension, G_CONVERTED_SHAPE_EXTENSION_QVS) != 0) &&
     (compare_strings_ignore_case(extension, G_CONVERTED_SHAPE_EXTENSION_PQS) != 0))
    {
      return record;
    }

  source_record = record;
  record        = allocate_memory_record_locked(resolved_path,
                                                get_shape_conversion_size(source_record->payload),
                                                memory_flags_,
                                                report_error_);
  convert_shape_file_data(source_record->payload, record->payload);
  free_memory_record_locked(source_record);

  color_shape = locate_shape(record->payload, (const uint32 *) g_SHAPE_COLOR_TABLE_TAG);
  if(color_shape != NULL)
    load_shape_color_table((const uint8 *) color_shape + SHAPE_ENTRY_PIXELS_OFFSET);
  resolve_shape_file_directory(record->payload);
  return record;
}

void *
load_shape_file_data(const char *path_, uint32 memory_flags_, int32 report_error_)
{
  MemoryBlockDescriptor *record;

  record = load_shape_file_record(path_, memory_flags_, report_error_);
  if(record == NULL)
    return NULL;

  return record->payload;
}

void *
load_shape_file_data_with_error(const char *path_, uint32 memory_flags_)
{
  return load_shape_file_data(path_, memory_flags_, 1);
}

void *
load_shape_file_data_quiet(const char *path_, uint32 memory_flags_)
{
  return load_shape_file_data(path_, memory_flags_, 0);
}

MemoryBlockDescriptor *
load_shape_file_record_alias(const char *path_, uint32 memory_flags_, int32 report_error_)
{
  return load_shape_file_record(path_, memory_flags_, report_error_);
}

MemoryBlockDescriptor *
load_shape_file_record_with_error(const char *path_, uint32 memory_flags_)
{
  return load_shape_file_record(path_, memory_flags_, 1);
}

MemoryBlockDescriptor *
load_shape_file_record_quiet(const char *path_, uint32 memory_flags_)
{
  return load_shape_file_record(path_, memory_flags_, 0);
}

void *
load_shape_file_data_alias(const char *path_, uint32 memory_flags_, int32 report_error_)
{
  return load_shape_file_data(path_, memory_flags_, report_error_);
}

void *
load_shape_file_data_with_error_alias(const char *path_, uint32 memory_flags_)
{
  return load_shape_file_data_with_error(path_, memory_flags_);
}

void *
load_shape_file_data_quiet_alias(const char *path_, uint32 memory_flags_)
{
  return load_shape_file_data_quiet(path_, memory_flags_);
}
