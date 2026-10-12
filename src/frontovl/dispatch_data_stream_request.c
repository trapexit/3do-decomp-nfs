#include "data_stream_request_dispatcher.h"
#include "data_stream_request_queue.h"
#include "data_stream_subscriber_requests.h"
#include "runtime_error.h"
#include "sdk_veneers.h"

#include "msgport.h"

int32
dispatch_data_stream_request(DataStreamState *stream_,
                             DSRequestMsgPtr  request_,
                             boolean         *keep_running_)
{
  int32         status;
  DSStreamCBPtr sdk_stream;

  sdk_stream = (DSStreamCBPtr) stream_;
  if((request_->whatToDo < kDSOpPreRollStream) || (request_->whatToDo > kDSOpConnect))
    {
      return kDSInvalidDSRequest;
    }

  queue_data_stream_request(sdk_stream, request_);
  if(stream_->subscriber_replies_pending > 0)
    {
      return 0;
    }

  request_                            = stream_->request_head;
  stream_->subscriber_replies_pending = 0;

  switch(request_->whatToDo)
    {
      case kDSOpPreRollStream:
        status = handle_data_stream_preroll(sdk_stream, request_);
        break;

      case kDSOpCloseStream:
        status         = close_data_stream(sdk_stream, request_);
        *keep_running_ = FALSE;
        break;

      case kDSOpWaitEndOfStream:
        status = register_data_stream_completion_request(sdk_stream, request_);
        break;

      case kDSOpStartStream:
        status = handle_data_stream_start(sdk_stream, request_);
        break;

      case kDSOpStopStream:
        status = handle_data_stream_stop(sdk_stream, request_);
        break;

      case kDSOpSubscribe:
        status = register_data_stream_subscriber(sdk_stream, request_);
        break;

      case kDSOpGoMarker:
        status = position_data_stream_at_marker(sdk_stream, request_);
        break;

      case kDSOpGetChannel:
        status = request_data_stream_channel_status(sdk_stream, request_);
        break;

      case kDSOpSetChannel:
        status = set_data_stream_channel_status(sdk_stream, request_);
        break;

      case kDSOpControl:
        status = control_data_stream_subscriber(sdk_stream, request_);
        break;

      case kDSOpConnect:
        status = connect_data_stream_acquisition(sdk_stream, request_);
        break;

      default:
        status = kDSInvalidDSRequest;
        break;
    }

  if(stream_->subscriber_replies_pending == 0)
    {
      request_ = take_data_stream_request(sdk_stream);
      if(request_->whatToDo != kDSOpWaitEndOfStream)
        {
          status = ReplyMsg(request_->msgItem, status, request_, sizeof(DSRequestMsg));
          if(status < 0)
            {
              printf("Failure in %s: $%lx\n",
                     "DataStreamThread - HandleDSRequest - ReplyMsg() ",
                     status);
              print_runtime_error(status);
              RUNTIME_EXIT_VENEER(0);
            }
        }
    }

  return status;
}
