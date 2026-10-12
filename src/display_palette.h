#pragma once

#include "types.h"

#define DISPLAY_RGB555_PALETTE_ENTRY_COUNT 256
#define DISPLAY_CCB_DEFAULT_PALETTE_ENTRY_COUNT 32
#define DISPLAY_CCB_DEFAULT_PALETTE_FIRST_ENTRY                                                    \
  (DISPLAY_RGB555_PALETTE_ENTRY_COUNT - DISPLAY_CCB_DEFAULT_PALETTE_ENTRY_COUNT)

extern uint16 g_DISPLAY_RG_B555_PALETTE[DISPLAY_RGB555_PALETTE_ENTRY_COUNT];

/* The CCB default PLUT aliases the final 32 entries of the display palette. */
#define G_CCB_DEFAULT_PALETTE (&g_DISPLAY_RG_B555_PALETTE[DISPLAY_CCB_DEFAULT_PALETTE_FIRST_ENTRY])

void
initialize_palette_entries(int32 first_entry_, int32 entry_count_, const uint8 *rgba_entries_);
void
extract_interleaved_palette_entries_rgba(int32  first_entry_,
                                         int32  entry_count_,
                                         uint8 *rgba_entries_);
void
convert_rgb6_palette_entries(int32        first_entry_,
                             int32        entry_count_,
                             const uint8 *rgb_entries_,
                             uint16      *destination_palette_);
void
initialize_rgb6_palette_entries(int32 first_entry_, int32 entry_count_, const uint8 *rgb_entries_);
void
extract_interleaved_palette_entries_rgb6(int32  first_entry_,
                                         int32  entry_count_,
                                         uint8 *rgb_entries_);
