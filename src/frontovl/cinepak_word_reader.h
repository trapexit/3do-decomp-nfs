#pragma once

#include "types.h"

uint32
read_cinepak_chunk_word(const uint8 *bytes_);
uint32
read_cinepak_frame_word(const uint8 *bytes_);
