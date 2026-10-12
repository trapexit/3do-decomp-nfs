#include "data_stream_state.h"
#include "data_stream_subscriber_requests.h"

#include "datastream.h"

enum
{
  K_SET_FIRST_INTERNAL_STREAM_OPTION       = 0,
  K_SET_SECOND_INTERNAL_STREAM_OPTION      = 1,
  K_GET_FIRST_INTERNAL_STREAM_OPTION       = 2,
  K_GET_SECOND_INTERNAL_STREAM_OPTION      = 3,
  K_FIRST_INTERNAL_STREAM_OPTION_DISABLED  = 0x400,
  K_SECOND_INTERNAL_STREAM_OPTION_DISABLED = 0x800
};

int32
handle_internal_data_stream_control(DSStreamCBPtr stream_, DSRequestMsgPtr request_)
{
  uint8           *option_value;
  int32            opcode;
  DataStreamState *stream_state;

  stream_state                             = (DataStreamState *) stream_;
  stream_state->subscriber_replies_pending = 0;
  opcode                                   = request_->msg.control.userDefinedOpcode;
  option_value                             = (uint8 *) request_->msg.control.userDefinedArgPtr;

  switch(opcode)
    {
      case K_SET_FIRST_INTERNAL_STREAM_OPTION:
        if(*option_value != 0)
          {
            stream_->streamFlags &= ~K_FIRST_INTERNAL_STREAM_OPTION_DISABLED;
          }
        else
          {
            stream_->streamFlags |= K_FIRST_INTERNAL_STREAM_OPTION_DISABLED;
          }
        break;

      case K_SET_SECOND_INTERNAL_STREAM_OPTION:
        if(*option_value != 0)
          {
            stream_->streamFlags &= ~K_SECOND_INTERNAL_STREAM_OPTION_DISABLED;
          }
        else
          {
            stream_->streamFlags |= K_SECOND_INTERNAL_STREAM_OPTION_DISABLED;
          }
        break;

      case K_GET_FIRST_INTERNAL_STREAM_OPTION:
        *option_value = (stream_->streamFlags & K_FIRST_INTERNAL_STREAM_OPTION_DISABLED) == 0;
        break;

      case K_GET_SECOND_INTERNAL_STREAM_OPTION:
        *option_value = (stream_->streamFlags & K_SECOND_INTERNAL_STREAM_OPTION_DISABLED) == 0;
        break;
    }
  return 0;
}
