#include "sound_effect_slots.h"

#define SOUND_EFFECT_SLOT_DEFAULT_PRIORITY 999

SoundEffectSlot g_PRIMARY_SOUND_EFFECT_SLOTS[PRIMARY_SOUND_EFFECT_SLOT_COUNT];
SoundEffectSlot g_SECONDARY_SOUND_EFFECT_SLOTS[SECONDARY_SOUND_EFFECT_SLOT_COUNT];
SoundEffectSlot g_TERTIARY_SOUND_EFFECT_SLOTS[TERTIARY_SOUND_EFFECT_SLOT_COUNT];

void
initialize_primary_sound_effect_slots(void)
{
  int32 slot_index;

  for(slot_index = 0; slot_index < PRIMARY_SOUND_EFFECT_SLOT_COUNT; slot_index++)
    {
      g_PRIMARY_SOUND_EFFECT_SLOTS[slot_index].sample_index = -1;
      g_PRIMARY_SOUND_EFFECT_SLOTS[slot_index].active_voice = 0;
      g_PRIMARY_SOUND_EFFECT_SLOTS[slot_index].priority     = SOUND_EFFECT_SLOT_DEFAULT_PRIORITY;
    }
}

void
initialize_secondary_sound_effect_slots(void)
{
  int32 slot_index;

  for(slot_index = 0; slot_index < SECONDARY_SOUND_EFFECT_SLOT_COUNT; slot_index++)
    {
      g_SECONDARY_SOUND_EFFECT_SLOTS[slot_index].sample_index = -1;
      g_SECONDARY_SOUND_EFFECT_SLOTS[slot_index].active_voice = 0;
      g_SECONDARY_SOUND_EFFECT_SLOTS[slot_index].priority     = SOUND_EFFECT_SLOT_DEFAULT_PRIORITY;
    }
}

void
initialize_tertiary_sound_effect_slots(void)
{
  int32 slot_index;

  for(slot_index = 0; slot_index < TERTIARY_SOUND_EFFECT_SLOT_COUNT; slot_index++)
    {
      g_TERTIARY_SOUND_EFFECT_SLOTS[slot_index].sample_index = -1;
      g_TERTIARY_SOUND_EFFECT_SLOTS[slot_index].active_voice = 0;
      g_TERTIARY_SOUND_EFFECT_SLOTS[slot_index].priority     = SOUND_EFFECT_SLOT_DEFAULT_PRIORITY;
    }
}
