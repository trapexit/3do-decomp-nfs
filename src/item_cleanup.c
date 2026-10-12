#include "item_cleanup.h"

#include "item.h"
#include "operror.h"

Item
open_item_with_error(Item item_)
{
  Item opened_item;

  opened_item = OpenItem(item_, NULL);
  PrintfSysErr(opened_item);

  return opened_item;
}

void
delete_item_and_clear(Item *item_)
{
  if(*item_ != 0)
    {
      DeleteItem(*item_);
      *item_ = 0;
    }
}
