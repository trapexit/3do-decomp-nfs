#pragma once

#include "types.h"

#define GRAYSCALE_COLOR_COUNT 32
#define GRAYSCALE_RGB_COLOR_BYTES 3
#define GRAYSCALE_RGB_PALETTE_BYTES (GRAYSCALE_COLOR_COUNT * GRAYSCALE_RGB_COLOR_BYTES)

extern int32 g_USE_GAMMA_GRAYSCALE;
extern uint8 g_GAMMA_GRAYSCALE_RAMP[];

void
build_grayscale_rgb_palette(uint8 *rgb_palette_);
void
apply_linear_grayscale_clut(void);
void
apply_gamma_grayscale_clut(void);
void
transition_display_clut(const uint8 *source_palette_,
                        const uint8 *target_palette_,
                        int32        frame_count_);
void
fade_display_clut_from_black(int32 legacy_start_, int32 legacy_count_, int32 frame_count_);
void
fade_display_clut_to_black(int32 legacy_start_, int32 legacy_count_, int32 frame_count_);
