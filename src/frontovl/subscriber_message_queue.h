#pragma once

#include "datastream.h"
#include "types.h"

typedef struct SubscriberMessageQueue
{
  SubscriberMsgPtr head;
  SubscriberMsgPtr tail;
} SubscriberMessageQueue;

boolean
add_subscriber_message_to_tail(SubscriberMessageQueue *queue_, SubscriberMsgPtr message_);
SubscriberMsgPtr
get_next_subscriber_message(SubscriberMessageQueue *queue_);
