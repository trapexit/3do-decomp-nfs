#pragma once

#include "types.h"

#define PRIMARY_SOUND_EFFECT_SLOT_COUNT 6
#define SECONDARY_SOUND_EFFECT_SLOT_COUNT 8
#define TERTIARY_SOUND_EFFECT_SLOT_COUNT 4

typedef struct SoundEffectSlot
{
  int32 sample_index;
  int32 active_voice;
  int32 priority;
} SoundEffectSlot;

extern SoundEffectSlot g_PRIMARY_SOUND_EFFECT_SLOTS[PRIMARY_SOUND_EFFECT_SLOT_COUNT];
extern SoundEffectSlot g_SECONDARY_SOUND_EFFECT_SLOTS[SECONDARY_SOUND_EFFECT_SLOT_COUNT];
extern SoundEffectSlot g_TERTIARY_SOUND_EFFECT_SLOTS[TERTIARY_SOUND_EFFECT_SLOT_COUNT];

void
initialize_primary_sound_effect_slots(void);
void
initialize_secondary_sound_effect_slots(void);
void
initialize_tertiary_sound_effect_slots(void);
