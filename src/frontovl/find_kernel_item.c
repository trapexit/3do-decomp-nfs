/* Frontovl kernel-item searches using explicit name and version tag lists. */

#include "find_kernel_item.h"

#include "item.h"

#define KERNEL_ITEM_NAME_TAG_INDEX 0
#define KERNEL_ITEM_VERSION_TAG_INDEX 1
#define KERNEL_ITEM_REVISION_TAG_INDEX 2
#define KERNEL_ITEM_END_TAG_INDEX 3
#define KERNEL_ITEM_NAME_TAG_COUNT 2
#define KERNEL_ITEM_VERSIONED_TAG_COUNT 4

Item
find_named_kernel_item(int32 item_type_, const char *name_)
{
  TagArg tags[KERNEL_ITEM_NAME_TAG_COUNT];

  tags[KERNEL_ITEM_NAME_TAG_INDEX].ta_Tag    = TAG_ITEM_NAME;
  tags[KERNEL_ITEM_NAME_TAG_INDEX].ta_Arg    = (void *) name_;
  tags[KERNEL_ITEM_VERSION_TAG_INDEX].ta_Tag = TAG_END;
  return FindItem(item_type_, tags);
}

Item
find_versioned_kernel_item(int32 item_type_, const char *name_, uint8 version_, uint8 revision_)
{
  TagArg tags[KERNEL_ITEM_VERSIONED_TAG_COUNT];

  tags[KERNEL_ITEM_NAME_TAG_INDEX].ta_Tag    = TAG_ITEM_NAME;
  tags[KERNEL_ITEM_NAME_TAG_INDEX].ta_Arg    = (void *) name_;
  tags[KERNEL_ITEM_VERSION_TAG_INDEX].ta_Tag = TAG_ITEM_VERSION;
  /* The 3DO TagArg ABI carries these byte values in pointer-sized words. */
  tags[KERNEL_ITEM_VERSION_TAG_INDEX].ta_Arg  = (void *) (uint32) version_;
  tags[KERNEL_ITEM_REVISION_TAG_INDEX].ta_Tag = TAG_ITEM_REVISION;
  tags[KERNEL_ITEM_REVISION_TAG_INDEX].ta_Arg = (void *) (uint32) revision_;
  tags[KERNEL_ITEM_END_TAG_INDEX].ta_Tag      = TAG_END;
  return FindItem(item_type_, tags);
}
