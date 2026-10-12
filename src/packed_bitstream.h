#pragma once

#include "types.h"

extern const uint8 *g_PACKED_INPUT_CURSOR;
extern uint8       *g_PACKED_OUTPUT_CURSOR;
extern int32        g_PACKED_BUFFERED_BIT_COUNT;
extern uint32       g_PACKED_BIT_BUFFER;

uint32
read_packed_bits(int32 bit_count_);
int32
read_packed_variable_length(void);
int32
decode_huffman_packed_bytes(const uint8 *source_, uint8 *destination_, int32 unpack_);
