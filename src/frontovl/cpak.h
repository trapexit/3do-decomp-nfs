#pragma once

#include "types.h"

#define CPAK_SIZE_HIGH_BYTE 1
#define CPAK_SIZE_MIDDLE_BYTE 2
#define CPAK_SIZE_LOW_BYTE 3
#define CPAK_SIZE_HIGH_SHIFT 16
#define CPAK_SIZE_MIDDLE_SHIFT 8
#define CPAK_FRAME_WORD_COUNT 2560
#define CINEPAK_CODEC_DATA_SIZE 20480
#define CINEPAK_CODEC_MAGIC ((uint32) 'cPak')

typedef struct CinepakCodecState
{
  uint32 magic;
  uint32 option;
  void  *handler;
  uint8  data[CINEPAK_CODEC_DATA_SIZE];
} CinepakCodecState;

uint32
read_cpak_chunk_size(const uint8 chunk_[]);
void
copy_cpak_frame(const uint32 source_[], uint32 destination_[]);
CinepakCodecState *
create_cinepak_codec(void *handler_, uint32 option_);
void
delete_cinepak_codec(CinepakCodecState *codec_);
