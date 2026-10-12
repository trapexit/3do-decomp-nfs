#pragma once

#include "types.h"

#define VEHICLE_SOUND_SAMPLE_COUNT 25
#define SOUND_SAMPLE_SLICE_GROUP_COUNT 5
#define VEHICLE_SOUND_BANK_COUNT 9
#define VEHICLE_SOUND_BANK_NAME_LONG_BYTES 20
#define VEHICLE_SOUND_BANK_NAME_SHORT_BYTES 16

typedef struct VehicleHornSoundBankNameStorage
{
  char supra[VEHICLE_SOUND_BANK_NAME_LONG_BYTES];
  char diablo[VEHICLE_SOUND_BANK_NAME_LONG_BYTES];
  char porsche_911[VEHICLE_SOUND_BANK_NAME_SHORT_BYTES];
  char corvette_zr1[VEHICLE_SOUND_BANK_NAME_SHORT_BYTES];
  char ferrari_512tr[VEHICLE_SOUND_BANK_NAME_LONG_BYTES];
  char dodge_viper[VEHICLE_SOUND_BANK_NAME_LONG_BYTES];
  char acura_nsx[VEHICLE_SOUND_BANK_NAME_SHORT_BYTES];
  char mazda_rx7[VEHICLE_SOUND_BANK_NAME_SHORT_BYTES];
  char diablo_repeat[VEHICLE_SOUND_BANK_NAME_LONG_BYTES];
} VehicleHornSoundBankNameStorage;

typedef struct SoundSampleSliceGroup
{
  uint8 first_sample_index;
  uint8 sample_count;
  uint8 reserved_002[2];
  void *headered_file;
} SoundSampleSliceGroup;

extern int32                 g_STEREO_PANNING_ENABLED;
extern Item                  g_VEHICLE_SOUND_SAMPLES[VEHICLE_SOUND_SAMPLE_COUNT];
extern SoundSampleSliceGroup g_SOUND_SAMPLE_SLICE_GROUPS[SOUND_SAMPLE_SLICE_GROUP_COUNT];
extern const char G_VEHICLE_ENGINE_SOUND_BANK_NAME_STORAGE[VEHICLE_SOUND_BANK_COUNT]
                                                          [VEHICLE_SOUND_BANK_NAME_LONG_BYTES];
extern const VehicleHornSoundBankNameStorage G_VEHICLE_HORN_SOUND_BANK_NAME_STORAGE;
extern const char *g_VEHICLE_ENGINE_SOUND_BANK_NAMES[VEHICLE_SOUND_BANK_COUNT];
extern const char *g_VEHICLE_HORN_SOUND_BANK_NAMES[VEHICLE_SOUND_BANK_COUNT];

void
enable_stereo_panning(void);
void
disable_stereo_panning(void);
void
configure_stereo_panning_from_startup(void);
void
load_collision_sound_samples(void);
void
load_frontend_sound_samples(void);
void
load_vehicle_sound_samples(void);
void *
allocate_sound_sample_memory(int32 byte_count_);
void
free_sound_sample_memory(void *memory_);
int32
get_file_size(const char *path_);
void *
load_headered_file(const char *path_);
void
load_sound_sample_slices(const char *path_,
                         int32       first_sample_index_,
                         int32       sample_count_,
                         int32       sample_group_);
void
load_vehicle_sound_samples(void);
void
unload_vehicle_sound_samples(void);
