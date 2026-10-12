#include "control_subscriber.h"

#include "kernel.h"

void
stop_control_channel(ControlSubscriberContext *context_, int32 channel_number_)
{
  SubsChannel *channel;

  if(channel_number_ < CTRL_MAX_CHANNELS)
    {
      channel = &context_->standard.channel[channel_number_];
      channel->status &= ~CHAN_ENABLED;
    }
}

void
flush_control_channel(ControlSubscriberContext *context_, int32 channel_number_)
{
  SubsChannel     *channel;
  SubscriberMsgPtr message;
  SubscriberMsgPtr next_message;

  if(channel_number_ < CTRL_MAX_CHANNELS)
    {
      channel = &context_->standard.channel[channel_number_];
      stop_control_channel(context_, channel_number_);
      while((message = channel->msgQueue.head) != NULL)
        {
          next_message = (SubscriberMsgPtr) message->link;
          ReplyMsg(message->msgItem, 0, message, sizeof(SubscriberMsgPtr));
          channel->msgQueue.head = next_message;
        }
      channel->msgQueue.tail = NULL;
    }
}
