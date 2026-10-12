#include "text_render_state.h"

#include "bitmap_pixel.h"
#include "ccb_manager.h"
#include "cel_render.h"
#include "default_memory_manager_callback.h"
#include "display_buffer_state.h"
#include "render_bounds.h"

#define FONT_FORMAT_BASE_CHARACTER '0'
#define FONT_FORMAT_BYTE_INDEX 3
#define FONT_FORMAT_ONE_BIT 1
#define FONT_FORMAT_FOUR_BIT 4
#define FONT_FORMAT_EIGHT_BIT 8
#define FONT_MAP_TAG 0x464E544D
#define FONT_PIXEL_CONTROL_DUAL 0xC381C301
#define FONT_PIXEL_CONTROL_SECONDARY 0x1F801F00
#define FONT_PIXEL_CONTROL_STANDARD 0x1F001F00
#define FONT_CHARACTER_INDEX_MASK 0xFF
#define FONT_OFFSET_HIGH_SHIFT 16
#define FONT_OFFSET_LOW_MASK 0xFFFF
#define TEXT_COLOR_COMPONENT_MASK 0x1F
#define TEXT_COLOR_BLUE_SHIFT 0
#define TEXT_COLOR_GREEN_SHIFT 5
#define TEXT_COLOR_RED_SHIFT 10
#define TEXT_COLOR_OPAQUE 0x8000
#define TEXT_COLOR_PALETTE_MASK 7U
#define TEXT_PALETTE_LAST_COLOR 15U
#define TEXT_PALETTE_SCALE_SHIFT 16
#define TEXT_PRIMARY_COMPONENT_BASE 3
#define TEXT_PRIMARY_COMPONENT_SHIFT 2
#define TEXT_PRIMARY_OPAQUE_BIT 8
#define FONT_ONE_BIT_ROW_BYTE_BIAS 7
#define FONT_ONE_BIT_ROW_BYTE_SHIFT 3
#define FONT_NIBBLE_BITS 4
#define FONT_NIBBLE_MASK 0x0F
#define FONT_ROW_NIBBLE_PIXELS 2
#define LEGACY_TEXT_FOREGROUND_COLOR 0x7FFF
#define LEGACY_TEXT_FORMAT_TAG 0x46334431
#define LEGACY_TEXT_FIRST_CHARACTER 0x20
#define LEGACY_TEXT_LAST_CHARACTER 0x8E
#define LEGACY_TEXT_DEFAULT_WIDTH 8
#define LEGACY_TEXT_DEFAULT_HEIGHT 8
#define LEGACY_TEXT_RESERVED_21 8
#define LEGACY_TEXT_RESERVED_22 7
#define LEGACY_TEXT_WIDTHS_CODE_OFFSET 0xA8
#define LEGACY_TEXT_GLYPH_OFFSETS_CODE_OFFSET 0x14

typedef struct TextGlyph
{
  int32 width;
  int32 height;
  int32 advance;
  int32 x;
  int32 y;
  int32 source_offset;
} TextGlyph;

typedef struct TextFontHeader
{
  uint32 format_tag;
  uint8  first_character;
  uint8  last_character;
  uint8  default_width;
  uint8  default_height;
  uint8  default_spacing;
  uint8  reserved_09;
  uint8  reserved_0a;
  uint8  reserved_0b;
  uint32 reserved_0c;
  uint32 width_offsets;
  uint32 height_advance_offsets;
  uint32 position_offsets;
  uint32 cel_header_offset;
} TextFontHeader;

typedef enum TextRenderStateWordIndex
{
  TEXT_RENDER_STATE_FONT_DATA_WORD,
  TEXT_RENDER_STATE_FOREGROUND_COLOR_WORD,
  TEXT_RENDER_STATE_BACKGROUND_COLOR_WORD,
  TEXT_RENDER_STATE_CURSOR_X_WORD,
  TEXT_RENDER_STATE_CURSOR_Y_WORD,
  TEXT_RENDER_STATE_DUAL_PALETTE_WORD,
  TEXT_RENDER_STATE_FORMAT_TAG_WORD,
  TEXT_RENDER_STATE_CHARACTER_METRICS_WORD,
  TEXT_RENDER_STATE_SPACING_WORD,
  TEXT_RENDER_STATE_WIDTHS_WORD,
  TEXT_RENDER_STATE_HEIGHTS_WORD,
  TEXT_RENDER_STATE_ADVANCES_WORD,
  TEXT_RENDER_STATE_X_OFFSETS_WORD,
  TEXT_RENDER_STATE_Y_OFFSETS_WORD,
  TEXT_RENDER_STATE_CEL_HEADER_WORD,
  TEXT_RENDER_STATE_GLYPH_OFFSETS_WORD
} TextRenderStateWordIndex;

typedef char TextRenderStateSize[(sizeof(TextRenderState) == 64) ? 1 : -1];
typedef char TextFontHeaderSize[(sizeof(TextFontHeader) == 32) ? 1 : -1];

/*
 * The original image initializes its dormant font pointers with relocations
 * into code.  Preserve those linker-relative aliases; a real font replaces
 * all three before text rendering begins.
 */
TextRenderState g_TEXT_RENDER_STATE = { (const uint8 *) default_memory_manager_callback,
                                        LEGACY_TEXT_FOREGROUND_COLOR,
                                        0,
                                        0,
                                        0,
                                        0,
                                        LEGACY_TEXT_FORMAT_TAG,
                                        LEGACY_TEXT_FIRST_CHARACTER,
                                        LEGACY_TEXT_LAST_CHARACTER,
                                        LEGACY_TEXT_DEFAULT_WIDTH,
                                        LEGACY_TEXT_DEFAULT_HEIGHT,
                                        0,
                                        LEGACY_TEXT_RESERVED_21,
                                        LEGACY_TEXT_RESERVED_22,
                                        0,
                                        ((const uint8 *) project_point_with_division) +
                                          LEGACY_TEXT_WIDTHS_CODE_OFFSET,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL,
                                        (const int32 *) (((const uint8 *) configure_render_bounds) +
                                                         LEGACY_TEXT_GLYPH_OFFSETS_CODE_OFFSET) };

int32   g_TEXT_PALETTE_BANK_INDEX = 0;
uint16 *g_TEXT_PRIMARY_PALETTE    = NULL;
uint16 *g_TEXT_SECONDARY_PALETTE  = NULL;
uint16  g_TEXT_PRIMARY_PALETTE_STORAGE[TEXT_PALETTE_COLOR_COUNT];
uint16  g_TEXT_COLOR_PALETTE_BANKS[TEXT_COLOR_PALETTE_COUNT][TEXT_PALETTE_COLOR_COUNT];

static uint16
_make_scaled_text_color(uint32 red_, uint32 green_, uint32 blue_, int32 color_index_);

static uint16
_make_primary_text_palette_color(int32 color_index_);

static const uint8 *
_resolve_optional_font_offset(const uint8 *font_data_, uint32 offset_);

void
draw_text(const char *text_)
{
  TextGlyph    glyph;
  const uint8 *source;
  uint16      *saved_palette;
  int32        character_count;
  int32        format;
  int32        index;
  int32        pixel_x;
  int32        row;
  int32        y;
  int32        remaining;
  uint32       color;
  uint32       pixel_color;
  uint32       saved_pixel_control;

  character_count =
    ((int32) g_TEXT_RENDER_STATE.last_character - (int32) g_TEXT_RENDER_STATE.first_character + 1);
  format =
    ((int32) g_TEXT_RENDER_STATE.font_data[FONT_FORMAT_BYTE_INDEX] - FONT_FORMAT_BASE_CHARACTER);
  color               = g_TEXT_RENDER_STATE.foreground_color;
  saved_pixel_control = 0;
  if(color == 0)
    {
      color               = 1;
      saved_pixel_control = get_ccb_pixel_control();
      set_ccb_pixel_control(saved_pixel_control | 1);
    }

  g_BITMAP_TEXT_PALETTE[0] = 0;
  g_BITMAP_TEXT_PALETTE[1] = (uint16) color;
  while(*text_ != '\0')
    {
      index = ((int32) (uint8) *text_ - (int32) g_TEXT_RENDER_STATE.first_character);
      index &= FONT_CHARACTER_INDEX_MASK;
      if(index < character_count)
        {
          if(g_TEXT_RENDER_STATE.widths == NULL)
            glyph.width = g_TEXT_RENDER_STATE.default_width;
          else
            glyph.width = g_TEXT_RENDER_STATE.widths[index];
          if(g_TEXT_RENDER_STATE.heights == NULL)
            glyph.height = g_TEXT_RENDER_STATE.default_height;
          else
            glyph.height = g_TEXT_RENDER_STATE.heights[index];
          if(g_TEXT_RENDER_STATE.advances == NULL)
            {
              glyph.advance = (glyph.width + g_TEXT_RENDER_STATE.default_spacing);
            }
          else
            glyph.advance = g_TEXT_RENDER_STATE.advances[index];
          if(g_TEXT_RENDER_STATE.x_offsets == NULL)
            glyph.x = g_TEXT_RENDER_STATE.cursor_x;
          else
            {
              glyph.x = (g_TEXT_RENDER_STATE.cursor_x + g_TEXT_RENDER_STATE.x_offsets[index]);
            }
          if(g_TEXT_RENDER_STATE.y_offsets == NULL)
            glyph.y = g_TEXT_RENDER_STATE.cursor_y;
          else
            {
              glyph.y = (g_TEXT_RENDER_STATE.cursor_y + g_TEXT_RENDER_STATE.y_offsets[index]);
            }
          glyph.source_offset = g_TEXT_RENDER_STATE.glyph_offsets[index];

          if(g_TEXT_RENDER_STATE.format_tag == FONT_MAP_TAG)
            {
              if((glyph.width > 0) && (glyph.height > 0))
                {
                  configure_display_buffer_descriptor_snapshot(glyph.x,
                                                               glyph.x + glyph.width,
                                                               glyph.y,
                                                               glyph.y + glyph.height);
                  if(g_TEXT_RENDER_STATE.dual_palette_enabled != 0)
                    {
                      saved_palette = g_CCB_TEMPLATE.palette;
                      set_ccb_pixel_control(FONT_PIXEL_CONTROL_DUAL);
                      g_CCB_TEMPLATE.palette = g_TEXT_PRIMARY_PALETTE;
                      (void) draw_cel_header(g_TEXT_RENDER_STATE.cel_header,
                                             glyph.x - glyph.source_offset,
                                             glyph.y);
                      if(g_CCB_MANAGER_STATE.flags != 0)
                        {
                          (void) allocate_previous_ccb_prefix(g_TEXT_SECONDARY_PALETTE,
                                                              FONT_PIXEL_CONTROL_SECONDARY);
                        }
                      else
                        {
                          set_ccb_pixel_control(FONT_PIXEL_CONTROL_SECONDARY);
                          g_CCB_TEMPLATE.palette = g_TEXT_SECONDARY_PALETTE;
                          (void) draw_cel_header(g_TEXT_RENDER_STATE.cel_header,
                                                 glyph.x - glyph.source_offset,
                                                 glyph.y);
                        }
                      set_ccb_pixel_control(FONT_PIXEL_CONTROL_STANDARD);
                      g_CCB_TEMPLATE.palette = saved_palette;
                    }
                  else
                    {
                      (void) draw_cel_header(g_TEXT_RENDER_STATE.cel_header,
                                             glyph.x - glyph.source_offset,
                                             glyph.y);
                    }

                  configure_display_buffer_descriptor_snapshot(
                    0,
                    (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[0],
                    0,
                    (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[1]);
                }
            }
          else if((format == FONT_FORMAT_ONE_BIT) || (format == FONT_FORMAT_FOUR_BIT) ||
                  (format == FONT_FORMAT_EIGHT_BIT))
            {
              source = (g_TEXT_RENDER_STATE.font_data + glyph.source_offset);
              y      = glyph.y;
              for(row = 0; row < glyph.height; ++row)
                {
                  if((y < (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT
                            .words[DISPLAY_BUFFER_DESCRIPTOR_Y_WORD]) ||
                     (y >= (int32) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT
                             .words[DISPLAY_BUFFER_DESCRIPTOR_HEIGHT_WORD]))
                    {
                      if(format == FONT_FORMAT_ONE_BIT)
                        {
                          source += ((glyph.width + FONT_ONE_BIT_ROW_BYTE_BIAS) >>
                                     FONT_ONE_BIT_ROW_BYTE_SHIFT);
                        }
                      else if(format == FONT_FORMAT_FOUR_BIT)
                        source += ((glyph.width + 1) >> 1);
                      else
                        source += glyph.width;
                    }
                  else if(format == FONT_FORMAT_ONE_BIT)
                    {
                      source = draw_1_bit_bitmap_row(glyph.x, y, source, glyph.width);
                    }
                  else if(format == FONT_FORMAT_FOUR_BIT)
                    {
                      remaining = glyph.width;
                      pixel_x   = glyph.x;
                      while(remaining > 0)
                        {
                          pixel_color = ((uint32) *source >> FONT_NIBBLE_BITS);
                          if(pixel_color != 0)
                            draw_clipped_bitmap_pixel(pixel_x, y, pixel_color);
                          pixel_x++;
                          pixel_color = ((uint32) *source & FONT_NIBBLE_MASK);
                          if(pixel_color != 0)
                            draw_clipped_bitmap_pixel(pixel_x, y, pixel_color);
                          pixel_x++;
                          source++;
                          remaining -= FONT_ROW_NIBBLE_PIXELS;
                        }
                    }
                  else
                    {
                      remaining = glyph.width;
                      pixel_x   = glyph.x;
                      while(remaining > 0)
                        {
                          pixel_color = *source;
                          if(pixel_color != 0)
                            draw_clipped_bitmap_pixel(pixel_x, y, pixel_color);
                          pixel_x++;
                          source++;
                          remaining--;
                        }
                    }
                  y++;
                }
            }

          g_TEXT_RENDER_STATE.cursor_x += glyph.advance;
        }

      text_++;
    }

  if(saved_pixel_control != 0)
    set_ccb_pixel_control(saved_pixel_control);
}

void
draw_text_at(const char *text_, int32 cursor_x_, int32 cursor_y_)
{
  set_text_render_position(cursor_x_, cursor_y_);
  draw_text(text_);
}

void
get_text_render_state(TextRenderState *state_)
{
  const uint32 *source;
  uint32       *destination;

  source                                        = (const uint32 *) &g_TEXT_RENDER_STATE;
  destination                                   = (uint32 *) state_;
  destination[TEXT_RENDER_STATE_FONT_DATA_WORD] = source[TEXT_RENDER_STATE_FONT_DATA_WORD];
  destination[TEXT_RENDER_STATE_FOREGROUND_COLOR_WORD] =
    source[TEXT_RENDER_STATE_FOREGROUND_COLOR_WORD];
  destination[TEXT_RENDER_STATE_BACKGROUND_COLOR_WORD] =
    source[TEXT_RENDER_STATE_BACKGROUND_COLOR_WORD];
  destination[TEXT_RENDER_STATE_CURSOR_X_WORD]     = source[TEXT_RENDER_STATE_CURSOR_X_WORD];
  destination[TEXT_RENDER_STATE_CURSOR_Y_WORD]     = source[TEXT_RENDER_STATE_CURSOR_Y_WORD];
  destination[TEXT_RENDER_STATE_DUAL_PALETTE_WORD] = source[TEXT_RENDER_STATE_DUAL_PALETTE_WORD];
  destination[TEXT_RENDER_STATE_FORMAT_TAG_WORD]   = source[TEXT_RENDER_STATE_FORMAT_TAG_WORD];
  destination[TEXT_RENDER_STATE_CHARACTER_METRICS_WORD] =
    source[TEXT_RENDER_STATE_CHARACTER_METRICS_WORD];
  destination[TEXT_RENDER_STATE_SPACING_WORD]       = source[TEXT_RENDER_STATE_SPACING_WORD];
  destination[TEXT_RENDER_STATE_WIDTHS_WORD]        = source[TEXT_RENDER_STATE_WIDTHS_WORD];
  destination[TEXT_RENDER_STATE_HEIGHTS_WORD]       = source[TEXT_RENDER_STATE_HEIGHTS_WORD];
  destination[TEXT_RENDER_STATE_ADVANCES_WORD]      = source[TEXT_RENDER_STATE_ADVANCES_WORD];
  destination[TEXT_RENDER_STATE_X_OFFSETS_WORD]     = source[TEXT_RENDER_STATE_X_OFFSETS_WORD];
  destination[TEXT_RENDER_STATE_Y_OFFSETS_WORD]     = source[TEXT_RENDER_STATE_Y_OFFSETS_WORD];
  destination[TEXT_RENDER_STATE_CEL_HEADER_WORD]    = source[TEXT_RENDER_STATE_CEL_HEADER_WORD];
  destination[TEXT_RENDER_STATE_GLYPH_OFFSETS_WORD] = source[TEXT_RENDER_STATE_GLYPH_OFFSETS_WORD];
}

void
set_text_render_state(const TextRenderState *state_)
{
  g_TEXT_RENDER_STATE = *state_;
}

void
initialize_text_render_font(const void *font_data_)
{
  const TextFontHeader *header;
  const uint8          *font_data;
  uint32                offsets;

  header                                   = (const TextFontHeader *) font_data_;
  font_data                                = (const uint8 *) font_data_;
  g_TEXT_RENDER_STATE.font_data            = font_data;
  g_TEXT_RENDER_STATE.format_tag           = header->format_tag;
  g_TEXT_RENDER_STATE.first_character      = header->first_character;
  g_TEXT_RENDER_STATE.last_character       = header->last_character;
  g_TEXT_RENDER_STATE.default_width        = header->default_width;
  g_TEXT_RENDER_STATE.default_height       = header->default_height;
  g_TEXT_RENDER_STATE.default_spacing      = header->default_spacing;
  g_TEXT_RENDER_STATE.reserved_21          = header->reserved_09;
  g_TEXT_RENDER_STATE.reserved_22          = header->reserved_0a;
  g_TEXT_RENDER_STATE.dual_palette_enabled = 0;

  offsets = header->width_offsets;
  g_TEXT_RENDER_STATE.widths =
    _resolve_optional_font_offset(font_data, offsets & FONT_OFFSET_LOW_MASK);
  offsets = header->height_advance_offsets;
  g_TEXT_RENDER_STATE.heights =
    _resolve_optional_font_offset(font_data, offsets >> FONT_OFFSET_HIGH_SHIFT);
  g_TEXT_RENDER_STATE.advances =
    _resolve_optional_font_offset(font_data, offsets & FONT_OFFSET_LOW_MASK);
  offsets = header->position_offsets;
  g_TEXT_RENDER_STATE.x_offsets =
    _resolve_optional_font_offset(font_data, offsets >> FONT_OFFSET_HIGH_SHIFT);
  g_TEXT_RENDER_STATE.y_offsets =
    _resolve_optional_font_offset(font_data, offsets & FONT_OFFSET_LOW_MASK);
  g_TEXT_RENDER_STATE.cel_header    = (const uint32 *) (font_data + header->cel_header_offset);
  g_TEXT_RENDER_STATE.glyph_offsets = (const int32 *) (font_data + sizeof(TextFontHeader));
}

void
generate_dual_text_palettes(uint32 color_)
{
  uint32 red;
  uint32 green;
  uint32 blue;
  int32  color_index;
  int32  palette_index;

  red   = ((color_ >> TEXT_COLOR_RED_SHIFT) & TEXT_COLOR_COMPONENT_MASK);
  green = ((color_ >> TEXT_COLOR_GREEN_SHIFT) & TEXT_COLOR_COMPONENT_MASK);
  blue  = ((color_ >> TEXT_COLOR_BLUE_SHIFT) & TEXT_COLOR_COMPONENT_MASK);

  palette_index             = g_TEXT_PALETTE_BANK_INDEX;
  g_TEXT_SECONDARY_PALETTE  = g_TEXT_COLOR_PALETTE_BANKS[palette_index];
  g_TEXT_PALETTE_BANK_INDEX = (int32) (((uint32) palette_index + 1U) & TEXT_COLOR_PALETTE_MASK);
  g_TEXT_PRIMARY_PALETTE    = g_TEXT_PRIMARY_PALETTE_STORAGE;

  for(color_index = 0; color_index < TEXT_PALETTE_COLOR_COUNT; ++color_index)
    {
      g_TEXT_SECONDARY_PALETTE[color_index] =
        _make_scaled_text_color(red, green, blue, color_index);
      g_TEXT_PRIMARY_PALETTE[color_index] = _make_primary_text_palette_color(color_index);
    }

  g_TEXT_SECONDARY_PALETTE[0] = 0;
}

void
set_text_render_colors(uint32 foreground_color_, uint32 background_color_)
{
  g_TEXT_RENDER_STATE.background_color = background_color_;
  g_TEXT_RENDER_STATE.foreground_color = foreground_color_;
  if(g_TEXT_RENDER_STATE.dual_palette_enabled != 0)
    generate_dual_text_palettes(foreground_color_);
}

void
set_text_render_position(int32 cursor_x_, int32 cursor_y_)
{
  g_TEXT_RENDER_STATE.cursor_x = cursor_x_;
  g_TEXT_RENDER_STATE.cursor_y = cursor_y_;
}

static uint16
_make_scaled_text_color(uint32 red_, uint32 green_, uint32 blue_, int32 color_index_)
{
  int32  scale;
  uint32 red;
  uint32 green;
  uint32 blue;

  scale = ((color_index_ << TEXT_PALETTE_SCALE_SHIFT) / TEXT_PALETTE_LAST_COLOR);
  red   = ((red_ * (uint32) scale) >> TEXT_PALETTE_SCALE_SHIFT);
  green = ((green_ * (uint32) scale) >> TEXT_PALETTE_SCALE_SHIFT);
  blue  = ((blue_ * (uint32) scale) >> TEXT_PALETTE_SCALE_SHIFT);

  return (uint16) (TEXT_COLOR_OPAQUE | (red << TEXT_COLOR_RED_SHIFT) |
                   (green << TEXT_COLOR_GREEN_SHIFT) | (blue << TEXT_COLOR_BLUE_SHIFT));
}

static uint16
_make_primary_text_palette_color(int32 color_index_)
{
  uint32 remaining;
  uint32 component;
  uint32 color;

  remaining = TEXT_PALETTE_LAST_COLOR - (uint32) color_index_;
  component = (TEXT_PRIMARY_COMPONENT_BASE |
               ((remaining & TEXT_COLOR_PALETTE_MASK) << TEXT_PRIMARY_COMPONENT_SHIFT));
  color = (component | (component << TEXT_COLOR_GREEN_SHIFT) | (component << TEXT_COLOR_RED_SHIFT));
  if((remaining & TEXT_PRIMARY_OPAQUE_BIT) != 0)
    color |= TEXT_COLOR_OPAQUE;

  return (uint16) color;
}

static const uint8 *
_resolve_optional_font_offset(const uint8 *font_data_, uint32 offset_)
{
  if(offset_ == 0)
    return NULL;

  return font_data_ + offset_;
}
