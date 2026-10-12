#include "subscriber_message_queue.h"

boolean
add_subscriber_message_to_tail(SubscriberMessageQueue *queue, SubscriberMsgPtr message)
{
  message->link = 0;
  if (queue->head != 0) {
    queue->tail->link = message;
    queue->tail = message;
    return 0;
  } else {
    queue->head = message;
    queue->tail = message;
    return 1;
  }
}