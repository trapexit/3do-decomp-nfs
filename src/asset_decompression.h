#pragma once

#include "types.h"

#define INVALID_PACK_CODE_MESSAGE_BYTES 36

extern const char G_INVALID_PACK_CODE_MESSAGE[INVALID_PACK_CODE_MESSAGE_BYTES];

int32
decode_delta_bytes(const uint8 *source_, uint8 *destination_);
int32
decode_byte_pair_bytes(const uint8 *source_, uint8 *destination_);
int32
decode_packed_data(const uint8 *source_,
                   uint8       *destination_,
                   int32        source_size_,
                   int32        report_invalid_);
int32
decode_packed_data_checked(const uint8 *source_, uint8 *destination_, int32 source_size_);
int32
decode_packed_data_quiet(const uint8 *source_, uint8 *destination_, int32 source_size_);
int32
decode_ref_pack_bytes(const uint8 *source_, uint8 *destination_, int32 decode_);
int32
decode_run_length_blocks(const uint8 *source_, uint8 *destination_);
