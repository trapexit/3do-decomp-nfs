/* Frontovl I/O completion entry point. */

#include "runtime_io.h"

#include "kernel_item.h"
#include "wait_for_kernel_port.h"

#include "io.h"
#include "kernelnodes.h"
#include "msgport.h"
#include "operror.h"
#include "task.h"

int32
check_runtime_io_completion(Item request_)
{
  IOReq *io_request;

  /*
   * The retail overlay implements this check directly.  Its private CheckIO
   * entry point looks up the IOReq and tests IO_DONE; the SDK library symbol
   * linked by this reconstruction rejects these legacy overlay item values.
   */
  io_request = (IOReq *) lookup_kernel_item(request_);
  if(io_request == NULL)
    return BADITEM;

  return (io_request->io_Flags & IO_DONE) != 0;
}

int32
wait_for_io_completion(Item request_)
{
  Message *completion_message;
  IOReq   *io_request;
  Item     received_message;
  int32    is_complete;

  io_request = (IOReq *) lookup_kernel_item(request_);
  if(io_request == NULL)
    return BADITEM;

  if((io_request->io_Flags & IO_QUICK) != 0)
    return 0;

  completion_message = (Message *) lookup_kernel_item(io_request->io_MsgItem);
  if(completion_message->msg.n_Type == MESSAGENODE)
    {
      is_complete = check_runtime_io_completion(request_);
      if((is_complete == 0) ||
         (completion_message->msg_ReplyPort == completion_message->msg_MsgPort))
        {
          received_message =
            wait_for_kernel_port(completion_message->msg_ReplyPort, completion_message->msg.n_Item);
          if(received_message < 0)
            return received_message;
        }

      return 0;
    }

  while(check_runtime_io_completion(request_) == 0)
    WaitSignal(SIGF_IODONE);

  return 0;
}
