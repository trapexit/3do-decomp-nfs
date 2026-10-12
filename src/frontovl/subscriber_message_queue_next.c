#include "subscriber_message_queue.h"

SubscriberMsgPtr
get_next_subscriber_message(SubscriberMessageQueue *queue)
{
  SubscriberMsgPtr message;

  message = queue->head;
  if (message == 0)
    return 0;

  queue->head = (SubscriberMsgPtr)message->link;
  if (queue->tail == message)
    queue->tail = 0;

  return message;
}