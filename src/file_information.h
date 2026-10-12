#pragma once

#include "filefunctions.h"
#include "filestream.h"
#include "types.h"

#define FILE_INFORMATION_SLOT_COUNT 36

typedef struct FileInformationSlot
{
  Stream *stream;
  Item    file_item;
  int32   type;
} FileInformationSlot;

typedef struct FileInformationState
{
  FileInformationSlot slots[FILE_INFORMATION_SLOT_COUNT];
  int32               unused[2];
  int32               initialized;
} FileInformationState;

extern FileInformationState g_FILE_INFORMATION_STATE;

int32
get_native_file_block_count(Item file_item_, int32 byte_count_);
int32
read_native_file(Item file_item_, void *buffer_, int32 byte_count_);
int32
delete_native_file(char *path_);
void
initialize_file_information(void);
void
open_file_information(const char *path_,
                      int32      *handle_,
                      int32      *file_offset_,
                      int32      *size_,
                      int32       report_error_);
void
open_file_information_with_error(const char *path_,
                                 int32      *handle_,
                                 int32      *file_offset_,
                                 int32      *size_);
void
open_file_information_default(const char *path_, int32 *handle_, int32 *file_offset_, int32 *size_);
void
close_file_information(int32 handle_);
int32
seek_file_information(int32 handle_, int32 offset_);
int32
read_file_information(int32 handle_, void *buffer_, int32 byte_count_);
int32
file_exists(const char *path_);
