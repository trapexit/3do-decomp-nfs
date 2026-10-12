#include "vram_transfer.h"

#include "bit_utilities.h"
#include "diagnostics.h"
#include "display_environment.h"
#include "display_graphics.h"
#include "graphics_folio.h"
#include "legacy_io_wait.h"

#include "graphics.h"
#include "io.h"

#define VRAM_TRANSFER_ALL_BITS_MASK 0xFFFFFFFF
#define VRAM_BITMAP_HEADER_WORDS 4
#define VRAM_TRANSFER_INITIALIZATION_ERROR "initsport - CANNOT OPEN DMA CHANNEL"

Item  g_VRAM_IO_REQUEST;
uint8 g_VRAM_TRANSFER_RESERVED_STATE[0x14];

void
initialize_vram_transfer(void)
{
  if(initialize_display_environment() == 0)
    fatal_error(VRAM_TRANSFER_INITIALIZATION_ERROR);

  g_VRAM_PAGE_BYTES = GrafBase->gf_VRAMPageSize;
  g_VRAM_PAGE_SHIFT = get_highest_set_bit_index(g_VRAM_PAGE_BYTES);
  g_VRAM_IO_REQUEST = GetVRAMIOReq();
}

Err
copy_vram_bytes_deferred(void *destination_, const void *source_, int32 byte_count_)
{
  return CopyVRAMPagesDefer(g_VRAM_IO_REQUEST,
                            destination_,
                            (void *) source_,
                            (uint32) (byte_count_ >> g_VRAM_PAGE_SHIFT),
                            VRAM_TRANSFER_ALL_BITS_MASK);
}

Err
clear_vram_bytes(void *destination_, int32 byte_count_)
{
  return set_vram_bytes(destination_, byte_count_, 0);
}

Err
set_vram_bytes(void *destination_, int32 byte_count_, int32 value_)
{
  return SetVRAMPages(g_VRAM_IO_REQUEST,
                      destination_,
                      value_,
                      (uint32) (byte_count_ >> g_VRAM_PAGE_SHIFT),
                      VRAM_TRANSFER_ALL_BITS_MASK);
}

Err
wait_for_vram_transfer(void)
{
  return wait_for_io_completion(g_VRAM_IO_REQUEST);
}

Err
copy_display_bitmap_deferred(uint32 *destination_header_)
{
  uint32 page_count;

  page_count =
    (g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_BUFFER_WIDTH_WORD] *
     g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT.words[DISPLAY_BUFFER_DESCRIPTOR_BUFFER_HEIGHT_WORD]) >>
    g_VRAM_PAGE_SHIFT;
  page_count <<= 1;

  return CopyVRAMPagesDefer(g_VRAM_IO_REQUEST,
                            destination_header_ + VRAM_BITMAP_HEADER_WORDS,
                            (uint32 *) g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT
                                .words[DISPLAY_BUFFER_DESCRIPTOR_BITMAP_HEADER_WORD] +
                              VRAM_BITMAP_HEADER_WORDS,
                            page_count,
                            VRAM_TRANSFER_ALL_BITS_MASK);
}
