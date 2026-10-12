#include "cel_access.h"

#include "asset_file_load.h"
#include "diagnostics.h"
#include "display_buffer_state.h"
#include "memory.h"

#include "hardware.h"

#define CEL_ALLOCATION_IDENTIFIER 0x43434275
#define CEL_DISPLAY_CENTER_X 160
#define CEL_DISPLAY_CENTER_Y 120
#define CEL_SIGN_BIT_SHIFT 31
#define CEL_FIXED_POSITION_SHIFT 16
#define CEL_CCB_CHUNK_TYPE 0x43434220
#define CEL_PIXEL_DATA_CHUNK_TYPE 0x50444154
#define CEL_PALETTE_CHUNK_TYPE 0x504C5554
#define CEL_CCB_DEFAULT_FLAGS 0x78200000
#define CEL_CCB_FLAG_LAST 0x40000000
#define CEL_CCB_FLAG_RELATIVE_SOURCE 0x10000000
#define CEL_CCB_FLAG_PREAMBLE_SOURCE 0x00400000
#define CEL_CCB_LINKED_FORMAT 6
#define CEL_CCB_FOLLOWING_FORMAT 3
#define CEL_LINKED_CCB_COUNT 2
#define CEL_FORMAT_MASK 7
#define CEL_CHUNK_HEADER_BYTES 8
#define CEL_CCB_CHUNK_HEADER_BYTES 12
#define CEL_DIMENSION_LOG2_SHIFT_16 16
#define CEL_DIMENSION_LOG2_SHIFT_8 8
#define CEL_DIMENSION_LOG2_SHIFT_4 4
#define CEL_DIMENSION_LOG2_SHIFT_2 2
#define CEL_DIMENSION_LOG2_SHIFT_1 1
#define CEL_ROW_ALIGNMENT_PADDING 31U
#define CEL_ROW_ALIGNMENT_MASK 0xFFFFFFE0U
#define CEL_MINIMUM_ROW_BYTES 8
#define CEL_BYTE_BITS 8
#define CEL_LRFORM_BITS_PER_PIXEL 16
#define CEL_LRFORM_PIXEL_ALIGNMENT_MASK (-2)

typedef struct CelAllocationHeader
{
  uint32 identifier;
  void  *allocation;
} CelAllocationHeader;

#define INVALID_CEL_ALLOCATION_MESSAGE                                                             \
  "unloadcel - ATTEMPT TO UNLOAD A CCB NOT RENDERED FROM MLOADCEL()\n"
#define LOAD_CEL_PARSE_ERROR_MESSAGE "loadcel - COULD NOT PARSE CEL FILE - %s\n"

uint8        g_CEL_BITS_PER_PIXEL[8] = { 0, 1, 2, 4, 6, 8, 16, 0 };
typedef char CelBitsPerPixelSize[(sizeof(g_CEL_BITS_PER_PIXEL) == 8) ? 1 : -1];
uint8        g_CEL_SOURCE_ALIGNMENT_MASKS[20] = { 0, 31, 15, 0, 7, 0, 15, 0, 3, 0,
                                                  0, 0,  0,  0, 0, 0, 1,  0, 0, 0 };
typedef char CelSourceAlignmentMasksSize[(sizeof(g_CEL_SOURCE_ALIGNMENT_MASKS) == 20) ? 1 : -1];

int32
unload_cel(CCB *cel_)
{
  CelAllocationHeader *header;

  if(cel_ == NULL)
    return 0;
  header = (CelAllocationHeader *) ((uint8 *) cel_ - sizeof(*header));
  if(header->identifier != CEL_ALLOCATION_IDENTIFIER)
    fatal_error(INVALID_CEL_ALLOCATION_MESSAGE);
  return free_memory(header->allocation);
}

int32
unload_cel_handle(CCB *cel_)
{
  return unload_cel(cel_);
}

uint32
get_cel_bits_per_pixel(const CCB *cel_)
{
  return g_CEL_BITS_PER_PIXEL[cel_->ccb_PRE0 & PRE0_BPP_MASK];
}

int32
get_cel_width(const CCB *cel_)
{
  return (int32) ((cel_->ccb_PRE1 & PRE1_TLHPCNT_MASK) + PRE1_TLHPCNT_PREFETCH);
}

int32
get_cel_height(const CCB *cel_)
{
  int32 height;

  height = (int32) (((cel_->ccb_PRE0 & PRE0_VCNT_MASK) >> PRE0_VCNT_SHIFT) + PRE0_VCNT_PREFETCH);
  if((cel_->ccb_PRE1 & PRE1_LRFORM) != 0)
    height <<= 1;

  return height;
}

CelData *
get_cel_source_pointer(const CCB *cel_)
{
  return cel_->ccb_SourcePtr;
}

void
clip_render_ccb_to_display(RenderCcb *ccb_)
{
  CCB   *cel;
  uint8 *source;
  int32  bits_per_pixel;
  int32  clip_amount;
  int32  height;
  int32  is_lr_form;
  int32  pixel_alignment_mask;
  int32  row_byte_count;
  uint32 row_bit_count;
  int32  source_byte_count;
  int32  source_pixel_count;
  int32  source_skip_pixels;
  int32  width;
  int32  x;
  int32  y;

  /* RenderCcb begins with the SDK CCB layout consumed by the cel engine. */
  cel = (CCB *) ccb_;
  if((cel->ccb_PRE0 & PRE0_LITERAL) != 0)
    return;

  bits_per_pixel       = (int32) get_cel_bits_per_pixel(cel);
  width                = get_cel_width(cel);
  height               = get_cel_height(cel);
  source               = (uint8 *) get_cel_source_pointer(cel);
  x                    = (cel->ccb_XPos >> CEL_FIXED_POSITION_SHIFT);
  y                    = (cel->ccb_YPos >> CEL_FIXED_POSITION_SHIFT);
  is_lr_form           = ((cel->ccb_PRE1 & PRE1_LRFORM) != 0);
  pixel_alignment_mask = -1;
  if(is_lr_form)
    {
      bits_per_pixel       = CEL_LRFORM_BITS_PER_PIXEL;
      pixel_alignment_mask = CEL_LRFORM_PIXEL_ALIGNMENT_MASK;
    }

  row_bit_count  = ((uint32) width * (uint32) bits_per_pixel);
  row_bit_count  = (row_bit_count + CEL_ROW_ALIGNMENT_PADDING) & CEL_ROW_ALIGNMENT_MASK;
  row_byte_count = (int32) (row_bit_count / CEL_BYTE_BITS);
  if(row_byte_count < CEL_MINIMUM_ROW_BYTES)
    row_byte_count = CEL_MINIMUM_ROW_BYTES;

  clip_amount =
    ((int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_Y_WORD] - y);
  if(clip_amount > 0)
    {
      clip_amount &= pixel_alignment_mask;
      source += (row_byte_count * clip_amount);
      height -= clip_amount;
      y += clip_amount;
    }

  clip_amount =
    ((y + height) -
     (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_HEIGHT_WORD]);
  if(clip_amount > 0)
    height -= (clip_amount & pixel_alignment_mask);

  clip_amount =
    ((x + width) -
     (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_WIDTH_WORD]);
  if(clip_amount > 0)
    width -= (clip_amount & pixel_alignment_mask);

  source_skip_pixels = 0;
  clip_amount =
    ((int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_X_WORD] - x);
  if(clip_amount > 0)
    {
      clip_amount &= pixel_alignment_mask;
      source_skip_pixels = (clip_amount & g_CEL_SOURCE_ALIGNMENT_MASKS[bits_per_pixel]);
      source_pixel_count = (clip_amount & ~(int32) g_CEL_SOURCE_ALIGNMENT_MASKS[bits_per_pixel]);
      source_byte_count  = (bits_per_pixel * source_pixel_count);
      if(is_lr_form)
        source_byte_count <<= 1;

      source += (source_byte_count / CEL_BYTE_BITS);
      width -= source_pixel_count;
      x += clip_amount;
    }

  if((width <= 0) || (height <= 0))
    {
      cel->ccb_Flags |= CCB_SKIP;
      return;
    }

  cel->ccb_SourcePtr = (CelData *) source;
  cel->ccb_XPos      = (int32) ((uint32) x << CEL_FIXED_POSITION_SHIFT);
  cel->ccb_YPos      = (int32) ((uint32) y << CEL_FIXED_POSITION_SHIFT);
  cel->ccb_PRE1 = ((cel->ccb_PRE1 & ~PRE1_TLHPCNT_MASK) | (uint32) (width - PRE1_TLHPCNT_PREFETCH));
  cel->ccb_PRE0 &= (PRE0_LITERAL | PRE0_BGND | PRE0_LINEAR | PRE0_REP8 | PRE0_BPP_MASK);
  if(is_lr_form)
    height >>= 1;

  cel->ccb_PRE0 |= ((uint32) (height - PRE0_VCNT_PREFETCH) << PRE0_VCNT_SHIFT) |
                   ((uint32) source_skip_pixels << PRE0_SKIPX_SHIFT);
}

CelChunkHeader *
advance_cel_chunk(uint32 *type_, CelChunkHeader **cursor_, int32 *remaining_byte_count_)
{
  CelChunkHeader *chunk;

  if(*remaining_byte_count_ <= 0)
    return NULL;

  chunk  = *cursor_;
  *type_ = chunk->type;
  *remaining_byte_count_ -= chunk->byte_count;
  *cursor_ = (CelChunkHeader *) ((uint8 *) chunk + chunk->byte_count);
  return chunk;
}

static uint32
_get_cel_format_word(const CCB *cel_)
{
  if((cel_->ccb_Flags & CEL_CCB_FLAG_PREAMBLE_SOURCE) != 0)
    return cel_->ccb_PRE0;
  if((cel_->ccb_Flags & CEL_CCB_FLAG_RELATIVE_SOURCE) != 0)
    return *(const uint32 *) cel_->ccb_SourcePtr;

  return *(const uint32 *) ((const uint8 *) cel_ + (uint32) cel_->ccb_SourcePtr +
                            CEL_CCB_CHUNK_HEADER_BYTES);
}

CCB *
parse_cel_data(void *data_, uint32 byte_count_, int32 report_error_)
{
  CelChunkHeader *chunk;
  CelChunkHeader *cursor;
  CCB            *cel;
  CCB            *first_cel;
  CCB            *previous_cel;
  int32           cel_count;
  int32           palette_count;
  int32           remaining_byte_count;
  uint32          chunk_type;

  (void) report_error_;
  cursor               = (CelChunkHeader *) data_;
  remaining_byte_count = (int32) byte_count_;
  first_cel            = NULL;
  previous_cel         = NULL;
  cel_count            = 0;
  palette_count        = 0;
  chunk                = advance_cel_chunk(&chunk_type, &cursor, &remaining_byte_count);
  while(chunk != NULL)
    {
      if(chunk_type == CEL_CCB_CHUNK_TYPE)
        {
          cel_count++;
          cel                = (CCB *) ((uint8 *) chunk + CEL_CCB_CHUNK_HEADER_BYTES);
          cel->ccb_NextPtr   = NULL;
          cel->ccb_SourcePtr = NULL;
          cel->ccb_PLUTPtr   = NULL;
          cel->ccb_Flags |= CEL_CCB_DEFAULT_FLAGS;
          if(first_cel == NULL)
            first_cel = cel;
          if(previous_cel != NULL)
            {
              if(previous_cel->ccb_SourcePtr == NULL)
                return NULL;
              previous_cel->ccb_NextPtr = cel;
              previous_cel->ccb_Flags &= ~CEL_CCB_FLAG_LAST;
            }
          previous_cel = cel;
        }
      else if(chunk_type == CEL_PIXEL_DATA_CHUNK_TYPE)
        {
          if((previous_cel == NULL) || (previous_cel->ccb_SourcePtr != NULL))
            return NULL;
          previous_cel->ccb_SourcePtr = (CelData *) ((uint8 *) chunk + CEL_CHUNK_HEADER_BYTES);
        }
      else if(chunk_type == CEL_PALETTE_CHUNK_TYPE)
        {
          palette_count++;
          if((previous_cel == NULL) || (previous_cel->ccb_PLUTPtr != NULL))
            return NULL;
          previous_cel->ccb_PLUTPtr = (void *) ((uint8 *) chunk + CEL_CCB_CHUNK_HEADER_BYTES);
        }

      chunk = advance_cel_chunk(&chunk_type, &cursor, &remaining_byte_count);
    }

  if((previous_cel == NULL) || (previous_cel->ccb_SourcePtr == NULL))
    return NULL;
  if((cel_count != CEL_LINKED_CCB_COUNT) || (palette_count != 1) ||
     (first_cel->ccb_Width != previous_cel->ccb_Width) ||
     (first_cel->ccb_Height != previous_cel->ccb_Height) ||
     (first_cel->ccb_XPos != previous_cel->ccb_XPos) ||
     (first_cel->ccb_YPos != previous_cel->ccb_YPos))
    {
      return first_cel;
    }

  if((_get_cel_format_word(first_cel) & CEL_FORMAT_MASK) != CEL_CCB_LINKED_FORMAT)
    return first_cel;
  if((_get_cel_format_word(previous_cel) & CEL_FORMAT_MASK) != CEL_CCB_FOLLOWING_FORMAT)
    return first_cel;

  previous_cel->ccb_Flags &= ~CEL_CCB_FLAG_LAST;
  previous_cel->ccb_NextPtr = first_cel;
  first_cel->ccb_NextPtr    = NULL;
  first_cel->ccb_Flags |= CEL_CCB_FLAG_LAST;
  return previous_cel;
}

CCB *
load_cel(const char *path_, uint32 memory_flags_, int32 report_error_)
{
  void                *data;
  CCB                 *cel;
  CelAllocationHeader *header;

  data = load_asset_file(path_, memory_flags_, report_error_);
  if(data != NULL)
    {
      cel = parse_cel_data(data, get_memory_payload_byte_count(data), report_error_);
      if(cel != NULL)
        {
          cel->ccb_XPos      = (CEL_DISPLAY_CENTER_X -
                                ((cel->ccb_Width + (cel->ccb_Width >> CEL_SIGN_BIT_SHIFT)) >> 1))
                            << CEL_FIXED_POSITION_SHIFT;
          cel->ccb_YPos      = (CEL_DISPLAY_CENTER_Y -
                                ((cel->ccb_Height + (cel->ccb_Height >> CEL_SIGN_BIT_SHIFT)) >> 1))
                            << CEL_FIXED_POSITION_SHIFT;
          header             = (CelAllocationHeader *) ((uint8 *) cel - sizeof(*header));
          header->identifier = CEL_ALLOCATION_IDENTIFIER;
          header->allocation = data;
          return cel;
        }
    }

  if(report_error_ != 0)
    fatal_error(LOAD_CEL_PARSE_ERROR_MESSAGE, path_);
  return NULL;
}

CCB *
load_cel_with_error(const char *path_, uint32 memory_flags_)
{
  return load_cel(path_, memory_flags_, 1);
}

CCB *
load_cel_quiet(const char *path_, uint32 memory_flags_)
{
  return load_cel(path_, memory_flags_, 0);
}

CCB *
load_cel_default(const char *path_, uint32 memory_flags_)
{
  return load_cel_with_error(path_, memory_flags_);
}

void
set_cel_dimension_log2(CCB *cel_)
{
  uint32 width;
  uint32 height;
  int32  width_log2;
  int32  height_log2;

  width       = (uint32) cel_->ccb_Width;
  height      = (uint32) cel_->ccb_Height;
  width_log2  = 0;
  height_log2 = 0;
  if((width >> CEL_DIMENSION_LOG2_SHIFT_16) != 0)
    {
      width >>= CEL_DIMENSION_LOG2_SHIFT_16;
      width_log2 = CEL_DIMENSION_LOG2_SHIFT_16;
    }
  if((width >> CEL_DIMENSION_LOG2_SHIFT_8) != 0)
    {
      width >>= CEL_DIMENSION_LOG2_SHIFT_8;
      width_log2 += CEL_DIMENSION_LOG2_SHIFT_8;
    }
  if((width >> CEL_DIMENSION_LOG2_SHIFT_4) != 0)
    {
      width >>= CEL_DIMENSION_LOG2_SHIFT_4;
      width_log2 += CEL_DIMENSION_LOG2_SHIFT_4;
    }
  if((width >> CEL_DIMENSION_LOG2_SHIFT_2) != 0)
    {
      width >>= CEL_DIMENSION_LOG2_SHIFT_2;
      width_log2 += CEL_DIMENSION_LOG2_SHIFT_2;
    }
  if((width >> CEL_DIMENSION_LOG2_SHIFT_1) != 0)
    width_log2 += CEL_DIMENSION_LOG2_SHIFT_1;
  if((height >> CEL_DIMENSION_LOG2_SHIFT_16) != 0)
    {
      height >>= CEL_DIMENSION_LOG2_SHIFT_16;
      height_log2 = CEL_DIMENSION_LOG2_SHIFT_16;
    }
  if((height >> CEL_DIMENSION_LOG2_SHIFT_8) != 0)
    {
      height >>= CEL_DIMENSION_LOG2_SHIFT_8;
      height_log2 += CEL_DIMENSION_LOG2_SHIFT_8;
    }
  if((height >> CEL_DIMENSION_LOG2_SHIFT_4) != 0)
    {
      height >>= CEL_DIMENSION_LOG2_SHIFT_4;
      height_log2 += CEL_DIMENSION_LOG2_SHIFT_4;
    }
  if((height >> CEL_DIMENSION_LOG2_SHIFT_2) != 0)
    {
      height >>= CEL_DIMENSION_LOG2_SHIFT_2;
      height_log2 += CEL_DIMENSION_LOG2_SHIFT_2;
    }
  if((height >> CEL_DIMENSION_LOG2_SHIFT_1) != 0)
    height_log2 += CEL_DIMENSION_LOG2_SHIFT_1;
  cel_->ccb_Width  = width_log2;
  cel_->ccb_Height = height_log2;
}
