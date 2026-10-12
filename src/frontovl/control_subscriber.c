#include "control_subscriber.h"

MemoryPool *g_CONTROL_SUBSCRIBER_CONTEXT_POOL = NULL;

int32
close_control_subscriber(void)
{
  delete_memory_pool(g_CONTROL_SUBSCRIBER_CONTEXT_POOL);
  g_CONTROL_SUBSCRIBER_CONTEXT_POOL = NULL;
  return 0;
}

int32
get_control_channel_status(SubscriberMsgPtr message_, ControlSubscriberContext *context_)
{
  int32 channel_number;

  channel_number = message_->msg.channel.number;
  if(channel_number < CTRL_MAX_CHANNELS)
    {
      return context_->standard.channel[channel_number].status;
    }
  return 0;
}

int32
set_control_channel_subscription(SubscriberMsgPtr message_, ControlSubscriberContext *context_)
{
  SubsChannel *channel;
  int32        channel_number;

  channel_number = message_->msg.channel.number;
  if(channel_number < CTRL_MAX_CHANNELS)
    {
      channel = &context_->standard.channel[channel_number];
      channel->status |= (~CHAN_SYSBITS & message_->msg.channel.status);
    }
  return 0;
}

int32
handle_control_subscriber_control(SubscriberMsgPtr message_, void *context_)
{
  (void) message_;
  (void) context_;
  return 0;
}

int32
handle_control_subscriber_sync(SubscriberMsgPtr message_, void *context_)
{
  (void) message_;
  (void) context_;
  return 0;
}

int32
handle_control_subscriber_opening(SubscriberMsgPtr message_, void *context_)
{
  (void) message_;
  (void) context_;
  return 0;
}

int32
handle_control_subscriber_closing(SubscriberMsgPtr message_, void *context_)
{
  (void) message_;
  (void) context_;
  return 0;
}

int32
stop_control_subscription(SubscriberMsgPtr message_, ControlSubscriberContext *context_)
{
  int32 channel_number;

  (void) message_;
  for(channel_number = 0; channel_number < CTRL_MAX_CHANNELS; channel_number++)
    {
      stop_control_channel(context_, channel_number);
    }
  return 0;
}

int32
handle_control_subscriber_end_of_file(SubscriberMsgPtr message_, void *context_)
{
  (void) message_;
  (void) context_;
  return 0;
}

int32
abort_control_subscription(SubscriberMsgPtr message_, ControlSubscriberContext *context_)
{
  int32 channel_number;

  (void) message_;
  for(channel_number = 0; channel_number < CTRL_MAX_CHANNELS; channel_number++)
    {
      flush_control_channel(context_, channel_number);
    }
  return 0;
}
