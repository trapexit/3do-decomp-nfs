#pragma once

#include "types.h"

typedef void *(*CinepakPaletteAllocateFunction)(int32 byte_count_);
typedef void  (*CinepakPaletteFreeFunction)(void *memory_, int32 byte_count_);

void *
create_cinepak_palette_handler(CinepakPaletteAllocateFunction allocate_);
void
delete_cinepak_palette_handler(void *handler_, CinepakPaletteFreeFunction free_);
