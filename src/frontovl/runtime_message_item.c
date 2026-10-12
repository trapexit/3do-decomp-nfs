/* Frontovl message-item creation with the oracle's tag-list behavior. */

#include "runtime_message_item.h"

#include "item.h"
#include "kernelnodes.h"
#include "msgport.h"

#define RUNTIME_MESSAGE_PRIORITY_TAG_INDEX 0
#define RUNTIME_MESSAGE_OPTION_TAG_INDEX 1
#define RUNTIME_MESSAGE_END_TAG_INDEX 2
#define RUNTIME_MESSAGE_PORT_TAG_COUNT 3
#define RUNTIME_MESSAGE_REPLY_PORT_TAG_INDEX 2
#define RUNTIME_MESSAGE_FINAL_TAG_INDEX 3
#define RUNTIME_MESSAGE_TAG_COUNT 4
#define RUNTIME_MESSAGE_EMPTY_NAME_TAG 0xFF

Item
create_runtime_message_port(const char *name_, uint8 priority_, int32 signal_)
{
  TagArg tags[RUNTIME_MESSAGE_PORT_TAG_COUNT];
  int32  end_tag_index;

  (void) name_;
  tags[RUNTIME_MESSAGE_PRIORITY_TAG_INDEX].ta_Tag = TAG_ITEM_PRI;
  tags[RUNTIME_MESSAGE_PRIORITY_TAG_INDEX].ta_Arg = (void *) (uint32) priority_;
  end_tag_index                                   = RUNTIME_MESSAGE_OPTION_TAG_INDEX;
  if(signal_ != 0)
    {
      tags[RUNTIME_MESSAGE_OPTION_TAG_INDEX].ta_Tag = CREATEPORT_TAG_SIGNAL;
      tags[RUNTIME_MESSAGE_OPTION_TAG_INDEX].ta_Arg = (void *) signal_;
      end_tag_index                                 = RUNTIME_MESSAGE_END_TAG_INDEX;
    }

  tags[end_tag_index].ta_Tag = TAG_END;
  return CreateSizedItem(MKNODEID(KERNELNODE, MSGPORTNODE), tags, 0);
}

Item
create_runtime_message(const char *name_, uint8 priority_, Item reply_port_)
{
  TagArg tags[RUNTIME_MESSAGE_TAG_COUNT];

  tags[RUNTIME_MESSAGE_PRIORITY_TAG_INDEX].ta_Tag = TAG_ITEM_PRI;
  tags[RUNTIME_MESSAGE_PRIORITY_TAG_INDEX].ta_Arg = (void *) (uint32) priority_;
  if(name_ == NULL)
    tags[RUNTIME_MESSAGE_OPTION_TAG_INDEX].ta_Tag = RUNTIME_MESSAGE_EMPTY_NAME_TAG;
  else
    {
      tags[RUNTIME_MESSAGE_OPTION_TAG_INDEX].ta_Tag = TAG_ITEM_NAME;
      tags[RUNTIME_MESSAGE_OPTION_TAG_INDEX].ta_Arg = (void *) name_;
    }

  tags[RUNTIME_MESSAGE_REPLY_PORT_TAG_INDEX].ta_Tag = CREATEMSG_TAG_REPLYPORT;
  tags[RUNTIME_MESSAGE_REPLY_PORT_TAG_INDEX].ta_Arg = (void *) reply_port_;
  tags[RUNTIME_MESSAGE_FINAL_TAG_INDEX].ta_Tag      = TAG_END;
  return CreateSizedItem(MKNODEID(KERNELNODE, MESSAGENODE), tags, 0);
}
