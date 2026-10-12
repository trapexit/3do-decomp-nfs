#pragma once

#include "cpak.h"

int32
pre_decompress_cinepak_frame(CinepakCodecState *codec_, const uint8 *frame_);
int32
pre_decompress_cinepak_frame_24(CinepakCodecState *codec_, const uint8 *frame_);
