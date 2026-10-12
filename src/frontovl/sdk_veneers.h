#pragma once

#include "runtime_memory_ops.h"

#include "audio.h"
#include "graphics.h"
#include "types.h"

#include "stdio.h"

/*
 * Trusted SDK veneers retain address-derived linker names in the assembly
 * image. Keep that linkage detail behind semantic aliases in C source.
 */
#define SDK_VENEER_NAME_INNER(prefix, address) prefix##address
#define SDK_VENEER_NAME(prefix, address) SDK_VENEER_NAME_INNER(prefix, address)
#define SDK_DATA_NAME_INNER(prefix, address) prefix##address
#define SDK_DATA_NAME(prefix, address) SDK_DATA_NAME_INNER(prefix, address)
#define RUNTIME_EXIT_VENEER exit
#define LOAD_INSTRUMENT_TEMPLATE_VENEER load_runtime_instrument_template
#define CLOSE_DISK_STREAM_VENEER close_runtime_disk_stream
#define OPEN_DISK_STREAM_VENEER SDK_VENEER_NAME(func_, 000198E8)
#define READ_DISK_STREAM_VENEER SDK_VENEER_NAME(func_, 0001991C)
#define SEEK_DISK_STREAM_VENEER SDK_VENEER_NAME(func_, 00019958)
#define DRAW_CELS_VENEER draw_cels_via_graphics_folio
#define OPEN_DIRECTORY_PATH_VENEER SDK_VENEER_NAME(func_, 0001985C)
#define READ_DIRECTORY_VENEER SDK_VENEER_NAME(func_, 00019888)
#define CLOSE_DIRECTORY_VENEER close_runtime_directory
#define RUNTIME_MEMCPY_VENEER copy_runtime_memory

extern void
RUNTIME_EXIT_VENEER(int status_);
extern void *
OPEN_DISK_STREAM_VENEER(char *path_, int32 buffer_size_);
extern int32
READ_DISK_STREAM_VENEER(void *disk_stream_, void *buffer_, int32 byte_count_);
extern int32
SEEK_DISK_STREAM_VENEER(void *disk_stream_, int32 offset_, int32 origin_);
extern Err
DRAW_CELS_VENEER(Item bitmap_item_, CCB *cel_);
extern void *
OPEN_DIRECTORY_PATH_VENEER(char *path_);
extern int32
READ_DIRECTORY_VENEER(void *directory_, void *entry_);
