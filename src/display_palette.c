#include "display_palette.h"

#define RGB_RED_BYTE 0
#define RGB_GREEN_BYTE 1
#define RGB_BLUE_BYTE 2
#define RGB_BYTE_COUNT 3
#define RGBA_BYTE_COUNT 4
#define RGB555_RED_SHIFT 10
#define RGB555_GREEN_SHIFT 5
#define RGB555_BLUE_SHIFT 0
#define RGB555_COMPONENT_MASK 0x1F
#define RGB888_COMPONENT_TO_RGB555_SHIFT 3
#define RGB555_COMPONENT_TO_RGB888_SHIFT 3
#define RGB6_COMPONENT_TO_RGB555_SHIFT 1
#define RGB555_COMPONENT_TO_RGB6_SHIFT 1
#define INTERLEAVED_PALETTE_ENTRY_STEP 2

typedef char DisplayPaletteUint16Size[(sizeof(uint16) == 2) ? 1 : -1];

/* The literal extent lets Needle prove the exact 512-byte oracle range. */
uint16 g_DISPLAY_RG_B555_PALETTE[256] = { 0 };

/* Packs three adjacent component bytes after reducing their precision. */
static uint16
_pack_rgb555(const uint8 *rgb_, int32 component_shift_);

/* Expands one RGB555 value into three adjacent component bytes. */
static void
_unpack_rgb555(uint8 *rgb_, uint16 color_, int32 component_shift_);

void
initialize_palette_entries(int32 first_entry_, int32 entry_count_, const uint8 *rgba_entries_)
{
  int32 entry_offset;
  int32 entry_index;

  for(entry_index = 0; entry_index < entry_count_; ++entry_index)
    {
      entry_offset = (entry_index * RGBA_BYTE_COUNT);
      g_DISPLAY_RG_B555_PALETTE[first_entry_ + entry_index] =
        _pack_rgb555(&rgba_entries_[entry_offset], RGB888_COMPONENT_TO_RGB555_SHIFT);
    }
}

void
extract_interleaved_palette_entries_rgba(int32  first_entry_,
                                         int32  entry_count_,
                                         uint8 *rgba_entries_)
{
  int32  entry_offset;
  int32  entry_index;
  uint16 color;

  for(entry_index = 0; entry_index < entry_count_; entry_index += INTERLEAVED_PALETTE_ENTRY_STEP)
    {
      color        = g_DISPLAY_RG_B555_PALETTE[first_entry_ + entry_index];
      entry_offset = (entry_index * RGBA_BYTE_COUNT);
      _unpack_rgb555(&rgba_entries_[entry_offset], color, RGB555_COMPONENT_TO_RGB888_SHIFT);
    }
}

void
convert_rgb6_palette_entries(int32        first_entry_,
                             int32        entry_count_,
                             const uint8 *rgb_entries_,
                             uint16      *destination_palette_)
{
  int32 entry_offset;
  int32 entry_index;

  for(entry_index = 0; entry_index < entry_count_; ++entry_index)
    {
      entry_offset = (entry_index * RGB_BYTE_COUNT);
      destination_palette_[first_entry_ + entry_index] =
        _pack_rgb555(&rgb_entries_[entry_offset], RGB6_COMPONENT_TO_RGB555_SHIFT);
    }
}

void
initialize_rgb6_palette_entries(int32 first_entry_, int32 entry_count_, const uint8 *rgb_entries_)
{
  convert_rgb6_palette_entries(first_entry_, entry_count_, rgb_entries_, g_DISPLAY_RG_B555_PALETTE);
}

void
extract_interleaved_palette_entries_rgb6(int32  first_entry_,
                                         int32  entry_count_,
                                         uint8 *rgb_entries_)
{
  int32  entry_offset;
  int32  entry_index;
  uint16 color;

  for(entry_index = 0; entry_index < entry_count_; entry_index += INTERLEAVED_PALETTE_ENTRY_STEP)
    {
      color        = g_DISPLAY_RG_B555_PALETTE[first_entry_ + entry_index];
      entry_offset = (entry_index * RGB_BYTE_COUNT);
      _unpack_rgb555(&rgb_entries_[entry_offset], color, RGB555_COMPONENT_TO_RGB6_SHIFT);
    }
}

static uint16
_pack_rgb555(const uint8 *rgb_, int32 component_shift_)
{
  uint32 red;
  uint32 green;
  uint32 blue;

  red   = ((uint32) rgb_[RGB_RED_BYTE] >> component_shift_);
  green = ((uint32) rgb_[RGB_GREEN_BYTE] >> component_shift_);
  blue  = ((uint32) rgb_[RGB_BLUE_BYTE] >> component_shift_);
  return (uint16) ((red << RGB555_RED_SHIFT) + (green << RGB555_GREEN_SHIFT) +
                   (blue << RGB555_BLUE_SHIFT));
}

static void
_unpack_rgb555(uint8 *rgb_, uint16 color_, int32 component_shift_)
{
  rgb_[RGB_RED_BYTE] =
    (uint8) ((((uint32) color_ >> RGB555_RED_SHIFT) & RGB555_COMPONENT_MASK) << component_shift_);
  rgb_[RGB_GREEN_BYTE] =
    (uint8) ((((uint32) color_ >> RGB555_GREEN_SHIFT) & RGB555_COMPONENT_MASK) << component_shift_);
  rgb_[RGB_BLUE_BYTE] =
    (uint8) ((((uint32) color_ >> RGB555_BLUE_SHIFT) & RGB555_COMPONENT_MASK) << component_shift_);
}
