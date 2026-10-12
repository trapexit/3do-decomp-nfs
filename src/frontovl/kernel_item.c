/* Frontovl kernel item lookup entry point. */

#include "kernel_item.h"

#include "item.h"
#include "mem.h"

int32
_GetPageSize(uint32 type_bits_);
Err
_ItemOpened(Item task_, Item item_);

void *
lookup_kernel_item(Item item_)
{
  return LookupItem(item_);
}

void *
check_kernel_item(Item item_, uint8 folio_type_, uint8 node_type_)
{
  return CheckItem(item_, folio_type_, node_type_);
}

int32
get_kernel_page_size(uint32 type_bits_)
{
  return _GetPageSize(type_bits_);
}

Err
is_kernel_item_opened(Item task_, Item item_)
{
  return _ItemOpened(task_, item_);
}
