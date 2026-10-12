#include "legacy_io_wait.h"

#include "io.h"
#include "item.h"
#include "kernel.h"
#include "msgport.h"
#include "task.h"

#define LEGACY_BAD_ITEM_ERROR ((Err) 0xD57B9001)

static int32
_is_io_complete(Item io_request_)
{
  IOReq *io_request;

  io_request = (IOReq *) LookupItem(io_request_);
  if(io_request == NULL)
    return LEGACY_BAD_ITEM_ERROR;
  return (int32) (io_request->io_Flags & IO_DONE);
}

Err
wait_for_io_completion(Item io_request_)
{
  IOReq   *io_request;
  Message *message;
  int32    complete;

  io_request = (IOReq *) LookupItem(io_request_);
  if(io_request == NULL)
    return LEGACY_BAD_ITEM_ERROR;
  if((io_request->io_Flags & IO_QUICK) != 0)
    return 0;

  message = (Message *) LookupItem(io_request->io_MsgItem);
  if(message == NULL)
    return LEGACY_BAD_ITEM_ERROR;
  if(message->msg.n_Type == MESSAGENODE)
    {
      complete = _is_io_complete(io_request_);
      if((complete == 0) || (message->msg_ReplyPort != message->msg_MsgPort))
        {
          if(WaitPort(message->msg_ReplyPort, io_request->io_MsgItem) < 0)
            return -1;
        }
      return 0;
    }

  while(_is_io_complete(io_request_) == 0)
    (void) WaitSignal(SIGF_IODONE);
  return 0;
}
