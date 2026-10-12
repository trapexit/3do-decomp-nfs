#pragma once

#include "types.h"

#define RANDOM_STATE_WORD_COUNT 6

extern uint32 g_RANDOM_STATE[RANDOM_STATE_WORD_COUNT];

uint32
next_random_word(void);
int32
scale_by_random_fraction(int32 value_);
