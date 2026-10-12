/* Frontovl memory-header lookup through the Portfolio C library veneer. */

#include "find_memory_header.h"

/* This Portfolio export is absent from the newer public headers. */
MemHdr *
_FindMH(void *address_);

MemHdr *
find_memory_header(void *address_)
{
  return _FindMH(address_);
}
