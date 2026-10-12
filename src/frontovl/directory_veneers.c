/* Frontovl directory-folio entry points. */

#include "directory_veneers.h"

#include "runtime_file_folio.h"

#include "directoryfunctions.h"

Directory *
open_runtime_directory(char *path_)
{
  /* Keep the SDK call as a normal return-producing veneer, not a tail call. */
  Directory *volatile directory;

  directory = OpenDirectoryPath(path_);
  return directory;
}

Directory *
open_runtime_directory_item(Item directory_item_)
{
  Directory *volatile directory;

  (void) GetFileFolio();
  directory = OpenDirectoryItem(directory_item_);
  return directory;
}

int32
read_runtime_directory(Directory *directory_, DirectoryEntry *entry_)
{
  /* Keep the SDK call as a normal return-producing veneer, not a tail call. */
  int32 volatile status;

  status = ReadDirectory(directory_, entry_);
  return status;
}

void
close_runtime_directory(Directory *directory_)
{
  (void) GetFileFolio();
  CloseDirectory(directory_);
}
