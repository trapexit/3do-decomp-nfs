#include "directory.h"
#include "directoryfunctions.h"
#include "filesystem.h"

#define FILE_FOLIO_OPEN_DIRECTORY_ITEM_VECTOR_OFFSET 0x1C

typedef Directory *(*OpenDirectoryItemFunction)(Item item_);

extern FileFolio *
GetFileFolio(void);

Directory *
OpenDirectoryItem(Item item_)
{
  FileFolio                *file_folio;
  OpenDirectoryItemFunction function;

  file_folio = GetFileFolio();
  function   = *(OpenDirectoryItemFunction *) ((uint8 *) file_folio -
                                               FILE_FOLIO_OPEN_DIRECTORY_ITEM_VECTOR_OFFSET);
  return function(item_);
}
