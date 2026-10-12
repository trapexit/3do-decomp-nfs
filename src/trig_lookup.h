#pragma once

#include "types.h"

#define ARCSINE_RATIO_TABLE_ENTRIES 512
#define SINE_QUARTER_WAVE_ENTRIES 257

extern uint8       g_ARCSINE_RATIO_TABLE[ARCSINE_RATIO_TABLE_ENTRIES];
extern const int32 G_SINE_QUARTER_WAVE[SINE_QUARTER_WAVE_ENTRIES];

int32
sine_1024(int32 angle_);
int32
cosine_1024(int32 angle_);
void
calculate_sine_cosine_1024(int32 angle_, int32 *sine_, int32 *cosine_);
int32
calculate_signed_arcsine_ratio(int32 numerator_, int32 denominator_);
