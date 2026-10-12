#include "graphics_folio.h"

#include "diagnostics.h"
#include "item_lookup.h"

#include "folio.h"
#include "item.h"

#define GRAPHICS_FOLIO_OPEN_ERROR "Error opening GraphicsFolio (%d)"

Item              GrafFolioNum;
struct GrafFolio *GrafBase;

Err
open_graphics_folio(void)
{
  Err result;

  result = OpenItem(find_named_item(MKNODEID(KERNELNODE, FOLIONODE), "Graphics"), NULL);
  GrafFolioNum = result;
  if(result < 0)
    {
      fatal_error(GRAPHICS_FOLIO_OPEN_ERROR, result);
      return GrafFolioNum;
    }

  GrafBase = (struct GrafFolio *) LookupItem(result);
  return 0;
}
