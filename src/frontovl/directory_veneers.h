#pragma once

#include "directory.h"

Directory *
open_runtime_directory(char *path_);
Directory *
open_runtime_directory_item(Item directory_item_);
int32
read_runtime_directory(Directory *directory_, DirectoryEntry *entry_);
void
close_runtime_directory(Directory *directory_);
