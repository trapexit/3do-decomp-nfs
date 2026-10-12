/* Frontovl semaphore creation with the original item-tag layout. */

#include "runtime_semaphore.h"

#include "item.h"
#include "kernelnodes.h"
#include "nodes.h"

#define RUNTIME_SEMAPHORE_PRIORITY_TAG_INDEX 0
#define RUNTIME_SEMAPHORE_NAME_TAG_INDEX 1
#define RUNTIME_SEMAPHORE_END_TAG_INDEX 2
#define RUNTIME_SEMAPHORE_TAG_COUNT 3
#define RUNTIME_SEMAPHORE_EMPTY_NAME_TAG 0xFF

Item
create_runtime_semaphore(const char *name_, uint8 priority_)
{
  TagArg tags[RUNTIME_SEMAPHORE_TAG_COUNT];

  tags[RUNTIME_SEMAPHORE_PRIORITY_TAG_INDEX].ta_Tag = TAG_ITEM_PRI;
  tags[RUNTIME_SEMAPHORE_PRIORITY_TAG_INDEX].ta_Arg = (void *) (uint32) priority_;
  if(name_ == NULL)
    {
      tags[RUNTIME_SEMAPHORE_NAME_TAG_INDEX].ta_Tag = RUNTIME_SEMAPHORE_EMPTY_NAME_TAG;
    }
  else
    {
      tags[RUNTIME_SEMAPHORE_NAME_TAG_INDEX].ta_Tag = TAG_ITEM_NAME;
      tags[RUNTIME_SEMAPHORE_NAME_TAG_INDEX].ta_Arg = (void *) name_;
    }

  tags[RUNTIME_SEMAPHORE_END_TAG_INDEX].ta_Tag = TAG_END;
  return CreateSizedItem(MKNODEID(KERNELNODE, SEMAPHORENODE), tags, 0);
}
