#include "sound_effect_state.h"

#include "file_information.h"
#include "file_load_options.h"
#include "game_bootstrap.h"
#include "game_data_path.h"
#include "headered_file.h"
#include "memory.h"
#include "pointer_array.h"
#include "string_utilities.h"
#include "vehicle_selection_state.h"

#include "audio.h"
#include "mem.h"

#include "stdio.h"

#define SOUND_SAMPLE_PATH_BYTE_COUNT 80
#define COLLISION_SOUND_BANK_PATH_FORMAT "%sDriveData/aiff/Collisions.AIFFfam"
#define FRONTEND_SOUND_BANK_PATH_FORMAT "%sDriveData/aiff/FrontEndSound.AIFFfam"
#define COLLISION_SOUND_FIRST_SAMPLE_INDEX 9
#define COLLISION_SOUND_SAMPLE_COUNT 12
#define COLLISION_SOUND_SAMPLE_GROUP 1
#define FRONTEND_SOUND_FIRST_SAMPLE_INDEX 1
#define FRONTEND_SOUND_SAMPLE_COUNT 1
#define FRONTEND_SOUND_SAMPLE_GROUP 3
#define SOUND_SAMPLE_ALLOCATION_NAME "sample"
#define VEHICLE_SOUND_BANK_PATH_FORMAT "%sDriveData/aiff/"
#define ENGINE_SOUND_FIRST_SAMPLE_INDEX 3
#define ENGINE_SOUND_SAMPLE_COUNT 5
#define ENGINE_SOUND_SAMPLE_GROUP 0
#define HORN_SOUND_FIRST_SAMPLE_INDEX 22
#define HORN_SOUND_SAMPLE_COUNT 2
#define HORN_SOUND_SAMPLE_GROUP 4
#define HEADERED_FILE_LOAD_OPTIONS 0x20

#define SOUND_EFFECT_STATE_SIZE_ASSERT(name_, expression_)                                         \
  typedef char sound_effect_state_size_assert_##name_[(expression_) ? 1 : -1]

int32
get_file_size_with_selector(const char *path_, int32 selector_);

int32                 g_STEREO_PANNING_ENABLED;
Item                  g_VEHICLE_SOUND_SAMPLES[VEHICLE_SOUND_SAMPLE_COUNT];
SoundSampleSliceGroup g_SOUND_SAMPLE_SLICE_GROUPS[SOUND_SAMPLE_SLICE_GROUP_COUNT];
const char G_VEHICLE_ENGINE_SOUND_BANK_NAME_STORAGE[VEHICLE_SOUND_BANK_COUNT]
                                                   [VEHICLE_SOUND_BANK_NAME_LONG_BYTES] = {
                                                     "SupraSound.AIFFfam", "DiabloSound.AIFFfam",
                                                     "911Sound.AIFFfam",   "ZR1Sound.AIFFfam",
                                                     "TR512Sound.AIFFfam", "ViperSound.AIFFfam",
                                                     "NSXSound.AIFFfam",   "RX7Sound.AIFFfam",
                                                     "DiabloSound.AIFFfam"
                                                   };
const VehicleHornSoundBankNameStorage G_VEHICLE_HORN_SOUND_BANK_NAME_STORAGE = {
  "SupraHorn.AIFFfam", "DiabloHorn.AIFFfam", "911Horn.AIFFfam",
  "ZR1Horn.AIFFfam",   "TR512Horn.AIFFfam",  "ViperHorn.AIFFfam",
  "NSXHorn.AIFFfam",   "RX7Horn.AIFFfam",    "DiabloHorn.AIFFfam"
};
const char *g_VEHICLE_ENGINE_SOUND_BANK_NAMES[VEHICLE_SOUND_BANK_COUNT] = {
  G_VEHICLE_ENGINE_SOUND_BANK_NAME_STORAGE[0], G_VEHICLE_ENGINE_SOUND_BANK_NAME_STORAGE[1],
  G_VEHICLE_ENGINE_SOUND_BANK_NAME_STORAGE[2], G_VEHICLE_ENGINE_SOUND_BANK_NAME_STORAGE[3],
  G_VEHICLE_ENGINE_SOUND_BANK_NAME_STORAGE[4], G_VEHICLE_ENGINE_SOUND_BANK_NAME_STORAGE[5],
  G_VEHICLE_ENGINE_SOUND_BANK_NAME_STORAGE[6], G_VEHICLE_ENGINE_SOUND_BANK_NAME_STORAGE[7],
  G_VEHICLE_ENGINE_SOUND_BANK_NAME_STORAGE[8]
};
const char *g_VEHICLE_HORN_SOUND_BANK_NAMES[VEHICLE_SOUND_BANK_COUNT] = {
  G_VEHICLE_HORN_SOUND_BANK_NAME_STORAGE.supra,
  G_VEHICLE_HORN_SOUND_BANK_NAME_STORAGE.diablo,
  G_VEHICLE_HORN_SOUND_BANK_NAME_STORAGE.porsche_911,
  G_VEHICLE_HORN_SOUND_BANK_NAME_STORAGE.corvette_zr1,
  G_VEHICLE_HORN_SOUND_BANK_NAME_STORAGE.ferrari_512tr,
  G_VEHICLE_HORN_SOUND_BANK_NAME_STORAGE.dodge_viper,
  G_VEHICLE_HORN_SOUND_BANK_NAME_STORAGE.acura_nsx,
  G_VEHICLE_HORN_SOUND_BANK_NAME_STORAGE.mazda_rx7,
  G_VEHICLE_HORN_SOUND_BANK_NAME_STORAGE.diablo_repeat
};
SOUND_EFFECT_STATE_SIZE_ASSERT(engine_sound_bank_name_storage,
                               sizeof(G_VEHICLE_ENGINE_SOUND_BANK_NAME_STORAGE) == 0xB4);
SOUND_EFFECT_STATE_SIZE_ASSERT(horn_sound_bank_name_storage,
                               sizeof(G_VEHICLE_HORN_SOUND_BANK_NAME_STORAGE) == 0xA4);

void *
allocate_sound_sample_memory(int32 byte_count_)
{
  return allocate_named_memory(SOUND_SAMPLE_ALLOCATION_NAME, byte_count_, 0);
}

void
free_sound_sample_memory(void *memory_)
{
  free_memory(memory_);
}

static Item
define_sound_sample(uint8 *sample_data_, int32 byte_count_)
{
  return DefineSampleHere(sample_data_,
                          byte_count_,
                          allocate_sound_sample_memory,
                          free_sound_sample_memory);
}

int32
get_file_size(const char *path_)
{
  return get_file_size_with_selector(path_, 1);
}

int32
get_file_size_with_selector(const char *path_, int32 selector_)
{
  int32 file_offset;
  int32 handle;
  int32 size;

  open_file_information(path_, &handle, &file_offset, &size, selector_);
  close_file_information(handle);
  return size;
}

static void
flush_sound_sample_output(void)
{
  fflush(stdout);
}

void *
load_headered_file(const char *path_)
{
  void *file_data;

  file_data = load_file_with_options(path_, HEADERED_FILE_LOAD_OPTIONS);
  if(file_data == NULL)
    return NULL;

  resolve_headered_file_offsets(file_data, 0);
  return file_data;
}

void
load_collision_sound_samples(void)
{
  char sample_path[SOUND_SAMPLE_PATH_BYTE_COUNT];

  sprintf(sample_path, COLLISION_SOUND_BANK_PATH_FORMAT, g_GAME_DATA_PATH);
  load_sound_sample_slices(sample_path,
                           COLLISION_SOUND_FIRST_SAMPLE_INDEX,
                           COLLISION_SOUND_SAMPLE_COUNT,
                           COLLISION_SOUND_SAMPLE_GROUP);
}

void
load_frontend_sound_samples(void)
{
  char sample_path[SOUND_SAMPLE_PATH_BYTE_COUNT];

  sprintf(sample_path, FRONTEND_SOUND_BANK_PATH_FORMAT, g_GAME_DATA_PATH);
  load_sound_sample_slices(sample_path,
                           FRONTEND_SOUND_FIRST_SAMPLE_INDEX,
                           FRONTEND_SOUND_SAMPLE_COUNT,
                           FRONTEND_SOUND_SAMPLE_GROUP);
}

void
load_vehicle_sound_samples(void)
{
  char sample_path[SOUND_SAMPLE_PATH_BYTE_COUNT];

  sprintf(sample_path, VEHICLE_SOUND_BANK_PATH_FORMAT, g_GAME_DATA_PATH);
  append_string(sample_path, g_VEHICLE_ENGINE_SOUND_BANK_NAMES[g_PLAYER_VEHICLE_INDEX]);
  load_sound_sample_slices(sample_path,
                           ENGINE_SOUND_FIRST_SAMPLE_INDEX,
                           ENGINE_SOUND_SAMPLE_COUNT,
                           ENGINE_SOUND_SAMPLE_GROUP);

  sprintf(sample_path, VEHICLE_SOUND_BANK_PATH_FORMAT, g_GAME_DATA_PATH);
  append_string(sample_path,
                g_VEHICLE_HORN_SOUND_BANK_NAMES[g_STARTUP_CONFIGURATION.opponent_vehicle_index]);
  load_sound_sample_slices(sample_path,
                           HORN_SOUND_FIRST_SAMPLE_INDEX,
                           HORN_SOUND_SAMPLE_COUNT,
                           HORN_SOUND_SAMPLE_GROUP);
  load_collision_sound_samples();
}

void
load_sound_sample_slices(const char *path_,
                         int32       first_sample_index_,
                         int32       sample_count_,
                         int32       sample_group_)
{
  SoundSampleSliceGroup *volatile group;
  uint8                 *first_sample_data;
  uint8 *volatile        next_sample_data;
  uint8                 *sample_data;
  volatile int32         file_size;
  int32                  sample_index;
  int32                  slice_index;

  flush_sound_sample_output();
  file_size = get_file_size(path_);
  flush_sound_sample_output();

  group                     = &g_SOUND_SAMPLE_SLICE_GROUPS[sample_group_];
  group->first_sample_index = (uint8) first_sample_index_;
  group->sample_count       = (uint8) sample_count_;
  group->headered_file      = load_headered_file(path_);
  first_sample_data =
    (uint8 *) get_headered_pointer_array_element((void *const *) group->headered_file, 0);
  sample_data  = first_sample_data;
  sample_index = first_sample_index_;

  for(slice_index = 1; slice_index < sample_count_; slice_index++)
    {
      next_sample_data =
        (uint8 *) get_headered_pointer_array_element((void *const *) group->headered_file,
                                                     slice_index);
      g_VEHICLE_SOUND_SAMPLES[sample_index] =
        define_sound_sample(sample_data, next_sample_data - sample_data);
      flush_sound_sample_output();
      if(g_VEHICLE_SOUND_SAMPLES[sample_index] < 0)
        flush_sound_sample_output();
      sample_index++;
      sample_data = next_sample_data;
    }

  g_VEHICLE_SOUND_SAMPLES[sample_index] =
    define_sound_sample(sample_data, (first_sample_data + file_size) - sample_data);
  flush_sound_sample_output();
  free_memory(group->headered_file);
}

void
enable_stereo_panning(void)
{
  g_STEREO_PANNING_ENABLED = 1;
}

void
disable_stereo_panning(void)
{
  g_STEREO_PANNING_ENABLED = 0;
}

void
configure_stereo_panning_from_startup(void)
{
  if(g_STARTUP_CONFIGURATION.stereo_enabled == 0)
    disable_stereo_panning();
  else
    enable_stereo_panning();
}

void
unload_vehicle_sound_samples(void)
{
  int32 sample_index;

  for(sample_index = 0; sample_index < VEHICLE_SOUND_SAMPLE_COUNT; sample_index++)
    {
      if((g_VEHICLE_SOUND_SAMPLES[sample_index] > 0) &&
         (UnloadSample(g_VEHICLE_SOUND_SAMPLES[sample_index]) > 0))
        fflush(stdout);
    }

  ScavengeMem();
}
