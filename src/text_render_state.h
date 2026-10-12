#pragma once

#include "types.h"

#define TEXT_COLOR_PALETTE_COUNT 8
#define TEXT_PALETTE_COLOR_COUNT 16

typedef struct TextRenderState
{
  const uint8  *font_data;
  uint32        foreground_color;
  uint32        background_color;
  int32         cursor_x;
  int32         cursor_y;
  int32         dual_palette_enabled;
  uint32        format_tag;
  uint8         first_character;
  uint8         last_character;
  uint8         default_width;
  uint8         default_height;
  uint8         default_spacing;
  uint8         reserved_21;
  uint8         reserved_22;
  uint8         reserved_23;
  const uint8  *widths;
  const uint8  *heights;
  const uint8  *advances;
  const uint8  *x_offsets;
  const uint8  *y_offsets;
  const uint32 *cel_header;
  const int32  *glyph_offsets;
} TextRenderState;

extern TextRenderState g_TEXT_RENDER_STATE;
extern int32           g_TEXT_PALETTE_BANK_INDEX;
extern uint16         *g_TEXT_PRIMARY_PALETTE;
extern uint16         *g_TEXT_SECONDARY_PALETTE;
extern uint16          g_TEXT_PRIMARY_PALETTE_STORAGE[TEXT_PALETTE_COLOR_COUNT];
extern uint16 g_TEXT_COLOR_PALETTE_BANKS[TEXT_COLOR_PALETTE_COUNT][TEXT_PALETTE_COLOR_COUNT];

void
draw_text(const char *text_);
void
draw_text_at(const char *text_, int32 cursor_x_, int32 cursor_y_);
void
get_text_render_state(TextRenderState *state_);
void
set_text_render_state(const TextRenderState *state_);
void
initialize_text_render_font(const void *font_data_);
void
generate_dual_text_palettes(uint32 color_);
void
set_text_render_colors(uint32 foreground_color_, uint32 background_color_);
void
set_text_render_position(int32 cursor_x_, int32 cursor_y_);
