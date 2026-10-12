#include "item_lookup.h"

#include "item.h"

#define NAMED_ITEM_TAG_COUNT 2
#define NAMED_ITEM_NAME_TAG_INDEX 0
#define NAMED_ITEM_END_TAG_INDEX 1
#define VERSIONED_ITEM_TAG_COUNT 4
#define VERSIONED_ITEM_NAME_TAG_INDEX 0
#define VERSIONED_ITEM_VERSION_TAG_INDEX 1
#define VERSIONED_ITEM_REVISION_TAG_INDEX 2
#define VERSIONED_ITEM_END_TAG_INDEX 3

Item
find_named_item(int32 item_type_, const char *name_)
{
  TagArg tags[NAMED_ITEM_TAG_COUNT];

  tags[NAMED_ITEM_NAME_TAG_INDEX].ta_Tag = TAG_ITEM_NAME;
  tags[NAMED_ITEM_NAME_TAG_INDEX].ta_Arg = (void *) name_;
  tags[NAMED_ITEM_END_TAG_INDEX].ta_Tag  = TAG_END;
  tags[NAMED_ITEM_END_TAG_INDEX].ta_Arg  = NULL;
  return FindItem(item_type_, tags);
}

Item
find_versioned_item(int32 item_type_, const char *name_, uint8 version_, uint8 revision_)
{
  TagArg tags[VERSIONED_ITEM_TAG_COUNT];

  tags[VERSIONED_ITEM_NAME_TAG_INDEX].ta_Tag     = TAG_ITEM_NAME;
  tags[VERSIONED_ITEM_NAME_TAG_INDEX].ta_Arg     = (void *) name_;
  tags[VERSIONED_ITEM_VERSION_TAG_INDEX].ta_Tag  = TAG_ITEM_VERSION;
  tags[VERSIONED_ITEM_VERSION_TAG_INDEX].ta_Arg  = (void *) (uint32) version_;
  tags[VERSIONED_ITEM_REVISION_TAG_INDEX].ta_Tag = TAG_ITEM_REVISION;
  tags[VERSIONED_ITEM_REVISION_TAG_INDEX].ta_Arg = (void *) (uint32) revision_;
  tags[VERSIONED_ITEM_END_TAG_INDEX].ta_Tag      = TAG_END;
  tags[VERSIONED_ITEM_END_TAG_INDEX].ta_Arg      = NULL;
  return FindItem(item_type_, tags);
}
