#pragma once

#include "ccb_manager.h"

#include "graphics.h"

typedef struct CelChunkHeader
{
  uint32 type;
  int32  byte_count;
} CelChunkHeader;

extern uint8 g_CEL_BITS_PER_PIXEL[8];
extern uint8 g_CEL_SOURCE_ALIGNMENT_MASKS[20];

int32
unload_cel(CCB *cel_);
int32
unload_cel_handle(CCB *cel_);
uint32
get_cel_bits_per_pixel(const CCB *cel_);
int32
get_cel_width(const CCB *cel_);
int32
get_cel_height(const CCB *cel_);
CelData *
get_cel_source_pointer(const CCB *cel_);
void
clip_render_ccb_to_display(RenderCcb *ccb_);
void
set_cel_dimension_log2(CCB *cel_);
CCB *
load_cel(const char *path_, uint32 memory_flags_, int32 report_error_);
CCB *
load_cel_with_error(const char *path_, uint32 memory_flags_);
CCB *
load_cel_quiet(const char *path_, uint32 memory_flags_);
CCB *
load_cel_default(const char *path_, uint32 memory_flags_);
CCB *
parse_cel_data(void *data_, uint32 byte_count_, int32 report_error_);
CelChunkHeader *
advance_cel_chunk(uint32 *type_, CelChunkHeader **cursor_, int32 *remaining_byte_count_);
