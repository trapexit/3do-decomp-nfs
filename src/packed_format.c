#include "packed_format.h"

#include "binary_reader.h"
#include "string_utilities.h"

#define PACKED_FLAG_MASK 0xFE
#define PACKED_MARKER_BYTE 0xFB
#define PACKED_MARKER_OFFSET 1
#define PACKED_VARIANT_OFFSET 5
#define PACKED_SUBVARIANT_OFFSET 6
#define LEGACY_SIGNATURE_BYTE_2 2
#define LEGACY_SIGNATURE_BYTE_3 3
#define PACKED_VARIANT_ONE 1
#define PACKED_VARIANT_TWO 2
#define PACKED_SUBVARIANT_ONE 1
#define PACKED_TYPE_10 0x10
#define PACKED_TYPE_26 0x26
#define PACKED_TYPE_28 0x28
#define PACKED_TYPE_2A 0x2A
#define PACKED_TYPE_2C 0x2C
#define PACKED_TYPE_2E 0x2E
#define PACKED_TYPE_30 0x30
#define PACKED_TYPE_32 0x32
#define PACKED_TYPE_34 0x34
#define PACKED_TYPE_42 0x42
#define PACKED_TYPE_44 0x44
#define PACKED_TYPE_46 0x46
#define PACKED_TYPE_66 0x66
#define PACKED_TYPE_6A 0x6A
#define PACKED_TYPE_6E 0x6E
#define PACKED_TYPE_70 0x70
#define PACKED_TYPE_72 0x72
#define PACKED_TYPE_74 0x74
#define PACKED_TYPE_76 0x76
#define PACKED_TYPE_78 0x78
#define PACKED_TYPE_7A 0x7A
#define PACKED_TYPE_C0 0xC0
#define LEGACY_PACKED_SIGNATURE 0x000003F3
#define LEGACY_SIGNATURE_HIGH_SHIFT 24
#define LEGACY_SIGNATURE_MID_SHIFT 16
#define LEGACY_SIGNATURE_LOW_SHIFT 8
#define PACKED_SIZE_CALLBACK_INDEX 5
#define PACKED_SIZE_FIELD_OFFSET 2
#define PACKED_SIZE_FIELD_BYTES 3
#define PACKED_FORMAT_SIZE_MAX 30
#define PACKED_FIXED_SIZE_BYTES 6

PackedCallback g_PACKED_CALLBACKS[PACKED_CALLBACK_COUNT] = {
  packed_callback_default, packed_callback_default, packed_callback_default,
  packed_callback_default, packed_callback_default, packed_callback_default,
  packed_callback_default, packed_callback_default, packed_callback_default
};

int32
packed_callback_default(void)
{
  return 0;
}

int32
get_fixed_packed_data_size(void)
{
  return PACKED_FIXED_SIZE_BYTES;
}

PackedFormat
identify_packed_format(const uint8 *source_)
{
  const uint8 *header;
  uint32       legacy_signature;
  uint8        format_byte;
  uint8        variant;

  header = skip_copyright_preamble(source_);
  if(header[PACKED_MARKER_OFFSET] != PACKED_MARKER_BYTE)
    {
      legacy_signature = (((uint32) header[0] << LEGACY_SIGNATURE_HIGH_SHIFT) |
                          ((uint32) header[1] << LEGACY_SIGNATURE_MID_SHIFT) |
                          ((uint32) header[LEGACY_SIGNATURE_BYTE_2] << LEGACY_SIGNATURE_LOW_SHIFT) |
                          (uint32) header[LEGACY_SIGNATURE_BYTE_3]);
      if(legacy_signature == LEGACY_PACKED_SIGNATURE)
        return PACKED_FORMAT_LEGACY;
      return PACKED_FORMAT_UNKNOWN;
    }

  format_byte = (header[0] & PACKED_FLAG_MASK);
  switch(format_byte)
    {
      case PACKED_TYPE_10:
      case PACKED_TYPE_78:
        return PACKED_FORMAT_10_78;
      case PACKED_TYPE_26:
      case PACKED_TYPE_2A:
        return PACKED_FORMAT_26_2_A;
      case PACKED_TYPE_28:
      case PACKED_TYPE_2E:
        return PACKED_FORMAT_28_2_E;
      case PACKED_TYPE_2C:
        return PACKED_FORMAT_2_C;
      case PACKED_TYPE_30:
        return PACKED_FORMAT_30;
      case PACKED_TYPE_32:
        return PACKED_FORMAT_32;
      case PACKED_TYPE_34:
        return PACKED_FORMAT_34;
      case PACKED_TYPE_42:
      case PACKED_TYPE_44:
        return PACKED_FORMAT_42_44;
      case PACKED_TYPE_46:
        return PACKED_FORMAT_46;
      case PACKED_TYPE_66:
        return PACKED_FORMAT_66;
      case PACKED_TYPE_6A:
      case PACKED_TYPE_C0:
        return PACKED_FORMAT_6_A_C0;
      case PACKED_TYPE_6E:
        return PACKED_FORMAT_6_E;
      case PACKED_TYPE_70:
        return PACKED_FORMAT_70;
      case PACKED_TYPE_72:
        return PACKED_FORMAT_72;
      case PACKED_TYPE_74:
        return PACKED_FORMAT_74;
      case PACKED_TYPE_76:
        variant = header[PACKED_VARIANT_OFFSET];
        if((variant == PACKED_VARIANT_ONE) &&
           (header[PACKED_SUBVARIANT_OFFSET] == PACKED_SUBVARIANT_ONE))
          {
            return PACKED_FORMAT_76_VARIANT_1_1;
          }
        if((variant == PACKED_VARIANT_TWO) &&
           (header[PACKED_SUBVARIANT_OFFSET] == PACKED_SUBVARIANT_ONE))
          {
            return PACKED_FORMAT_76_VARIANT_2_1;
          }
        return PACKED_FORMAT_76;
      case PACKED_TYPE_7A:
        variant = header[PACKED_VARIANT_OFFSET];
        if((variant == PACKED_VARIANT_ONE) &&
           (header[PACKED_SUBVARIANT_OFFSET] == PACKED_SUBVARIANT_ONE))
          {
            return PACKED_FORMAT_7_A_VARIANT_1_1;
          }
        if((variant == PACKED_VARIANT_TWO) &&
           (header[PACKED_SUBVARIANT_OFFSET] == PACKED_SUBVARIANT_ONE))
          {
            return PACKED_FORMAT_7_A_VARIANT_2_1;
          }
        return PACKED_FORMAT_7_A;
      default:
        return PACKED_FORMAT_UNRECOGNIZED;
    }
}

int32
read_packed_data_size_field(const uint8 *source_)
{
  const uint8 *header;
  PackedFormat format;
  int32        result;

  result = 0;
  header = skip_copyright_preamble(source_);
  format = identify_packed_format(header);
  if((format > PACKED_FORMAT_UNKNOWN) && (format <= PACKED_FORMAT_SIZE_MAX))
    {
      result = (int32) read_big_endian_unsigned(header + PACKED_SIZE_FIELD_OFFSET,
                                                PACKED_SIZE_FIELD_BYTES);
    }

  return result;
}

int32
get_packed_data_size(const uint8 *source_, int32 source_size_)
{
  const uint8 *header;
  PackedFormat format;
  int32        callback_result;
  int32        preamble_size;

  if(source_size_ <= PACKED_SIZE_FIELD_OFFSET)
    return source_size_;

  callback_result = g_PACKED_CALLBACKS[PACKED_SIZE_CALLBACK_INDEX]();
  if(callback_result != 0)
    return callback_result;

  header        = skip_copyright_preamble(source_);
  preamble_size = (int32) (header - source_);
  source_size_ -= preamble_size;
  format = identify_packed_format(header);
  if((format <= PACKED_FORMAT_UNKNOWN) || (format > PACKED_FORMAT_SIZE_MAX))
    return source_size_;

  return (int32) read_big_endian_unsigned(header + PACKED_SIZE_FIELD_OFFSET,
                                          PACKED_SIZE_FIELD_BYTES);
}
