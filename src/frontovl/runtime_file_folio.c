/* Compatibility entry point from the legacy filesystem glue object. */

#include "runtime_file_folio.h"

FileFolio *g_RUNTIME_FILE_FOLIO = NULL;

void
InitFileFolioGlue(void)
{
  (void) GetFileFolio();
}
