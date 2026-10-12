#pragma once

#include "graphics.h"

extern uint32 g_CINEPAK_CROSSFADE_PIXC_VALUES[28];
extern uint8  g_CINEPAK_CROSSFADE_LEVEL_BYTES[52];

void
configure_cinepak_crossfade_cels(CCB *first_cel_, CCB *second_cel_, int32 step_);
