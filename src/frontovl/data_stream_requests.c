#include "data_stream_requests.h"

#include "data_stream_state.h"
#include "kernel_item.h"
#include "runtime_error.h"
#include "sdk_veneers.h"
#include "stream_message_items.h"

#include "msgport.h"

int32
send_data_stream_request(Item            message_item_,
                         boolean         asynchronous_,
                         DSStreamCBPtr   stream_,
                         DSRequestMsgPtr request_)
{
  int32    status;
  Message *message;
  Item     reply_port;

  if(request_->whatToDo < 0 || request_->whatToDo > kDSOpConnect)
    {
      return kDSInvalidDSRequest;
    }

  request_->msgItem = message_item_;
  message           = (Message *) lookup_kernel_item(message_item_);
  if(message == NULL)
    {
      return -1;
    }

  reply_port = message->msg_ReplyPort;
  if(reply_port == 0)
    {
      return kDSNoReplyPortErr;
    }

  status = SendMsg(stream_->requestPort, message_item_, request_, sizeof(DSRequestMsg));
  if(status < 0)
    {
      return status;
    }

  if(!asynchronous_)
    {
      status = wait_for_stream_message(reply_port, NULL, &message, NULL, request_->msgItem);
      if(status < 0)
        {
          return status;
        }
      status = message->msg_Result;
    }

  return status;
}

int32
broadcast_to_data_stream_subscribers(DSStreamCBPtr    stream_,
                                     MemoryPool      *message_pool_,
                                     SubscriberMsgPtr message_)
{
  int32            status;
  int32            subscriber_index;
  DSSubscriberPtr  subscriber;
  SubscriberMsgPtr message_copy;
  /* Retail's zero-subscriber path inherits a zero result from its sole caller. */
  status = 0;

  if(message_pool_->free_count < stream_->numSubscribers)
    {
      printf("_SubscriberBroadcast() - insufficient number of subscriber "
             "messages\n");
      return kDSNoMsgErr;
    }

  for(subscriber_index = 0; subscriber_index < stream_->numSubscribers; subscriber_index += 1)
    {
      subscriber   = &stream_->subscriber[subscriber_index];
      message_copy = (SubscriberMsgPtr) allocate_memory_pool_entry(message_pool_);
      if(message_copy != NULL)
        {
          message_->msgItem = message_copy->msgItem;
          *message_copy     = *message_;
          status            = SendMsg(subscriber->subscriberPort,
                                      message_copy->msgItem,
                                      message_copy,
                                      sizeof(SubscriberMsg));
          if(status < 0)
            {
              printf("Failure in %s: $%lx\n", "_SubscriberBroadcast() -> SendMsg() ", status);
              print_runtime_error(status);
              RUNTIME_EXIT_VENEER(0);
              break;
            }
        }
      else
        {
          printf("_SubscriberBroadcast() - ran out of subscriber messages\n");
          status = kDSNoMsgErr;
          break;
        }
    }

  return status;
}

int32
subscribe_to_data_stream(Item            message_item_,
                         DSRequestMsgPtr request_,
                         DSStreamCBPtr   stream_,
                         DSDataType      stream_type_,
                         Item            subscriber_port_)
{
  int32        status;
  DSRequestMsg request_storage;
  boolean      asynchronous;

  if(request_ == NULL)
    {
      request_     = &request_storage;
      asynchronous = 0;
    }
  else
    {
      asynchronous = 1;
    }

  request_->whatToDo                     = kDSOpSubscribe;
  request_->msg.subscribe.dataType       = stream_type_;
  request_->msg.subscribe.subscriberPort = subscriber_port_;
  status = send_data_stream_request(message_item_, asynchronous, stream_, request_);
  return status;
}

int32
preroll_data_stream(Item message_item_, DSRequestMsgPtr request_, DSStreamCBPtr stream_)
{
  int32        status;
  DSRequestMsg request_storage;
  boolean      asynchronous;

  if(request_ == NULL)
    {
      request_     = &request_storage;
      asynchronous = 0;
    }
  else
    {
      asynchronous = 1;
    }

  request_->whatToDo = kDSOpPreRollStream;
  status             = send_data_stream_request(message_item_, asynchronous, stream_, request_);
  return status;
}

int32
start_data_stream(Item            message_item_,
                  DSRequestMsgPtr request_,
                  DSStreamCBPtr   stream_,
                  uint32          options_)
{
  int32        status;
  DSRequestMsg request_storage;
  boolean      asynchronous;

  if(request_ == NULL)
    {
      request_     = &request_storage;
      asynchronous = 0;
    }
  else
    {
      asynchronous = 1;
    }

  request_->whatToDo          = kDSOpStartStream;
  request_->msg.start.options = options_;
  status = send_data_stream_request(message_item_, asynchronous, stream_, request_);
  return status;
}

int32
stop_data_stream(Item            message_item_,
                 DSRequestMsgPtr request_,
                 DSStreamCBPtr   stream_,
                 uint32          options_)
{
  int32        status;
  DSRequestMsg request_storage;
  boolean      asynchronous;

  if(request_ == NULL)
    {
      request_     = &request_storage;
      asynchronous = 0;
    }
  else
    {
      asynchronous = 1;
    }

  request_->whatToDo         = kDSOpStopStream;
  request_->msg.stop.options = options_;
  status = send_data_stream_request(message_item_, asynchronous, stream_, request_);
  return status;
}

int32
go_to_data_stream_marker(Item            message_item_,
                         DSRequestMsgPtr request_,
                         DSStreamCBPtr   stream_,
                         uint32          marker_value_,
                         uint32          options_)
{
  int32        status;
  DSRequestMsg request_storage;
  boolean      asynchronous;

  if(request_ == NULL)
    {
      request_     = &request_storage;
      asynchronous = 0;
    }
  else
    {
      asynchronous = 1;
    }

  request_->whatToDo                 = kDSOpGoMarker;
  request_->msg.goMarker.markerValue = marker_value_;
  request_->msg.goMarker.options     = options_;
  status = send_data_stream_request(message_item_, asynchronous, stream_, request_);
  return status;
}

int32
get_data_stream_channel(Item            message_item_,
                        DSRequestMsgPtr request_,
                        DSStreamCBPtr   stream_,
                        DSDataType      stream_type_,
                        int32           channel_number_,
                        int32          *channel_status_)
{
  int32        status;
  DSRequestMsg request_storage;
  boolean      asynchronous;

  if(request_ == NULL)
    {
      request_     = &request_storage;
      asynchronous = 0;
    }
  else
    {
      asynchronous = 1;
    }

  request_->whatToDo                        = kDSOpGetChannel;
  request_->msg.getChannel.streamType       = stream_type_;
  request_->msg.getChannel.channelNumber    = channel_number_;
  request_->msg.getChannel.channelStatusPtr = channel_status_;
  status = send_data_stream_request(message_item_, asynchronous, stream_, request_);
  return status;
}

int32
set_data_stream_channel(Item            message_item_,
                        DSRequestMsgPtr request_,
                        DSStreamCBPtr   stream_,
                        DSDataType      stream_type_,
                        int32           channel_number_,
                        int32           channel_status_)
{
  int32        status;
  DSRequestMsg request_storage;
  boolean      asynchronous;

  if(request_ == NULL)
    {
      request_     = &request_storage;
      asynchronous = 0;
    }
  else
    {
      asynchronous = 1;
    }

  request_->whatToDo                     = kDSOpSetChannel;
  request_->msg.setChannel.streamType    = stream_type_;
  request_->msg.setChannel.channelNumber = channel_number_;
  request_->msg.setChannel.channelStatus = channel_status_;
  status = send_data_stream_request(message_item_, asynchronous, stream_, request_);
  return status;
}

int32
control_data_stream(Item            message_item_,
                    DSRequestMsgPtr request_,
                    DSStreamCBPtr   stream_,
                    DSDataType      stream_type_,
                    int32           operation_,
                    void           *argument_)
{
  int32        status;
  DSRequestMsg request_storage;
  boolean      asynchronous;

  if(request_ == NULL)
    {
      request_     = &request_storage;
      asynchronous = 0;
    }
  else
    {
      asynchronous = 1;
    }

  request_->whatToDo                      = kDSOpControl;
  request_->msg.control.streamType        = stream_type_;
  request_->msg.control.userDefinedOpcode = operation_;
  request_->msg.control.userDefinedArgPtr = argument_;
  status = send_data_stream_request(message_item_, asynchronous, stream_, request_);
  return status;
}

int32
connect_data_stream(Item            message_item_,
                    DSRequestMsgPtr request_,
                    DSStreamCBPtr   stream_,
                    Item            acquisition_port_)
{
  int32        status;
  DSRequestMsg request_storage;
  boolean      asynchronous;

  if(request_ == NULL)
    {
      request_     = &request_storage;
      asynchronous = 0;
    }
  else
    {
      asynchronous = 1;
    }

  request_->whatToDo                = kDSOpConnect;
  request_->msg.connect.acquirePort = acquisition_port_;
  status = send_data_stream_request(message_item_, asynchronous, stream_, request_);
  return status;
}

int32
wait_for_data_stream_end(Item message_item_, DSRequestMsgPtr request_, DSStreamCBPtr stream_)
{
  int32        status;
  DSRequestMsg request_storage;
  boolean      asynchronous;

  if(request_ == NULL)
    {
      request_     = &request_storage;
      asynchronous = 0;
    }
  else
    {
      asynchronous = 1;
    }

  request_->whatToDo = kDSOpWaitEndOfStream;
  status             = send_data_stream_request(message_item_, asynchronous, stream_, request_);
  return status;
}

int32
synchronize_data_stream_clock(DSStreamCBPtr stream_,
                              MemoryPool   *message_pool_,
                              uint32        current_time_)
{
  int32         status;
  SubscriberMsg message;

  status = set_data_stream_clock((DataStreamState *) stream_, current_time_);
  if(status != 0)
    {
      return status;
    }

  message.whatToDo       = kStreamOpSync;
  message.privatePtr     = NULL;
  message.msg.sync.clock = current_time_;
  status                 = broadcast_to_data_stream_subscribers(stream_, message_pool_, &message);
  return status;
}
