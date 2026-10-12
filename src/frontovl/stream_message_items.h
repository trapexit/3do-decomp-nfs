#pragma once

#include "memory_pool.h"

#include "msgport.h"
#include "types.h"

typedef struct StreamMessagePoolEntry
{
  int32 operation;
  Item  message_item;
} StreamMessagePoolEntry;

uint32
get_message_port_signal(Item message_port_item_);
Item
create_message_port(uint32 *signal_);
void
remove_message_port(Item message_port_item_);
Item
create_stream_message_item(Item reply_port_);
void
remove_stream_message_item(Item message_item_);
int32
allocate_stream_message_pool_entry(void *reply_port_, void *pool_entry_);
boolean
fill_pool_with_stream_message_items(MemoryPool *pool_, Item reply_port_);
boolean
poll_for_stream_message(Item      message_port_,
                        Item     *message_item_,
                        Message **message_,
                        void    **message_data_,
                        int32    *status_);
int32
wait_for_stream_message(Item      message_port_,
                        Item     *message_item_,
                        Message **message_,
                        void    **message_data_,
                        Item      waited_message_item_);
