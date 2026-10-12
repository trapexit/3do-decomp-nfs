#include "stream_message_items.h"

#include "kernel_item.h"
#include "sdk_veneers.h"
#include "wait_for_kernel_port.h"

#include "debug.h"
#include "kernel.h"
#include "kernelnodes.h"
#include "msgport.h"
#include "nodes.h"

#define STREAM_MESSAGE_TAG_COUNT 2

uint32
get_message_port_signal(Item message_port_item_)
{
  MsgPort *message_port;

  message_port = (MsgPort *) lookup_kernel_item(message_port_item_);
  if(message_port == NULL)
    {
      return 0;
    }
  return message_port->mp_Signal;
}

Item
create_message_port(uint32 *signal_)
{
  Item message_port_item;

  message_port_item = CreateSizedItem(MKNODEID(KERNELNODE, MSGPORTNODE), NULL, 0);
  if((message_port_item > 0) && (signal_ != NULL))
    {
      *signal_ = get_message_port_signal(message_port_item);
    }
  return message_port_item;
}

void
remove_message_port(Item message_port_item_)
{
  volatile int32 status;

  status = DeleteItem(message_port_item_);
  (void) status;
}

Item
create_stream_message_item(Item reply_port_)
{
  Item   message_item;
  TagArg tags[STREAM_MESSAGE_TAG_COUNT];

  tags[0].ta_Tag = CREATEMSG_TAG_REPLYPORT;
  tags[0].ta_Arg = (void *) reply_port_;
  tags[1].ta_Tag = TAG_END;

  message_item = CreateSizedItem(MKNODEID(KERNELNODE, MESSAGENODE), tags, 0);
  return message_item;
}

void
remove_stream_message_item(Item message_item_)
{
  volatile int32 status;

  status = DeleteItem(message_item_);
  (void) status;
}

int32
allocate_stream_message_pool_entry(void *reply_port_, void *pool_entry_)
{
  Item                    message_item;
  StreamMessagePoolEntry *message;

  message_item = create_stream_message_item((Item) reply_port_);
  if(message_item <= 0)
    {
      return 0;
    }

  message               = (StreamMessagePoolEntry *) pool_entry_;
  message->message_item = message_item;
  return 1;
}

boolean
fill_pool_with_stream_message_items(MemoryPool *pool_, Item reply_port_)
{
  return for_each_free_memory_pool_entry(pool_,
                                         allocate_stream_message_pool_entry,
                                         (void *) reply_port_) != 0;
}

boolean
poll_for_stream_message(Item      message_port_,
                        Item     *message_item_,
                        Message **message_,
                        void    **message_data_,
                        int32    *status_)
{
  Message *received_message;
  Item     received_item;

  received_item = GetMsg(message_port_);
  if(received_item <= 0)
    {
      *status_ = received_item;
      return 0;
    }

  received_message = (Message *) lookup_kernel_item(received_item);
  if(received_message == NULL)
    {
      *status_ = -1;
      return 0;
    }

  if(message_item_ != NULL)
    *message_item_ = received_item;
  if(message_ != NULL)
    *message_ = received_message;
  if(message_data_ != NULL)
    *message_data_ = received_message->msg_DataPtr;
  *status_ = 0;

  if((received_message->msg.n_Flags & (MESSAGE_SENT | MESSAGE_REPLIED)) != 0)
    {
      kprintf("PollForMsg() - got unclean msg back from GetMsg()\n");
      Debug();
    }

  return 1;
}

int32
wait_for_stream_message(Item      message_port_,
                        Item     *message_item_,
                        Message **message_,
                        void    **message_data_,
                        Item      waited_message_item_)
{
  Item     received_item;
  Message *received_message;

  received_item = wait_for_kernel_port(message_port_, waited_message_item_);
  if(received_item < 0)
    {
      return received_item;
    }

  received_message = (Message *) lookup_kernel_item(received_item);
  if(received_message == NULL)
    {
      return -1;
    }

  if(message_item_ != NULL)
    {
      *message_item_ = received_item;
    }
  if(message_ != NULL)
    {
      *message_ = received_message;
    }
  if(message_data_ != NULL)
    {
      *message_data_ = received_message->msg_DataPtr;
    }

  if((received_message->msg.n_Flags & (MESSAGE_SENT | MESSAGE_REPLIED)) != 0)
    {
      kprintf("WaitForMsg() - got unclean msg back from WaitPort()\n");
      if(waited_message_item_ != 0)
        {
          kprintf("...while waiting on msg %lx\n", waited_message_item_);
        }
      Debug();
    }

  return 0;
}
